# Menu-Driven RSA Algorithm in C

## Source Code

```c
#include <stdio.h>
#include <math.h>

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int modInverse(int e, int phi) {
    for (int d = 1; d < phi; d++) {
        if ((e * d) % phi == 1)
            return d;
    }
    return -1;
}

long long modExp(long long base, long long exp, long long mod) {
    long long result = 1;
    while (exp > 0) {
        result = (result * base) % mod;
        exp--;
    }
    return result;
}

int main() {
    int p, q, n, phi, e, d;
    long long message, cipher, decrypted;
    int choice;
    int keysGenerated = 0;

    while (1) {
        printf("\n===== RSA MENU =====\n");
        printf("1. Generate Keys\n");
        printf("2. Encrypt Message\n");
        printf("3. Decrypt Ciphertext\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {

        case 1:
            printf("Enter prime number p: ");
            scanf("%d", &p);

            printf("Enter prime number q: ");
            scanf("%d", &q);

            n = p * q;
            phi = (p - 1) * (q - 1);

            for (e = 2; e < phi; e++) {
                if (gcd(e, phi) == 1)
                    break;
            }

            d = modInverse(e, phi);

            printf("\nPublic Key (e, n) = (%d, %d)\n", e, n);
            printf("Private Key (d, n) = (%d, %d)\n", d, n);

            keysGenerated = 1;
            break;

        case 2:
            if (!keysGenerated) {
                printf("Generate keys first!\n");
                break;
            }

            printf("Enter message (number less than %d): ", n);
            scanf("%lld", &message);

            cipher = modExp(message, e, n);

            printf("Encrypted Ciphertext = %lld\n", cipher);
            break;

        case 3:
            if (!keysGenerated) {
                printf("Generate keys first!\n");
                break;
            }

            printf("Enter ciphertext: ");
            scanf("%lld", &cipher);

            decrypted = modExp(cipher, d, n);

            printf("Decrypted Message = %lld\n", decrypted);
            break;

        case 4:
            printf("Exiting...\n");
            return 0;

        default:
            printf("Invalid choice!\n");
        }
    }

    return 0;
}
```

## Compilation

```bash
gcc rsa.c -o rsa
./rsa
```

## Sample Execution

```text
===== RSA MENU =====
1. Generate Keys
2. Encrypt Message
3. Decrypt Ciphertext
4. Exit

Enter your choice: 1

Enter prime number p: 3
Enter prime number q: 11

Public Key (e, n) = (3, 33)
Private Key (d, n) = (7, 33)

Enter your choice: 2
Enter message (number less than 33): 5

Encrypted Ciphertext = 26

Enter your choice: 3
Enter ciphertext: 26

Decrypted Message = 5
```

## RSA Formula

- \( n = p \times q \)
- \( \phi(n) = (p-1)(q-1) \)
- Choose \( e \) such that \( gcd(e, \phi(n)) = 1 \)
- Find \( d \) such that:

```text
(e × d) mod φ(n) = 1
```

### Encryption

```text
C = M^e mod n
```

### Decryption

```text
M = C^d mod n
```