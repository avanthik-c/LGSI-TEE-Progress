# ML-KEM-512 Keypair Generation (PQClean)

This document contains the C code to generate a post-quantum cryptographic keypair using the PQClean implementation of **ML-KEM-512** (formerly Kyber-512), along with a step-by-step breakdown of how it works.

## 1. Complete C Code

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "api.h"

/*
 * 1. CRYPTOGRAPHIC RNG (Random Number Generator)
 * Post-quantum cryptography algorithms heavily rely on secure randomness.
 * The PQClean framework expects the host to provide a cryptographically 
 * secure pseudorandom number generator (CSPRNG). Here, we implement it by 
 * reading directly from '/dev/urandom' (a standard Unix/Linux secure interface).
 */
// The function must be namespaced with PQCLEAN_
int PQCLEAN_randombytes(uint8_t *buf, size_t n) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return -1;
    fread(buf, 1, n, f);
    fclose(f);
    return 0;
}

/*
 * 2. HEX DUMP UTILITY
 * Cryptographic keys are raw binary arrays, making them unreadable as text. 
 * This utility iterates through the buffer and prints each byte as a two-character 
 * uppercase hexadecimal string to make the keys human-readable for debugging.
 */
void print_hex(const char *label, const uint8_t *buf, size_t len) {
    printf("\n=== %s (%zu bytes) ===\n", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n"); // Wrap every 16 bytes
    }
    printf("\n");
}

int main() {
    /*
     * 3. KEY BUFFERS
     * We allocate two byte arrays to hold the Public Key (PK) and Secret Key (SK). 
     * The sizes are strictly defined by macros inside PQClean's `api.h`:
     * - Public Key size: 800 bytes
     * - Secret Key size: 1632 bytes
     */
    uint8_t pk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES];

    /*
     * 4. KEYPAIR GENERATION
     * This invokes the ML-KEM algorithm to generate the mathematically linked 
     * public and secret keypair. It automatically calls our `PQCLEAN_randombytes` 
     * function under the hood to seed the process.
     */
    int result = PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);

    /*
     * 5. OUTPUT AND VERIFICATION
     * We print the sizes, do a quick inline sanity check of the first 8 bytes, 
     * and then perform a full memory dump of both keys using `print_hex`.
     */
    if (result == 0) {
        printf("Keypair generated.\n");
        printf("Public Key size: %d bytes\n", PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES);
        printf("Secret Key size: %d bytes\n", PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES);
        
        printf("First 8 bytes of PK: ");
        for(int i = 0; i < 8; i++) {
            printf("%02X ", pk[i]);
        }
        printf("\n");
    } else {
        printf("Key generation failed with error code: %d\n", result);
    }
    
    // Dump the full raw cryptographic material
    print_hex("PUBLIC KEY (PK)", pk, sizeof(pk));
    print_hex("SECRET KEY (SK)", sk, sizeof(sk));
    
    return 0;
}
