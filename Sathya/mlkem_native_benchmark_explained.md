# Code Walkthrough: ML-KEM Keygen Benchmark (`mlkem-native`)

## What this program does, in one sentence

It calls the `crypto_kem_keypair()` function from the **mlkem-native** library (an implementation of the post-quantum key-encapsulation algorithm **ML-KEM**, formerly Kyber) 50,000 times in a row, and for every call it measures two things: **how long it took** and **how much stack memory it used**. The results are printed to the terminal and saved to two CSV files.

This is a **micro-benchmark**, the kind cryptography engineers write to check that a library's key-generation routine is fast and doesn't blow up the stack (which matters a lot on constrained devices like smart cards or microcontrollers).

---

## The big picture: two measurements, one loop

```
for 50,000 iterations:
    measure time to run crypto_kem_keypair()
    measure how deep into the stack that call reached
```

Everything else in the file exists to support those two measurements accurately, and to summarize the results afterward.

---

## Walking through the file top to bottom

### 1. Setup and configuration (`#define`s)

At the top, a handful of `#define` constants control the benchmark, each with a sensible default:

| Constant | Default | Meaning |
|---|---|---|
| `NUM_ITERS` | 50,000 | How many keypairs to generate |
| `WARMUP_ITERS` | 100 | (declared but not actually used to control the loop — see note below) |
| `LIB_NAME` | `"mlkem-native"` | Label written into the CSV files |
| `OUT_FILE` | `results_mlkem_native_time.csv` | Where timing results go |
| `STACK_FILE` | `results_mlkem_native_stack.csv` | Where stack-usage results go |
| `POISON_REGION_BYTES` | 16 KB | Size of the memory region used to detect stack usage |
| `POISON_BYTE` | `0xAA` | The "marker" byte used to detect untouched stack memory |
| `GUARD_BYTES` | 256 | A small safety gap left below the current stack pointer |

All of these can be overridden at compile time (e.g. `-DNUM_ITERS=1000`) without editing the source.

### 2. `now_us()` — a stopwatch

```c
static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}
```

Returns the current time in **microseconds**, using `CLOCK_MONOTONIC` — a clock that only ever moves forward and isn't affected by the system clock being adjusted (NTP sync, daylight savings, etc.). This makes it reliable for measuring elapsed time.

### 3. `cmp_double` / `cmp_long` — sorting helpers

Small comparator functions used later with `qsort()` so the timing and stack samples can be sorted (needed to compute the median and 99th percentile).

### 4. `get_sp()` — reading the CPU's stack pointer

```c
static inline uintptr_t get_sp(void) {
    uintptr_t sp;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(sp));
    return sp;
}
```

This uses **inline assembly** to read the raw stack pointer register directly — `rsp` on x86-64, `sp` on ARM64/ARM. There's also a portable fallback (using the address of a local variable) for any other architecture. Reading the real stack pointer is necessary because the next trick — measuring stack usage — needs to know exactly where the stack currently sits in memory.

### 5. The "stack poisoning" trick — how stack usage is measured

This is the most interesting part of the file, and it's a well-known technique for measuring **stack watermarking** without needing special tooling or compiler support.

**Step 1 — poison a region below the current stack pointer:**

```c
static void poison_stack_region(volatile uint8_t *base, size_t len) {
    for (size_t i = 0; i < len; i++) {
        base[i] = POISON_BYTE;
    }
}
```

Before calling the function under test, the code fills a 16 KB block of memory *just below* the current stack pointer with the byte `0xAA`. Since a function's local variables get placed on the stack as it executes, if `crypto_kem_keypair()` needs, say, 3 KB of stack space, its local variables will overwrite 3 KB of that poisoned region as it runs.

**Step 2 — call the function under test**, with memory barriers (`__asm__ __volatile__("" ::: "memory")`) on either side to stop the compiler from reordering instructions in a way that would corrupt the measurement.

**Step 3 — scan for how much of the poison survived:**

```c
static size_t scan_stack_depth(volatile uint8_t *base, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (base[i] != POISON_BYTE) {
            return len - i;
        }
    }
    return 0;
}
```

It scans from the *bottom* of the poisoned region upward, looking for the first byte that's still `0xAA` (untouched). Everything below that point was touched by the function call — so `len - i` is the deepest the stack reached, i.e. the function's **maximum stack usage** for that call.

Putting it together, `benchmark_keypair()` does all three steps and also times the call:

```c
static size_t benchmark_keypair(uint8_t *pk, uint8_t *sk, int *call_result, double *elapsed_us) {
    uintptr_t sp = get_sp();
    volatile uint8_t *poison_top  = (volatile uint8_t *)(sp - GUARD_BYTES);
    volatile uint8_t *poison_base = poison_top - POISON_REGION_BYTES;
    poison_stack_region(poison_base, POISON_REGION_BYTES);
    __asm__ __volatile__("" ::: "memory");
    double t0 = now_us();
    *call_result = crypto_kem_keypair(pk, sk);
    double t1 = now_us();
    *elapsed_us = t1 - t0;
    __asm__ __volatile__("" ::: "memory");
    return scan_stack_depth(poison_base, POISON_REGION_BYTES);
}
```

The `GUARD_BYTES` gap keeps the poisoning away from memory the *benchmark harness itself* is still using, so it doesn't accidentally poison its own live data.

> **Caveat the code itself checks for:** if the deepest touched byte reaches the very edge of the 16 KB poison region, the real stack usage might be *larger* than what was measured (the function could have used even more stack than the region covers). `report_stack()` prints a warning if this happens, telling you to increase `POISON_REGION_BYTES`.

### 6. `randombytes()` — supplying entropy to the crypto library

```c
int randombytes(uint8_t *out, size_t outlen) { ... }
```

`crypto_kem_keypair()` needs a source of randomness to generate keys, and it calls this function internally to get it. The implementation is platform-specific:

- **On macOS:** uses `getentropy()`, which only accepts up to 256 bytes at a time, so the code loops in 256-byte chunks.
- **On Linux/other:** uses `getrandom()`, which can be interrupted by a signal (`EINTR`), so the code retries in that case.

Both loops keep going until `outlen` bytes have been filled, and return `-1` on any real error.

### 7. `report_time()` and `report_stack()` — summarizing the results

After all 50,000 iterations finish, these two functions each:

1. Copy the raw samples and sort the copy.
2. Compute **mean**, **min**, **max**, **standard deviation** (time only), **median**, and **p99** (99th percentile).
3. Print a one-line summary to the terminal.
4. Write every individual raw sample to its CSV file, in the format:
   ```
   library,operation,iter,microseconds      (time)
   library,operation,iter,stack_bytes       (stack)
   ```

Writing every raw sample (not just the summary) means the CSVs can later be reloaded for deeper statistical analysis or plotting histograms.

### 8. `main()` — orchestrating everything

1. Allocates arrays to hold 50,000 timing and 50,000 stack-usage samples.
2. Opens the two CSV files and writes their header rows.
3. **Warms up** the library for 100 milliseconds by calling `benchmark_keypair()` repeatedly and throwing away the results — this lets CPU caches, branch predictors, and any lazy initialization inside the library settle before real measurements start, so the first "real" samples aren't artificially slow.
4. Runs the actual benchmark loop `NUM_ITERS` (50,000) times, storing each call's timing and stack depth. If `crypto_kem_keypair()` ever returns a nonzero (failure) result, the program aborts immediately with an error message.
5. Prints a header identifying the ML-KEM parameter set being tested (`MLK_CONFIG_PARAMETER_SET`, e.g. 512/768/1024) and the iteration/warmup counts.
6. Calls `report_time()` and `report_stack()` to print and save the final statistics.
7. Cleans up (closes files, frees memory) and exits.

---

## Why someone would write this

This kind of file is typical in **cryptographic library engineering**, especially for post-quantum algorithms like ML-KEM, where two things matter a lot beyond "does it work":

- **Speed** — key generation happens frequently in protocols like TLS, so its cost matters at scale.
- **Stack footprint** — ML-KEM and similar lattice-based schemes are known for needing relatively large stack buffers; on embedded devices with only a few KB of RAM, this can be the deciding factor in whether a library is even usable. Measuring it precisely (rather than guessing) is valuable.

The statistical rigor (warmup, many iterations, percentile reporting, CSV export) suggests this is part of a **benchmark suite comparing multiple ML-KEM implementations** — `LIB_NAME` being a configurable label, and the CSV schema including a `library` column, both point to results from several libraries eventually being merged and compared side by side.

