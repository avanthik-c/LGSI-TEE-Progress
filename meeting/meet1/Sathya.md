# RSA Implementation in OP-TEE

## Objective
To implement the RSA cryptographic algorithm inside the Trusted Execution Environment (TEE) using OP-TEE and QEMU, ensuring that encryption and decryption operations are executed securely within the Secure World.

## Introduction
RSA is an asymmetric cryptographic algorithm that uses two keys:

- **Public Key** – Used for encryption
- **Private Key** – Used for decryption

In this implementation, RSA operations are performed inside the Trusted Application (TA), while the Host Application (CA) runs in the Normal World and communicates with the TA using the TEE Client API.

---

## OP-TEE Architecture

### Host Application (Normal World)

The Host Application is responsible for:

- Creating a TEE Context
- Opening a Session with the Trusted Application
- Taking user input
- Invoking commands in the Trusted Application
- Displaying the results

### Trusted Application (Secure World)

The Trusted Application is responsible for:

- Generating RSA key pairs
- Performing RSA encryption
- Performing RSA decryption
- Returning results securely to the Host Application

---

## Code Flow

### Step 1: Initialize TEE Context

The Host Application establishes communication with OP-TEE.

```c
TEEC_InitializeContext(NULL, &ctx);
```

**Purpose:**
- Connects the Client Application to the TEE.

### Step 2: Open Session

```c
TEEC_OpenSession(
    &ctx,
    &sess,
    &uuid,
    TEEC_LOGIN_PUBLIC,
    NULL,
    NULL,
    NULL);
```

**Purpose:**
- Opens a secure session with the Trusted Application.

### Step 3: Display Menu

The Host Application displays a menu:

1. Generate RSA Key Pair
2. Encrypt Message
3. Decrypt Message
4. Exit

The user selects an operation.

### Step 4: Invoke Trusted Application

```c
TEEC_InvokeCommand(
    &sess,
    CMD_RSA_GENERATE,
    &op,
    &err_origin);
```

**Purpose:**
- Sends commands from the Client Application to the Trusted Application.

### Step 5: RSA Key Generation in TA

Inside the Trusted Application:

```c
TEE_AllocateTransientObject(
    TEE_TYPE_RSA_KEYPAIR,
    1024,
    &rsa_keypair);
```

Generate the key pair:

```c
TEE_GenerateKey(
    rsa_keypair,
    1024,
    NULL,
    0);
```

**Purpose:**
- Generates RSA public and private keys securely inside the TEE.

### Step 6: RSA Encryption

The plaintext received from the Host Application is encrypted using the public key.

```c
TEE_AsymmetricEncrypt(
    operation,
    NULL,
    0,
    plaintext,
    plaintext_len,
    ciphertext,
    &ciphertext_len);
```

**Output:**
- Ciphertext generated securely inside the Trusted Application.

### Step 7: RSA Decryption

Ciphertext is sent back to the Trusted Application.

```c
TEE_AsymmetricDecrypt(
    operation,
    NULL,
    0,
    ciphertext,
    ciphertext_len,
    plaintext,
    &plaintext_len);
```

**Output:**
- Original plaintext recovered securely.

### Step 8: Return Result

The encrypted or decrypted data is returned to the Host Application through shared memory buffers.

### Step 9: Close Session

```c
TEEC_CloseSession(&sess);
TEEC_FinalizeContext(&ctx);
```

**Purpose:**
- Closes the secure session and releases allocated resources.


---

## Security Advantages of OP-TEE

- Private key never leaves the Secure World.
- RSA operations execute entirely inside the Trusted Application.
- Sensitive data is protected from Normal World applications.
- Secure communication is maintained between the Client Application and Trusted Application.

---

## Project Output

The implementation successfully demonstrated:

- RSA Key Generation
- RSA Encryption
- RSA Decryption
- Secure execution inside OP-TEE running on QEMU



---

## Conclusion

RSA was successfully implemented inside OP-TEE using the Client Application and Trusted Application architecture. The Host Application handled user interaction, while all cryptographic operations were executed securely inside the Trusted Application. This implementation provided a secure foundation for exploring Post-Quantum Cryptography algorithms such as ML-KEM within the Trusted Execution Environment.
