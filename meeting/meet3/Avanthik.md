```bash
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$ ls
core  include  mbedtls  mlkem  sub.mk
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$ tree mlkem
mlkem
└── sample.c

1 directory, 1 file
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$ cat mlkem/sample.c
#include <mlkem/sample.h>
int simple_add(int a,int b){
        return a+b;
}
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$
```
add line 
`srcs-y += mlkem/sample.c #change made` in sub.mk
```bash
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$ ls
aes_alt.h  mbedtls_config_kernel.h  mbedtls_config_uta.h  mlkem
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$ tree mlkem
mlkem
└── sample.h

1 directory, 1 file
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$ cat mlkem/sample.h
#ifndef SAMPLE_H
#define SAMPLE_H

int simple_add(int a,int b);
#endif
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$
```
similarly add the header file under mlkem folder

and make modifications in the ta/hello_world_ta.c
add header `#include<mlkem/sample.h>`
and can call function call like code directly
```c
IMSG("Calling sample.c from mbedtls dir");
int add_res=simple_add(20,30);
```



what we have doen here
when optee os is booted these cryptolib are compiled for the os to use/like for os to perform the functions and give result

but here these same libraries (if given headers in ta) will be complied along with the ta too when its executed
so ta can execute crypto function without accessing kernel or triggering any system call

other method is using the TEE API call TEE_AllocateOperation to call specific crypto functions


### Global APIS

- Sample global api snippet
```c
TEE_Result TA_AES_CBC_Encrypt_Example(
    uint8_t *key_buf,   size_t key_len,    /* e.g., 256-bit key = 32 bytes */
    uint8_t *iv_buf,    size_t iv_len,     /* 16 bytes for AES */
    uint8_t *src_buf,   size_t src_len,    /* Input plaintext (must be multiple of 16 for NOPAD) */
    uint8_t *dest_buf,  size_t *dest_len   /* Output ciphertext buffer */
) {
    TEE_Result res = TEE_SUCCESS;
    TEE_OperationHandle op_handle = TEE_HANDLE_NULL;
    TEE_ObjectHandle key_handle = TEE_HANDLE_NULL;
    TEE_Attribute attr;

    uint32_t key_bit_size = key_len * 8;

    /* 1. Allocate Operation Handle */
    res = TEE_AllocateOperation(&op_handle, TEE_ALG_AES_CBC_NOPAD, TEE_MODE_ENCRYPT, key_bit_size);
    if (res != TEE_SUCCESS) return res;

    /* 2. Allocate Transient Key Object */
    res = TEE_AllocateTransientObject(TEE_TYPE_AES, key_bit_size, &key_handle);
    if (res != TEE_SUCCESS) goto cleanup;

    /* 3. Populate Key Object with raw bytes */
    TEE_InitRefAttribute(&attr, TEE_ATTR_SECRET_VALUE, key_buf, key_len);
    // stores the memaddress of buffer,sizeof buffer,type of data in buffer into attr obj
    //TEE_ATTR_SECRET_VALUE - says that it is aes secret key

    res = TEE_PopulateTransientObject(key_handle, &attr, 1);
    //passes the values to kernelspace memory
    //1 - number of aatribute
    //aes has only 1 ,then encryption key
    //rsa will have 2 ->public key and modulus

    if (res != TEE_SUCCESS) goto cleanup;

    /* 4. Bind Key to Operation */
    res = TEE_SetOperationKey(op_handle, key_handle);
    //links the kernel structs for teh operation and keyspace
    if (res != TEE_SUCCESS) goto cleanup;

    /* 5. Initialize Cipher with IV */
    TEE_CipherInit(op_handle, iv_buf, iv_len);

    /* 6. Perform Encryption (Single-pass via DoFinal, or multi-pass with Update) */
    res = TEE_CipherDoFinal(op_handle, src_buf, src_len, dest_buf, dest_len);

cleanup:
    /* 7. Clean up handles to avoid kernel/user memory leaks */
    if (key_handle != TEE_HANDLE_NULL) {
        TEE_FreeTransientObject(key_handle);
    }
    if (op_handle != TEE_HANDLE_NULL) {
        TEE_FreeOperation(op_handle);
    }

    return res;
}
```

theory time:
on boot the Secure RAM is partitioned into:
- OP-TEE Core Pool (Fixed Region):specific part of ram for kernel space optee
This holds the kernel code, kernel heap, and hardware drivers.
- TA RAM Pool (Fixed Region):the remaining part of secure ram for userspace optee
thi holds the TAs

function flow:

- `TEE_AllocateOperation(&op_handle, TEE_ALG_AES_CBC_NOPAD, TEE_MODE_ENCRYPT, key_bit_size);`
the parameters are in variable stack
once the function called the execution ponter jumps to libutee library
where this function is described
what happens
`struct __TEE_OperationHandle` object allocated using malloc
copies the parameters into this object
then moves 
syscall id to x0 register
algo id to x1 register
maxkey size to x2 register
calls syscall (SVC)
syscall reads the algo id and maxkeysize to allocate appropriate memory in
kernelspace ram
and allocated ram is associated with the particular algorithm
and return a stateid in x0 to ta space ,stored in the struct object
this struct object is then given a pointer `op_handle`(which was null till now)
now for future interaction we need stateID

- `TEE_AllocateTransientObject(TEE_TYPE_AES, key_bit_size, &key_handle);`
function call->libutee mallocs `struct __TEE_ObjectHandle` object
populates data objectType (AES) and maxObjectSize (256).
loads the CPU registers:
x0 = System call ID for utee_cryp_obj_alloc
x1 = TEE_TYPE_AES
x2 = 256 (max size)
libutee triggers the SVC instruction, jumping the boundary from EL0 to EL1.

It reserves a kernel-level structure (typically struct tee_obj) in the kernel's secure heap.

Based on the objectType and maxObjectSize, the kernel allocates enough secure RAM to eventually hold a 256-bit AES key securely. Note: The memory where the key will live is usually initialized to zero at this point.
The kernel generates a unique integer identifier for this empty object (let's call it objectID, 
e.g., ticket #99).

The kernel places objectID  into register x0 and returns to EL0.
libutee takes 99 and saves it inside the struct __TEE_ObjectHandle on your TA's heap.
Finally, libutee sets your TA's key_handle pointer to point to this heap struct.

- `TEE_PopulateTransientObject(key_handle, &attr, 1);`->libutee
then loads values intot register
x0 = System Call ID 
x1 = objectID (#99)
x2 = Pointer to attr (address on TA stack)
x3 = attrCount (1)
libutee executes the SVC instruction to switch control to EL1.
The kernel uses a safe copy routine (like copy_from_user()) to read the raw key bytes directly from the TA's user-space buffer key_buf.
It writes those key bytes into the protected secure memory buffer inside struct tee_obj in kernel RAM.


- `TEE_SetOperationKey(op_handle, key_handle);`
libutee
loads register
x0 = System Call ID for utee_cryp_state_set_key
x1 = stateID (#42)
x2 = objectID (#99)
libutee triggers the SVC instruction to switch to EL1.

loads the key to the crypto operation ie
E(k,pt) becomes Ek(pt)
aldready incorporate the key to the function

- `TEE_CipherInit(op_handle, iv_buf, iv_len);`
loads the IV to the algo
User Space (libutee): The TA invokes the function. libutee sets up the SVC call (utee_cryp_cipher_init), passing stateID (#42) and the user-space address of iv_buf.
Kernel Boundary: The OP-TEE kernel intercepts the SVC. It looks up struct tee_cryp_state using #42.
Data Ingestion: The kernel safely copies the 16 bytes from iv_buf into a secure kernel-space buffer.

- `TEE_CipherDoFinal(op_handle, src_buf, src_len, dest_buf, dest_len);`

pass the pt to the algo and get the result in dest_buffer
cleanup in kernelspace

libutee
then load register
x0	op_handle	The ID number of your engine (#42).
x1	srcData	RAM address of the final plaintext chunk (or NULL if 0 bytes).
x2	srcLen	Length of final plaintext chunk (e.g., 5 bytes or 0).
x3	destData	RAM address of output buffer reserved for final ciphertext + padding.
x4	&destLen	RAM address where kernel writes the actual final bytes written.
x8	Syscall ID	The syscall number for UTEE_CRYP_CIPHER_DO_FINAL (e.g., 0x3B).
call syscall

- Destroy the Cryptographic Engine (operation struct)
`TEE_FreeOperation(op_handle);`

- Destroy the Secret Key Container (object struct)
`TEE_FreeTransientObject(key_handle);`