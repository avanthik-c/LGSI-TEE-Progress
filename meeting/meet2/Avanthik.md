*** I have tried to add a sample program as part of the OPTEE kernel which is called using Psuedo Trusted Application ***

## Function

```bash
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/core/crypto$ tree
.
├── aes-cts.c
├── aes-gcm-ghash-tbl.c
├── aes-gcm-sw.c
├── aes-gcm.c
├── cbc-mac.c
├── crypto.c
├── rng_fortuna.c
├── rng_hw.c
├── signed_hdr.c
├── sm2-kdf.c
├── sm3-hash.c
├── sm3-hmac.c
├── sm3.c
├── sm3.h
├── sm4-cbc.c
├── sm4-ctr.c
├── sm4-ecb.c
├── sm4-xts.c
├── sm4.c
├── sm4.h
├── sm4_accel.c
└── sub.mk

1 directory, 22 files
```
- make the custom function file:
` micro sample.c `
### Code
- add contents
```c
// sample.c
#include <trace.h>

int custom_add(int a, int b) {
    IMSG("custom crypto called! calculating %d + %d", a, b);
    return a + b;
}
```
### Header file

- header file `/optee-qemu/optee_os/core/crypto/sample.h`
```c
#ifndef SAMPLE_H
#define SAMPLE_H

// The prototype that tells the PTA what the function looks like
int custom_add(int a, int b);

#endif
```
### Compile
- add in sub.mk `/optee-qemu/optee_os/core/crypto/sub.mk`
`srcs-y += sample.c`
srcs tells the build system this is a source file.
-y means "yes, compile this file unconditionally."
+= appends your file to the existing list of files OP-TEE is already building in that folder.

## PTA

### Code
create pseudo TA at `~/optee-qemu/optee_os/core/pta/my_crypto_pta.c`

```c
#include <kernel/pseudo_ta.h>
#include <trace.h>
#include <crypto/sample.h> // Include your custom crypto header

// Define the Unique ID for this service
#define PTA_MY_CRYPTO_UUID \
    { 0x12345678, 0x1234, 0x1234, \
        { 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef } }

// Define the Command ID (0 is the index for custom_add)
#define PTA_CMD_ADD_NUMBERS 0

static TEE_Result invoke_command(void *session __maybe_unused,uint32_t cmd_id,uint32_t param_types,TEE_Param params[TEE_NUM_PARAMS])
{
    // 1. Verify the parameters: Input Value, Output Value, None, None
    uint32_t expected_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,TEE_PARAM_TYPE_VALUE_OUTPUT,TEE_PARAM_TYPE_NONE,TEE_PARAM_TYPE_NONE);

    if (param_types != expected_types) {
        EMSG("PTA: Invalid parameter types provided by TA");
        return TEE_ERROR_BAD_PARAMETERS;
    }

    // 2. Route the command ID to the correct function
    switch (cmd_id) {
        case PTA_CMD_ADD_NUMBERS:
            // Extract inputs from the TA's parameter array
            int a = params[0].value.a;
            int b = params[0].value.b;

            // Execute your kernel-level logic
            int result = custom_add(a, b);

            // Write result back to the TA's output parameter slot
            params[1].value.a = result;
            
            DMSG("PTA: Successfully computed %d + %d = %d", a, b, result);
            return TEE_SUCCESS;

        default:
            EMSG("PTA: Command ID %d not supported", cmd_id);
            return TEE_ERROR_NOT_SUPPORTED;
    }
}

// 3. Register the PTA with the Kernel
// This macro is the "birth certificate" for your PTA.
pseudo_ta_register(.uuid = PTA_MY_CRYPTO_UUID, 
                   .name = "my_crypto_pta",
                   .flags = PTA_DEFAULT_FLAGS,
                   .invoke_command_entry_point = invoke_command);//mentioning which function to run on uuid accessing

// kernel has a routing table which matches uuod with respective files for easier searching
// this is used to do that while compiling 
```
### Compile
append below code in `/optee-qemu/optee_os/core/pta/sub.mk`
```c

srcs-y += my_crypto_pta.c
```

## CA

- create the CA `optee-qemu/optee_examples/hello_world/host`
```c
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <tee_client_api.h>
#include <hello_world_ta.h>

int main(void)
{
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
    uint32_t err_origin;
    TEEC_Result res;

    printf("Initialize OP-TEE context...\n");
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

    printf("Open session to TA...\n");
    res = TEEC_OpenSession(&ctx, &sess, &uuid,TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_Opensession failed");

    // Clear the operation structure
    memset(&op, 0, sizeof(op));
    
    // Tell the TA we are sending one input value and expecting one output value back
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_VALUE_OUTPUT,TEEC_NONE, TEEC_NONE);
    
    // Load our payload
    op.params[0].value.a = 42;  // Integer A
    op.params[0].value.b = 100; // Integer B

    printf("Sending A=%d and B=%d into the Secure World...\n", op.params[0].value.a, op.params[0].value.b);

    // Send the payload
    res = TEEC_InvokeCommand(&sess, TA_HELLO_WORLD_CMD_INC_VALUE, &op,&err_origin);
    if (res != TEEC_SUCCESS)
        errx(1, "TEEC_InvokeCommand failed with code 0x%x", res);

    // Print the result that travelled all the way back from sample.c!
    printf("SUCCESS! Result received from Secure Kernel: %d\n", op.params[1].value.a);

    // Clean up
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);

    return 0;
}
```
## TA

- create the TA `optee-qemu/optee_examples/hello_world/ta`
```c
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <hello_world_ta.h>

// 1. Define the exact UUID and Command ID of our Kernel PTA
#define PTA_MY_CRYPTO_UUID \
    { 0x12345678, 0x1234, 0x1234, \
        { 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef } }

#define PTA_CMD_ADD_NUMBERS 0

// 2. The entry point when the CA sends a message
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,uint32_t cmd_id,uint32_t param_types, TEE_Param params[4])
{
    // Check if the CA is asking for our specific command
    if (cmd_id == TA_HELLO_WORLD_CMD_INC_VALUE) {
        TEE_Result res;
        TEE_TASessionHandle pta_session;
        uint32_t ret_origin;
        TEE_UUID pta_uuid = PTA_MY_CRYPTO_UUID;

        DMSG("TA: Received request from CA, opening session to Kernel PTA...");

        // Open a session to the Kernel PTA
        res = TEE_OpenTASession(&pta_uuid, TEE_TIMEOUT_INFINITE, 0, NULL, &pta_session, &ret_origin);
        if (res != TEE_SUCCESS) {
            EMSG("TA: Failed to open PTA session");
            return res;
        }

        // Forward the exact parameters (A and B) to the Kernel PTA
        res = TEE_InvokeTACommand(pta_session, TEE_TIMEOUT_INFINITE, PTA_CMD_ADD_NUMBERS,param_types, params, &ret_origin);

        // Close the connection to the kernel
        TEE_CloseTASession(pta_session);
        
        DMSG("TA: Kernel returned success, sending result back to CA.");
        return res;
    }
    
    return TEE_ERROR_BAD_PARAMETERS;
}
TEE_Result TA_CreateEntryPoint(void) {
    return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void) {
}

TEE_Result TA_OpenSessionEntryPoint(...) {
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(...) {
}
```
high level execution flow
CA -> TA -> PTA -> kernel function
questions that come to mind: why can CA access TA but TA cant access custom_add() directly ,instead need PTA?
Answer:
Priveledge levels
EL0:user space
EL1:kernel space

CA and TA are user space applications so there isnt an issue
but still due to security EL0(CA) -> EL1(linux) -> EL3(Secure monitor) -> EL1(OPTEE) -> EL0(TA)

custom_add lies in kernel code hence EL0(TA) cant access memory pointing to EL1(OPTEE kernel) directly hence use PTA

To prevent a bad TA from crashing the secure kernel or stealing data, the hardware strictly isolates EL0 from EL1. A user-space application (TA) cannot directly execute a memory address that belongs to the kernel. If it tries, the CPU hardware will literally block the execution and trigger a fatal "Segmentation Fault," killing the TA instantly.

EL0(TA) issues harware interrupt to execute kernel code
TEE_InvokeTACommand is the trigger for that interrupt
