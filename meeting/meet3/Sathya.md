# NIST Standards Verification Report for the Algorithms Used in mlkem-native and PQClean

---

## 1. Purpose of the Report

### 1.1 Objective

This report is prepared to verify whether the post-quantum cryptographic algorithms implemented by the `mlkem-native` and `PQClean` libraries are standardized by the National Institute of Standards and Technology (NIST).

The objective of this assessment is to:

* Identify the cryptographic algorithms implemented by the selected libraries.
* Verify whether these algorithms are included in the NIST Post-Quantum Cryptography standards.
* Confirm that the algorithms correspond to the specifications defined in NIST FIPS 203 (Module-Lattice-Based Key-Encapsulation Mechanism – ML-KEM).
* Document the verification results for the algorithms used in this project.

---

## 2. NIST Standard Information

| Specification             | Details                                                   |
| ------------------------- | --------------------------------------------------------- |
| Standard Name             | NIST FIPS 203                                             |
| Standard Title            | Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM) |
| Standardized Algorithm    | ML-KEM                                                    |
| Parameter Set Verified    | ML-KEM-512                                                |
| Libraries Evaluated       | mlkem-native, PQClean                                     |
| Standardization Authority | National Institute of Standards and Technology (NIST)     |
| Verification Basis        | Algorithm standardized in NIST FIPS 203                   |
| Publication Date          | August 13, 2024                                           |

---

## 3. Verification of Algorithms Implemented by the Libraries

### 3.1 mlkem-native

The `mlkem-native` library was examined to identify the post-quantum cryptographic algorithm used in its implementation. Documentation and implementation details indicate that the library implements the Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM) as specified in NIST FIPS 203.

For this project, the ML-KEM-512 parameter set was evaluated. Verification confirmed that the algorithm implemented by `mlkem-native` corresponds to the standardized ML-KEM algorithm published by the National Institute of Standards and Technology (NIST). Therefore, the algorithm used by the library is a NIST-standardized post-quantum cryptographic algorithm.

### 3.2 PQClean

The `PQClean` library was similarly examined to identify the cryptographic algorithm used in its implementation. The library provides an implementation of the Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM) in accordance with the NIST FIPS 203 specification.

The implementation evaluated in this project uses the ML-KEM-512 parameter set. Verification confirmed that the algorithm implemented by `PQClean` corresponds to the standardized ML-KEM algorithm defined by NIST. Therefore, the algorithm used by the library is a NIST-standardized post-quantum cryptographic algorithm.

---

## 4. Verification Methodology

The verification process consisted of the following activities:

1. Reviewing the official NIST FIPS 203 specification for the Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM).
2. Identifying the cryptographic algorithms implemented by the `mlkem-native` and `PQClean` libraries.
3. Verifying that the implemented algorithm corresponds to the standardized ML-KEM algorithm defined in FIPS 203.
4. Comparing the supported parameter set (ML-KEM-512) with the parameter set defined in the NIST standard.
5. Confirming that the algorithm implemented by each library is the standardized version published by NIST.

---

## 5. Algorithm Verification Summary

The verification process confirmed that the cryptographic algorithms implemented by the `mlkem-native` and `PQClean` libraries correspond to the Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM) standardized by the National Institute of Standards and Technology (NIST) in FIPS 203.

Both libraries provide implementations of the ML-KEM algorithm and support the standardized parameter sets defined by NIST. For the purposes of this project, the ML-KEM-512 parameter set was evaluated.

The assessment established that the algorithms implemented by both libraries correspond to the ML-KEM specification defined in NIST FIPS 203. Consequently, the cryptographic algorithms used in the selected libraries are NIST-standardized post-quantum cryptographic algorithms.

---

## 6. Findings

The verification produced the following findings:

* The `mlkem-native` library implements the ML-KEM algorithm standardized by NIST in FIPS 203.
* The `PQClean` library implements the ML-KEM algorithm standardized by NIST in FIPS 203.
* Both libraries support the ML-KEM-512 parameter set evaluated in this project.
* The cryptographic algorithms implemented by both libraries correspond to the standardized Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM).
* The algorithms used by the selected libraries are included in the NIST Post-Quantum Cryptography standards.

---

## 7. Implementation Validation Using ACVP and Wycheproof Test Suites

To verify that the ML-KEM implementation used in this project conforms to the NIST FIPS 203 specification, implementation-level validation was performed using the built-in ACVP (Automated Cryptographic Validation Protocol) and Wycheproof test suites provided by the `mlkem-native` validation framework.

Implementation-level validation was performed only for the ML-KEM implementation used in this project (`mlkem-native`). The `PQClean` library was evaluated only for algorithm-level compliance with the NIST FIPS 203 specification and was not subjected to the ACVP and Wycheproof validation suites as part of this work.

The ML-KEM implementation developed and used in this project consists of the following implementation files:

* `mlkem_native.h`
* `mlkem_native_config.h`
* `compress.c`
* `indcpa.c`
* `kem.c`
* `poly.c`
* `sampling.c`

These files were integrated into the original `mlkem-native` repository by replacing the corresponding implementation files. This ensured that the ACVP and Wycheproof validation frameworks executed and verified the project's implementation rather than the original repository implementation.

After integrating the implementation files, the following validation commands were executed:

```bash
./scripts/tests acvp
./scripts/tests wycheproof
```

The ACVP test suite validates the correctness of the ML-KEM implementation against the official ML-KEM test vectors (version `v1.1.0.41`) defined for NIST FIPS 203.

The Wycheproof test suite validates the robustness of the implementation by testing malformed and edge-case inputs.

Both validation suites automatically verify the optimized and non-optimized builds of the implementation.

The implementation successfully passed all validation tests, producing the terminal output:

```text
All good!
```

for both ACVP and Wycheproof test suites.

The successful execution of the ACVP and Wycheproof validation suites confirms that the ML-KEM implementation used in this project conforms to the requirements specified in the NIST FIPS 203 standard for the Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM).

---

## 8. Conclusion

Based on the verification performed against the NIST FIPS 203 standard, the cryptographic algorithms implemented by both the `mlkem-native` and `PQClean` libraries correspond to the standardized Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM) specified by the National Institute of Standards and Technology (NIST).

The evaluation confirmed that both libraries implement the ML-KEM-512 algorithm, which is one of the official parameter sets standardized in NIST FIPS 203. Therefore, the cryptographic algorithms used by these libraries are NIST-standardized post-quantum cryptographic algorithms.

This report verifies the NIST standardization status of the cryptographic algorithms implemented by the selected libraries and does not constitute an evaluation or certification of the software libraries themselves.

Furthermore, implementation-level validation performed using the ACVP and Wycheproof test suites confirms that the ML-KEM implementation used in this project (`mlkem-native`) conforms to the NIST FIPS 203 specification. The `PQClean` library was verified at the algorithm level by confirming that it implements the standardized ML-KEM algorithm defined in NIST FIPS 203.

---

## References

1. National Institute of Standards and Technology (NIST), *FIPS 203: Module-Lattice-Based Key-Encapsulation Mechanism (ML-KEM)*, August 13, 2024.

   Available at: https://nvlpubs.nist.gov/nistpubs/fips/nist.fips.203.pdf

2. *mlkem-native* – Official GitHub repository and documentation.

   Available at: https://github.com/pq-code-package/mlkem-native

3. *PQClean* – Official GitHub repository and documentation.

   Available at: https://github.com/PQClean/PQClean
