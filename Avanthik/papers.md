OP-TEE + ML-KEM-512 Literature Summary

1. TEE Fundamentals

Paper: Understanding the Prevailing Security Vulnerabilities in TrustZone-assisted TEE Systems (SoK)

Authors: Cerdeira et al.

Year: 2020

Venue: IEEE Symposium on Security and Privacy (S&P)

Relevance: This paper establishes the core concepts of TEEs. It explains that TEEs rely on hardware isolation to protect the confidentiality and integrity of Trusted Applications (TAs) against a compromised Rich Execution Environment (Normal World OS).

The Problem: It introduces the threat model where the Normal OS is entirely compromised by an attacker attempting to access sensitive assets inside the Secure World.

Security Model: The security model assumes the TEE kernel and hardware are fundamentally secure and that transitions between worlds are safely mediated.

Connection to ML-KEM: If your goal is to protect the ML-KEM-512 secret key from a compromised Linux OS, this paper establishes exactly why the key generation and encapsulation must occur inside the Secure World, isolated from the untrusted OS.

2. ARM TrustZone

Paper: ARM Security Technology - Building a Secure System using TrustZone Technology

Authors: ARM Technical Documentation

Year: 2009 (Updated regularly)

Venue: Official ARM Documentation

Relevance: Explains the NS (Non-Secure) bit, the Secure Monitor Call (SMC), and the TrustZone Address Space Controller (TZASC).

The Architecture: It details how the CPU context switches from the Normal World (EL1/EL0) to the Secure Monitor (EL3) and finally into the Secure World (S-EL1/S-EL0).

Connection to ML-KEM: When a Linux application requests ML-KEM key generation, it must trigger an SMC. This document explains the hardware context switch required to securely transfer execution to OP-TEE without exposing the CPU state to the Linux OS.

3. OP-TEE Architecture

Paper: OP-TEE Official Documentation - Architecture overview

Authors: Linaro / TrustedFirmware.org

Year: 2021

Venue: TrustedFirmware.org

Relevance: Details the OP-TEE Core, TEE Client API, tee-supplicant, and how shared memory is passed between the Normal and Secure worlds.

The Execution Path:

Normal World App -> TEE Client API -> OP-TEE Linux Driver -> SMC -> Secure Monitor -> OP-TEE Core -> TA.

Connection to ML-KEM: If you insert ML-KEM-512 into OP-TEE, the public key generated must be passed back to the Normal World via OP-TEE's shared memory framework, while the secret key must be stored in OP-TEE's secure storage. Understanding this flow is vital to ensure no secret data leaks into the shared memory buffers.


-specifics
4. OP-TEE Cryptographic Framework
https://www.researchgate.net/publication/387414714_Key-Encapsulation_Mechanisms_embedded_in_Trusted_Execution_Environment_An_Evaluation

Paper: Key-Encapsulation Mechanisms Embedded in Trusted Execution Environment: An Evaluation

The hardware is:

Board: HiKey 960
SoC: Huawei Kirin 960
CPU architecture: ARMv8
Security technology: ARM TrustZone
RAM: 3 GB LPDDR4
TEE: OP-TEE v3.19
REE: Normal/Rich Execution Environment
Execution: physical hardware, not just QEMU/emulation.


KEM	Variants evaluated	Approach
Kyber	Kyber-512, Kyber-768, Kyber-1024	Lattice-based
BIKE	BIKE-64, BIKE-96, BIKE-128	Code-based
HQC	HQC-128, HQC-196, HQC-256	Code-based
Classic McEliece	348864, 348864f, 460896, 460896f, 6688128, 6688128f, 6960119, 6960119f, 8192128, 8192128f	Code-based

For each algorithm, they measured the three fundamental KEM operations:

Key generation
Encapsulation
Decapsulation

The primary metric was average CPU clock ticks while executing in the Secure World (TEE)

They separately measured the time overhead introduced by communication between the Normal World (REE) and Secure World (TEE) for:

KeyGen
Encapsulation
Decapsulation

They also measured the memory requirements associated with the KEM's:

Private key
Public key
Ciphertext

The paper reports these sizes in bits.

Specifically, they did not measure:

ML-KEM/Kyber secret-key leakage
Secret-key extraction
Shared-memory attacks
DMA attacks
Cache side channels
Timing attacks against secret-dependent operations
Fault injection
OP-TEE memory corruption
TA isolation failures
Normal World → Secure World attacks


Authors: Andrade et al.

Year: 2024

Venue: IEEE

Relevance: This is highly relevant because it directly evaluates NIST PQC KEMs (including Kyber) embedded inside OP-TEE on ARM TrustZone.

What Happens During Crypto Addition: The paper explains the integration of Kyber into OP-TEE's TEE Internal Core API, assessing the memory footprint, CPU overhead, and execution time inside the Trusted Application.

Connection to ML-KEM: It demonstrates that Kyber-512 is highly efficient but requires careful memory management. When generating a key, you must rely on OP-TEE's hardware-backed RNG for seed generation, execute the matrix operations inside the TA, and utilize the Internal API to manage the key object lifecycle.

-show full compromise in rasberry pi
5. OP-TEE Attack Surface

Paper: Understanding the Prevailing Security Vulnerabilities in TrustZone-assisted TEE Systems (SoK)

Authors: Cerdeira et al.

Year: 2020

Relevance: Evaluates architectural deficiencies, specifically shared memory attacks and TA vulnerabilities (buffer overflows, unvalidated pointers).

Attack Mechanism: An attacker in the compromised Normal World crafts malformed TEE Client API parameters. If the OP-TEE kernel or TA fails to validate these pointers, the attacker can force the Secure World to overwrite its own memory (memory corruption) or leak data.

Connection to ML-KEM:

Compromised Normal World -> Malicious TEE Invocation -> Malformed Parameters -> OP-TEE Shared Memory Vulnerability -> Secure World Memory Disclosure -> ML-KEM Secret Key Exposed.

The paper shows this attack chain is highly viable if input validation is poorly implemented.

6. Raspberry Pi 3 / 3B+ Security

Paper: MyTEE: Own the Trusted Execution Environment on Embedded Devices
https://www.ndss-symposium.org/wp-content/uploads/2023/02/ndss2023_s41_paper.pdf

Authors: Lee et al.

Year: 2020

Venue: NDSS

Relevance: This paper explicitly addresses the hardware limitations of deploying TEEs on the Raspberry Pi 3.

Hardware Limitation: The Raspberry Pi 3 (BCM2837 SoC) does not implement a TrustZone Address Space Controller (TZASC) or TrustZone Protection Controller (TZPC).

The Attack: Because TZASC is missing, the physical memory (DRAM) is not partitioned into Secure and Non-Secure regions at the hardware bus level. A compromised Normal World OS can simply use Direct Memory Access (DMA) or bypass the MMU to directly read the physical memory where OP-TEE resides.


They do not consider:

malicious physical hardware
physical tampering
cold-boot attacks
side-channel attacks 

The Raspberry Pi 3B+ does not provide the hardware isolation mechanisms needed to turn its TrustZone CPU security state into the same kind of hardware-enforced memory/peripheral isolation available on a more fully featured TrustZone SoC

https://suntong30.github.io/assets/pdf/IPSN24_dTEE.pdf

Connection to ML-KEM: This is a fatal platform limitation, not an OP-TEE bug. On a Raspberry Pi 3, a compromised Normal World OS can extract the ML-KEM secret key via DMA attacks. OP-TEE's official documentation notes that the Raspberry Pi 3 port is strictly for educational purposes and is not secure. Note: This applies equally to the 3B+.

7. TEE Side Channels and Hardware Attacks

Paper: iperfTZ: Understanding Network Bottlenecks for TrustZone-based Trusted Applications

Authors: Göttel et al.

Year: 2019

Venue: arXiv / Safety-critical Systems

Relevance: Analyzes network and communication bottlenecks between the Secure and Normal worlds.

Performance vs Security: While primarily a performance paper, it highlights the overhead of context switching and shared memory copying. Integrating ML-KEM will alter performance characteristics due to the speed of Kyber's key generation compared to RSA/ECC, potentially increasing the frequency of SMC calls if used heavily, which can expose timing variances.

8. ML-KEM / Kyber Security

Paper: SCA_protected_Kyber (Implementation and Analysis)

Authors: Ravi et al. (GitHub/Literature repository)

Relevance: Focuses on Side-Channel Attacks (SCA) against Kyber, specifically targeting the Number Theoretic Transform (NTT) operations.

Attack Mechanism: Power or electromagnetic (EM) analysis can recover intermediate values during polynomial multiplication (NTT).

Connection to ML-KEM: If your OP-TEE implementation of ML-KEM-512 is unprotected (Level-0), an attacker with physical proximity to the Raspberry Pi can measure power consumption during key generation and extract the secret key, entirely bypassing TrustZone isolation.

9. PQC vs ECC

medium blog : Post-Quantum Cryptography for EEE Students: A Visual Guide to Kyber
https://medium.com/%40shaikh.eamin/post-quantum-cryptography-for-eee-students-a-visual-guide-to-kyber-14793ff5b491
Authors: Shaikh

Year: 2025

Relevance: Explains the math and implementation constraints of Kyber (NTT optimizations) 



10. Relevance Matrix

# Relevance Matrix

This matrix summarizes the relevance of each paper/source to the research problem of integrating **ML-KEM-512 into OP-TEE** and protecting the ML-KEM secret key against a compromised Normal World.

## Relevance Rating

| Rating | Meaning                                                               |
| ------ | --------------------------------------------------------------------- |
| ★★★★★  | Essential — directly addresses a major aspect of the research problem |
| ★★★★   | Highly relevant — strongly supports the research                      |
| ★★★    | Relevant — useful supporting literature                               |
| ★★     | Background — provides contextual or performance information           |
| ★      | Low relevance — limited connection to the research problem            |

## Literature Relevance Matrix

| ID   | Paper                                                                                   |  Year | Topic                       | Problem Addressed                                    | Attack / Threat                 | Attacker Location | Security Property Affected | Raspberry Pi Relevant? | OP-TEE Relevant? | ML-KEM Relevant? | Evidence Type        | Rating |
| ---- | --------------------------------------------------------------------------------------- | ----: | --------------------------- | ---------------------------------------------------- | ------------------------------- | ----------------- | -------------------------- | ---------------------- | ---------------- | ---------------- | -------------------- | -----: |
| P001 | *SoK: TrustZone Vulnerabilities*                                                        |  2020 | TEE Security                | Architectural flaws in TEEs                          | Shared memory, malicious inputs | Normal World OS   | Confidentiality, Integrity | Yes — General          | Yes              | Indirect         | Direct — Software    |  ★★★★★ |
| P002 | *MyTEE: Own the Trusted Execution Environment on Embedded Devices*                      |  2020 | Hardware Security           | Lack of TZASC on embedded IoT platforms              | DMA attacks                     | Normal World OS   | Confidentiality            | Yes — Explicitly RPi3  | Yes              | Indirect         | Direct — Hardware    |  ★★★★★ |
| P003 | *OP-TEE Official Documentation*                                                         |  2021 | OP-TEE Architecture         | TEE architecture and cross-world communication       | Cross-world interface           | N/A               | N/A                        | Yes                    | Yes              | Indirect         | N/A                  |   ★★★★ |
| P004 | *Key-Encapsulation Mechanisms Embedded in Trusted Execution Environment: An Evaluation* |  2024 | PQC / TEE                   | Kyber performance in OP-TEE                          | Implementation overhead         | N/A               | Availability / Performance | Yes — ARM              | Yes              | **Direct**       | Direct               |   ★★★★ |
| P005 | *SCA_protected_Kyber*                                                                   | 2021+ | PQC Security                | Side-channel vulnerabilities in Kyber NTT operations | Power / EM analysis             | Physical          | Confidentiality            | Yes — Cortex-A/M       | Indirect         | **Direct**       | Direct — SCA         |   ★★★★ |
| P006 | *iperfTZ: Understanding Network Bottlenecks for TrustZone-based Trusted Applications*   |  2019 | Communication / Performance | Bottlenecks in TrustZone-based applications          | N/A                             | N/A               | Availability / Performance | Yes                    | Yes              | Indirect         | Direct — Performance |     ★★ |



11. Research Buildup

TEE -> TrustZone establishes a separate CPU state (Secure World).

TrustZone -> Hardware partitions memory via TZASC (ideally).

OP-TEE -> Provides the OS and API to bridge Linux to the Secure World.

TA -> The isolated sandbox where your ML-KEM code will actually execute.

Crypto Library -> The OP-TEE Core internal API that must be extended to support lattice-based math (NTT).

Cross-world Interface -> Where Linux passes buffers to the TA to retrieve the generated ML-KEM public key.

TEE Vulnerabilities -> If OP-TEE improperly validates the buffer size of the ML-KEM key, a compromised Linux OS can induce a buffer overflow.

Raspberry Pi Hardware -> Crucial failure point: Because RPi3 lacks TZASC, the compromised Linux OS can bypass OP-TEE entirely and read the ML-KEM private key via DMA.

PQC/Kyber -> The specific polynomial math creates new side-channel signatures (power/timing on NTT).

ML-KEM-512 -> Integrating this requires mitigating both OP-TEE software vulnerabilities and Kyber-specific side channels.

My Research Problem -> Developing a secure pipeline for ML-KEM inside OP-TEE, knowing the software must be mathematically secure, even if the physical Raspberry Pi 3 hardware fails to provide absolute isolation.

12. Research Gap

While recent papers (like Andrade et al., 2024) have benchmarked Kyber inside OP-TEE, literature is lacking in formal threat modeling and side-channel evaluation of ML-KEM specifically within the OP-TEE execution environment.

Existing research treats OP-TEE integration as a pure performance/porting task or evaluates Kyber side-channels on bare-metal Cortex-M microcontrollers. There is a missing link regarding how the OP-TEE software stack itself—through context switching, shared memory caching, and OS interrupts—might inadvertently leak timing or cache information about the ML-KEM secret key back to an attacker residing in the Normal World Linux OS.

13. Most Important Papers

Understanding the Prevailing Security Vulnerabilities in TrustZone-assisted TEE Systems

Why: It provides the ultimate taxonomy of how a compromised Normal World breaks into OP-TEE.

Focus on: Section on "Shared Memory" and "TA input validation".

MyTEE: Own the Trusted Execution Environment on Embedded Devices

Why: Proves that your chosen hardware (Raspberry Pi 3) is physically incapable of stopping DMA memory-extraction attacks.

Focus on: The Raspberry Pi 3 platform analysis and DMA attack sections.

Key-Encapsulation Mechanisms Embedded in Trusted Execution Environment: An Evaluation

Why: The closest paper to your actual project; implements NIST KEMs in OP-TEE.

Focus on: Benchmarks, memory footprint, and CPU overhead for Kyber-512.