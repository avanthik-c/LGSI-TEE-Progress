# mlkem-native Benchmark — Native Assembly Build Explained

This benchmark compiles **two versions** of ML-KEM-512 (`mlkem-native`) side by side —
a portable C implementation and an architecture-specific "native backend" build that
uses hand-written assembly — and measures `crypto_kem_keypair` performance for each.
This document explains how the native build works and why it's faster.

## The two build targets

The Makefile produces two binaries from the same source tree:

| Binary | Backend | Output |
|---|---|---|
| `bench_mlkem_c` | Pure C, portable | `results_c.csv` |
| `bench_mlkem_native` | C + arch-specific `.S` assembly | `results_native.csv` |

Both compile the exact same `mlkem_native/src/*.c` files. The difference is that the
native build additionally compiles and links hand-written assembly files, and flips on
two macros that reroute the hottest functions to call into that assembly instead of
their C equivalents.

## Step 1: Architecture detection

```make
ARCH_RAW := $(shell uname -m)
ifeq ($(ARCH_RAW),arm64)
    ARCH := aarch64
else
    ARCH := $(ARCH_RAW)
endif
```

`uname -m` reports the host CPU architecture (`x86_64`, `aarch64`, `arm64`, etc.).
The one normalization needed is Apple Silicon: macOS reports `arm64`, but
mlkem-native's source tree names the folder `aarch64`, so the Makefile maps one to
the other. This `ARCH` variable is then used to pick the right assembly directory and
compiler flags automatically — no manual configuration needed per machine.

## Step 2: Discovering the assembly source

```make
ASM_SOURCE := $(shell find mlkem_native/src/native/$(ARCH) -name '*.S' 2>/dev/null) \
              $(shell find mlkem_native/src/fips202/native/$(ARCH) -name '*.S' 2>/dev/null)
```

mlkem-native ships pre-written `.S` (assembly, run through the C preprocessor) files
per architecture, in two places:

- `src/native/<arch>/` — assembly implementations of the ML-KEM polynomial arithmetic
  (NTT / inverse-NTT butterflies, Barrett/Montgomery reduction, pointwise multiplication)
- `src/fips202/native/<arch>/` — assembly implementations of Keccak/SHA-3 (FIPS 202),
  which ML-KEM uses heavily for its XOF, hashing, and expansion steps

Because the `find` is parameterized by `$(ARCH)`, this same rule transparently picks
up the x86_64 AVX2 files, AArch64 NEON files, or whatever else exists for the host —
the build doesn't need per-architecture Makefile branches beyond the flags below.

## Step 3: Architecture-specific compiler flags

```make
ifeq ($(ARCH),x86_64)
    NATIVE_FLAGS := -mavx2 -mbmi2 -mpopcnt \
        -DMLK_CONFIG_USE_NATIVE_BACKEND_ARITH -DMLK_CONFIG_USE_NATIVE_BACKEND_FIPS202
else
    NATIVE_FLAGS := -DMLK_CONFIG_USE_NATIVE_BACKEND_ARITH -DMLK_CONFIG_USE_NATIVE_BACKEND_FIPS202
endif
```

Two things happen here, and they're doing different jobs:

1. **`-mavx2 -mbmi2 -mpopcnt`** — these tell `gcc` itself it's allowed to *use* AVX2
   (256-bit vector), BMI2 (bit-manipulation), and POPCNT instructions when compiling.
   This is required on x86_64 because the `.S` files contain literal AVX2 instructions —
   without these flags the assembler would reject them (or gcc would refuse to invoke
   the right instruction set state). ARM/AArch64 doesn't need an equivalent flag here
   because its NEON assembly doesn't require an extra ISA-enablement flag the way AVX2 does.

2. **`-DMLK_CONFIG_USE_NATIVE_BACKEND_ARITH`** and **`-DMLK_CONFIG_USE_NATIVE_BACKEND_FIPS202`**
   — these are mlkem-native's own build-time switches, unrelated to the compiler. They
   activate `#ifdef` branches inside the C source that swap out C function bodies for
   thin wrappers calling into the compiled assembly symbols. Without these two macros,
   the assembly would be compiled and linked into the binary but never *called* — the
   C path would still run. This is exactly the bug the benchmark work caught earlier
   (a "portable" build silently linking AVX2 code): the fix was making these flags
   variant-specific instead of always-on.

## Step 4: Two independent gcc invocations

```make
bench_mlkem_c: $(MLK_SOURCE) $(APP_SOURCE)
	$(CC) $(BASE_CFLAGS) ... $^ -o $@ $(LDFLAGS)

bench_mlkem_native: $(MLK_SOURCE) $(ASM_SOURCE) $(APP_SOURCE)
	$(CC) $(BASE_CFLAGS) $(NATIVE_FLAGS) ... $^ -o $@ $(LDFLAGS)
```

`bench_mlkem_c` only ever sees `$(MLK_SOURCE)` (the `.c` files) — the `.S` files are
never passed to the compiler, and `NATIVE_FLAGS` is never added, so the native macros
are undefined and every call falls through to the plain C implementation.

`bench_mlkem_native` additionally compiles `$(ASM_SOURCE)` and adds `NATIVE_FLAGS`, so
the assembly is both present in the final binary *and* actually reached at runtime.

Keeping these as two fully separate `gcc` command lines (rather than one rule with a
conditional) is what guarantees the portable binary can't accidentally inherit
AVX2 code paths or flags — each target's flags and sources are self-contained.

## Why the native/assembly backend is faster

The performance gap comes from a few compounding effects:

**1. SIMD width the compiler won't reliably reach on its own.**
ML-KEM's core operation is the Number-Theoretic Transform (NTT) over a ring of
degree-256 polynomials with 16-bit coefficients — thousands of small, uniform
modular-arithmetic operations per keygen. AVX2 registers are 256 bits wide, so a
single instruction can process 16 × 16-bit coefficients at once. Auto-vectorization
in a general-purpose C compiler is conservative: it has to prove a transformation is
safe for arbitrary input and often can't restructure the specific butterfly/reduction
pattern NTT uses. The hand-written assembly is written directly against the known
data layout, so it uses the full vector width every time without relying on the
compiler to discover that opportunity.

**2. Modular reduction without expensive division.**
ML-KEM's arithmetic is modulo a fixed prime (3329). Naively, that's a division/modulo
per coefficient — one of the slowest common integer operations. Barrett and
Montgomery reduction (used here) replace that division with a small fixed sequence of
multiplies, shifts, and subtracts using precomputed constants. The assembly
implements these reduction sequences directly at the instruction level with no
redundant loads/stores, whereas the compiler's version of the same C code introduces
extra register spills and bounds-safety scaffolding.

**3. Instruction and register allocation tuned for the exact operation.**
Hand-written assembly can pick the exact instruction (e.g., `vpmulhw`, `vpsubw`,
`vpand` for AVX2) and register assignment for the algorithm, keeping data resident in
vector registers across an entire NTT layer instead of spilling to memory between
compiler-generated basic blocks. A C compiler optimizing generic C source has to be
more general and typically can't sustain that level of register reuse.

**4. BMI2/POPCNT for bit-level housekeeping.**
Rejection sampling during key generation (`SampleNTT`) needs fast, branch-light
bit-counting and extraction — the kind of thing `-mbmi2 -mpopcnt` unlocks directly at
the instruction level rather than emulating with shifts and masks.

**5. FIPS 202 (Keccak/SHA-3) is also hand-optimized.**
ML-KEM calls SHA-3/SHAKE extensively for hashing and expansion. The
`fips202/native/<arch>` assembly applies the same idea — a vectorized Keccak-f[1600]
permutation — since this is often a significant fraction of total keygen time and
benefits from the same SIMD treatment as the arithmetic core.

None of this changes the algorithm or its security properties — it's the identical
ML-KEM-512 computation, just executed through instruction sequences and register
usage that the reference C code, run through a general-purpose compiler, doesn't
reliably reach on its own. That's the entire native-vs-portable gap this benchmark
measures.

## Running it

```sh
make run     # builds both binaries, runs each, writes results_c.csv / results_native.csv
make clean   # removes binaries and result CSVs
```

`bench_mlkem_c` reports the portable baseline; `bench_mlkem_native` reports the
architecture-accelerated result (`mlkem-native-x86_64`, `mlkem-native-aarch64`, etc.,
per the `LIB_NAME` macro). Comparing the two CSVs is the actual speedup measurement.

