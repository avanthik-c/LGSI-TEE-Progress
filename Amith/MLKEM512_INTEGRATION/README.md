# ML-KEM-512 Integration into OP-TEE OS — Complete Change Log & Verification

This document records **every change actually made** to integrate
`mlkem-native`'s ML-KEM-512 key generation into OP-TEE OS as a genuine
GP-spec object type, callable via `TEE_AllocateTransientObject()` +
`TEE_GenerateKey()` — the same call shape used for RSA/ECDSA/Ed25519 — using
the real AArch64 NEON assembly backend. It also documents the real bugs
encountered and how each was diagnosed and fixed, and how the final result
was verified to be cryptographically correct (not just "doesn't crash").

Written so someone with no prior context could follow it end to end and
reproduce the same working system.

---

## Part 0 — What was built, in one sentence

A new object type, `TEE_TYPE_MLKEM512_KEYPAIR`, was added to OP-TEE OS core,
backed by `mlkem-native`'s real AArch64-optimized ML-KEM-512 implementation,
so that any Trusted Application can generate a real, standards-correct
ML-KEM-512 keypair using ordinary GP TEE Internal API calls.

---

## Part 1 — Vendoring the mlkem-native library

### 1.1 Source

Cloned from `https://github.com/pq-code-package/mlkem-native`. The `mlkem/`
subdirectory of that repo is a **single-compilation-unit (SCU) bundle** —
`mlkem_native.c` `#include`s every real source file it needs internally, and
`mlkem_native_asm.S` does the same for the AArch64/x86_64/RISC-V/PPC64LE
assembly backends (architecture-gated by preprocessor `#if`, so only the
AArch64 code actually compiles for this target).

### 1.2 Where it lives in the OP-TEE tree

Files copied to `core/mlkem_native/`:

```
core/mlkem_native/
├── mlkem_native.c          (from mlkem/mlkem_native.c)
├── mlkem_native_asm.S      (from mlkem/mlkem_native_asm.S)
├── mlkem_native.h          (from mlkem/mlkem_native.h)
├── mlkem_native_config.h   (our own file, see 1.3)
├── src/                    (copied wholesale from mlkem/src/ - everything
│                             mlkem_native.c #includes by relative path)
├── sub.mk                  (our own file, see 1.4)
├── mlkem512_keygen.c       (our own file, see Part 3)
```

**Important correction from the original plan**: this was first placed
under `core/lib/mlkem_native/`, following the pattern of third-party
libraries like `libtomcrypt`. That was wrong for this build — third-party
*libraries* under `core/lib/` need a separate `libname`/`libdir` +
`include mk/lib.mk` registration block (a different, heavier build
mechanism used for things linked as a standalone static library). Our code
is a plain source subdirectory of `core/` core code, exactly like
`core/crypto/` — so it needed to live directly under `core/`, not
`core/lib/`, and be registered the same simple way `core/crypto` is.

### 1.3 `core/mlkem_native/mlkem_native_config.h` (final, working version)

```c
#ifndef MLK_CONFIG_H
#define MLK_CONFIG_H

#define MLK_CONFIG_PARAMETER_SET 512
#define MLK_CONFIG_USE_NATIVE_BACKEND_ARITH
#define MLK_CONFIG_USE_NATIVE_BACKEND_FIPS202
#define MLK_CONFIG_NO_RANDOMIZED_API
#define MLK_CONFIG_NAMESPACE_PREFIX mlkem

/* REQUIRED when the two backend flags above are set - without these,
 * compilation fails with:
 *   "Bad configuration: MLK_CONFIG_USE_NATIVE_BACKEND_ARITH is set, but
 *    MLK_CONFIG_ARITH_BACKEND_FILE is not."
 * These point at the headers that actually declare the native/assembly
 * function interfaces for the arithmetic and FIPS-202 (Keccak) backends. */
#define MLK_CONFIG_ARITH_BACKEND_FILE "native/api.h"
#define MLK_CONFIG_FIPS202_BACKEND_FILE "fips202/native/api.h"

#endif
```

### 1.4 `core/mlkem_native/sub.mk`

```makefile
global-incdirs-y += .
srcs-y += mlkem_native.c
srcs-y += mlkem_native_asm.S
srcs-y += mlkem512_keygen.c

cflags-mlkem_native.c-y += -Wno-unused-parameter
cflags-mlkem_native.c-y += -Wno-sign-conversion
```

The two `cflags-` lines relax two specific warning classes only for the
vendored `mlkem_native.c` file (third-party code that wasn't written to
OP-TEE's own strict warning flags), without loosening warnings anywhere
else in the tree.

### 1.5 Registering the new subdirectory with the build

`core/sub.mk` is the file that tells OP-TEE's build system which
subdirectories of `core/` actually contain buildable code — it's the exact
same mechanism `core/crypto/` uses. The original (unmodified) file:

```makefile
subdirs-y += crypto
subdirs-y += drivers
subdirs-y += kernel
subdirs-y += mm
subdirs-y += pta
subdirs-y += tee
```

Changed to:

```makefile
subdirs-y += crypto
subdirs-y += mlkem_native
subdirs-y += drivers
subdirs-y += kernel
subdirs-y += mm
subdirs-y += pta
subdirs-y += tee
```

That single added line is what makes the build system descend into
`core/mlkem_native/` and read its `sub.mk` at all. Without it, none of the
files in that directory are ever compiled, silently.

`core/crypto.mk` (a *different* file — config selection for which crypto
backend/library is active, e.g. mbedtls vs tomcrypt) was checked and
confirmed to have no separate subdirectory list of its own that also needed
updating.

---

## Part 2 — Object type and attribute ID constants

### File: `lib/libutee/include/tee_api_defines.h`

Every `TEE_TYPE_*` and `TEE_ATTR_*` constant in OP-TEE follows a consistent
bit-packing scheme built around a small "family byte" per algorithm (the
same byte convention used for `TEE_MAIN_ALGO_*`). The existing ECC/25519/SM2
family occupied bytes `0x41`–`0x4A`; `0x4B` was confirmed free by grepping
the entire tree for collisions before use.

```c
#define TEE_TYPE_MLKEM512_PUBLIC_KEY         0xA000004B
#define TEE_TYPE_MLKEM512_KEYPAIR            0xA100004B

#define TEE_ATTR_MLKEM512_PUBLIC_VALUE       0xD000014B
#define TEE_ATTR_MLKEM512_PRIVATE_VALUE      0xC000024B
```

(`0xA0` = public-key-only object, `0xA1` = full keypair object — mirrors
every existing pair, e.g. `TEE_TYPE_ED25519_PUBLIC_KEY` /
`TEE_TYPE_ED25519_KEYPAIR`. `0xD0000...` / `0xC0000...` = public/private
buffer-type attribute, matching the exact bit pattern used by every
existing `*_PUBLIC_VALUE`/`*_PRIVATE_VALUE` pair.)

---

## Part 3 — The keypair struct and allocation/generation functions

### File: `core/include/crypto/crypto.h`

```c
struct mlkem512_keypair {
	uint8_t *pub;   /* MLKEM512_PUBLICKEYBYTES = 800 bytes */
	uint8_t *priv;  /* MLKEM512_SECRETKEYBYTES = 1632 bytes */
};

TEE_Result crypto_acipher_alloc_mlkem512_keypair(struct mlkem512_keypair *s,
						 size_t key_size_bits);
TEE_Result crypto_acipher_gen_mlkem512_key(struct mlkem512_keypair *key,
					   size_t key_size_bits);
```

### File: `core/mlkem_native/mlkem512_keygen.c`

This is the function that actually calls into the vendored library.
Final working version. The `calloc` calls below allocate the **persistent**
keypair object that outlives key generation (see Part 7.5). They are unrelated
to the transient-buffer problem in Part 7:

```c
// SPDX-License-Identifier: BSD-2-Clause
#include <crypto/crypto.h>
#include <kernel/thread.h>
#include <stdlib.h>
#include <string.h>
#include <string_ext.h>
#include <tee_api_types.h>
#include <trace.h>

#include "mlkem_native.h"

#define MLKEM512_PK_SIZE  MLKEM512_PUBLICKEYBYTES  /* 800 */
#define MLKEM512_SK_SIZE  MLKEM512_SECRETKEYBYTES  /* 1632 */
#define MLKEM512_KEY_SIZE_BITS UL(MLKEM512_PK_SIZE * 8)

TEE_Result crypto_acipher_alloc_mlkem512_keypair(struct mlkem512_keypair *s,
						 size_t key_size_bits)
{
	if (!s || key_size_bits != MLKEM512_KEY_SIZE_BITS)
		return TEE_ERROR_BAD_PARAMETERS;

	memset(s, 0, sizeof(*s));

	s->pub = calloc(1, MLKEM512_PK_SIZE);
	s->priv = calloc(1, MLKEM512_SK_SIZE);

	if (!s->pub || !s->priv) {
		free(s->pub);
		free(s->priv);
		return TEE_ERROR_OUT_OF_MEMORY;
	}

	return TEE_SUCCESS;
}

TEE_Result crypto_acipher_gen_mlkem512_key(struct mlkem512_keypair *key,
					   size_t key_size_bits)
{
	uint8_t coins[2 * MLKEM_SYMBYTES] __attribute__((aligned(16)));
	int mlk_rc;
	uint32_t vfp_state;

	if (key_size_bits != MLKEM512_KEY_SIZE_BITS)
		return TEE_ERROR_BAD_PARAMETERS;

	/* crypto_rng_read() is the kernel-side RNG - the same one that
	 * backs TEE_GenerateRandom() from TA userspace. This is the real
	 * TEE RNG, not a userspace convenience wrapper. */
	if (crypto_rng_read(coins, sizeof(coins)) != TEE_SUCCESS)
		return TEE_ERROR_BAD_STATE;

	/* Core code has no FPU/NEON access by default - must explicitly
	 * enable it for the duration of the native-backend call, and
	 * disable it again immediately after. */
	vfp_state = thread_kernel_enable_vfp();

	mlk_rc = mlkem_keypair_derand(key->pub, key->priv, coins);

	thread_kernel_disable_vfp(vfp_state);

	memzero_explicit(coins, sizeof(coins));

	if (mlk_rc != 0) {
		EMSG("mlkem_keypair_derand failed: %d", mlk_rc);
		return TEE_ERROR_BAD_STATE;
	}

	return TEE_SUCCESS;
}
```

**Bug hit here:** the very first version of this function did not call
`thread_kernel_enable_vfp()`/`thread_kernel_disable_vfp()`. Core/kernel code
in OP-TEE does not have FPU/NEON register access by default (unlike TA
userspace, which does) — attempting to execute NEON instructions without
this would fault. This must wrap **only** the actual native-backend call,
kept as short as possible.

---

## Part 4 — Attribute get/set operations

Object attributes need explicit "ops" functions telling the kernel how to
copy attribute bytes to/from user space, to/from binary (persistent
storage) format, and how to clear/free them. Ed25519/X25519 share one
ops-index because both their public and private values are the same fixed
size (32 bytes each). **ML-KEM's public (800B) and private (1632B) values
are different sizes**, so two separate ops-indices were needed, each a
mechanical adaptation of the real `op_attr_25519_*` functions with the size
constant changed.

### File: `core/tee/tee_svc_cryp.c`

**4.1 — Constants**, near the existing `ATTR_OPS_INDEX_*` definitions:

```c
#define ATTR_OPS_INDEX_MLKEM512_PUB   5
#define ATTR_OPS_INDEX_MLKEM512_PRIV  6

#define KEY_SIZE_BYTES_MLKEM512_PUB   UL(800)
#define KEY_SIZE_BYTES_MLKEM512_PRIV  UL(1632)
```

**4.2 — The fourteen functions.** Each is a near-identical adaptation of the
real, verified `op_attr_25519_*` function of the same name, with the size
constant swapped:

```c
/* --- Public value (800 bytes) --- */

static TEE_Result op_attr_mlkem512_pub_from_user(void *attr,
						 const void *buffer,
						 size_t size)
{
	uint8_t **key = attr;

	if (size != KEY_SIZE_BYTES_MLKEM512_PUB || !*key)
		return TEE_ERROR_SECURITY;

	return copy_from_user(*key, buffer, size);
}

static TEE_Result op_attr_mlkem512_pub_to_user(void *attr,
					       struct ts_session *sess __unused,
					       void *buffer, uint64_t *size)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	uint8_t **key = attr;
	uint64_t s = 0;
	uint64_t key_size = (uint64_t)KEY_SIZE_BYTES_MLKEM512_PUB;

	res = copy_from_user(&s, size, sizeof(s));
	if (res != TEE_SUCCESS)
		return res;

	res = copy_to_user(size, &key_size, sizeof(key_size));
	if (res != TEE_SUCCESS)
		return res;

	if (s < key_size || !buffer)
		return TEE_ERROR_SHORT_BUFFER;

	return copy_to_user(buffer, *key, key_size);
}

static TEE_Result op_attr_mlkem512_pub_to_binary(void *attr, void *data,
						 size_t data_len, size_t *offs)
{
	TEE_Result res;
	uint8_t **key = attr;
	size_t next_offs = 0;
	uint64_t key_size = (uint64_t)KEY_SIZE_BYTES_MLKEM512_PUB;

	res = op_u32_to_binary_helper(key_size, data, data_len, offs);
	if (res != TEE_SUCCESS)
		return res;

	if (ADD_OVERFLOW(*offs, key_size, &next_offs))
		return TEE_ERROR_OVERFLOW;

	if (data && next_offs <= data_len)
		memcpy((uint8_t *)data + *offs, *key, key_size);
	*offs = next_offs;

	return TEE_SUCCESS;
}

static bool op_attr_mlkem512_pub_from_binary(void *attr, const void *data,
					     size_t data_len, size_t *offs)
{
	uint8_t **key = attr;
	uint32_t s = 0;

	if (!op_u32_from_binary_helper(&s, data, data_len, offs))
		return false;
	if (*offs + s > data_len)
		return false;
	if (s > (uint32_t)KEY_SIZE_BYTES_MLKEM512_PUB)
		return false;

	memcpy(*key, (const uint8_t *)data + *offs, s);
	*offs += s;
	return true;
}

static TEE_Result op_attr_mlkem512_pub_from_obj(void *attr, void *src_attr)
{
	uint8_t **key = attr;
	uint8_t **src_key = src_attr;

	if (!*key || !*src_key)
		return TEE_ERROR_SECURITY;

	memcpy(*key, *src_key, KEY_SIZE_BYTES_MLKEM512_PUB);
	return TEE_SUCCESS;
}

static void op_attr_mlkem512_pub_clear(void *attr)
{
	uint8_t **key = attr;

	assert(*key);
	memzero_explicit(*key, KEY_SIZE_BYTES_MLKEM512_PUB);
}

static void op_attr_mlkem512_pub_free(void *attr)
{
	uint8_t **key = attr;

	op_attr_mlkem512_pub_clear(attr);
	free(*key);
}

/* --- Private value (1632 bytes) - identical pattern, different size --- */

static TEE_Result op_attr_mlkem512_priv_from_user(void *attr,
						  const void *buffer,
						  size_t size)
{
	uint8_t **key = attr;

	if (size != KEY_SIZE_BYTES_MLKEM512_PRIV || !*key)
		return TEE_ERROR_SECURITY;

	return copy_from_user(*key, buffer, size);
}

static TEE_Result op_attr_mlkem512_priv_to_user(void *attr,
						struct ts_session *sess __unused,
						void *buffer, uint64_t *size)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	uint8_t **key = attr;
	uint64_t s = 0;
	uint64_t key_size = (uint64_t)KEY_SIZE_BYTES_MLKEM512_PRIV;

	res = copy_from_user(&s, size, sizeof(s));
	if (res != TEE_SUCCESS)
		return res;

	res = copy_to_user(size, &key_size, sizeof(key_size));
	if (res != TEE_SUCCESS)
		return res;

	if (s < key_size || !buffer)
		return TEE_ERROR_SHORT_BUFFER;

	return copy_to_user(buffer, *key, key_size);
}

static TEE_Result op_attr_mlkem512_priv_to_binary(void *attr, void *data,
						  size_t data_len, size_t *offs)
{
	TEE_Result res;
	uint8_t **key = attr;
	size_t next_offs = 0;
	uint64_t key_size = (uint64_t)KEY_SIZE_BYTES_MLKEM512_PRIV;

	res = op_u32_to_binary_helper(key_size, data, data_len, offs);
	if (res != TEE_SUCCESS)
		return res;

	if (ADD_OVERFLOW(*offs, key_size, &next_offs))
		return TEE_ERROR_OVERFLOW;

	if (data && next_offs <= data_len)
		memcpy((uint8_t *)data + *offs, *key, key_size);
	*offs = next_offs;

	return TEE_SUCCESS;
}

static bool op_attr_mlkem512_priv_from_binary(void *attr, const void *data,
					      size_t data_len, size_t *offs)
{
	uint8_t **key = attr;
	uint32_t s = 0;

	if (!op_u32_from_binary_helper(&s, data, data_len, offs))
		return false;
	if (*offs + s > data_len)
		return false;
	if (s > (uint32_t)KEY_SIZE_BYTES_MLKEM512_PRIV)
		return false;

	memcpy(*key, (const uint8_t *)data + *offs, s);
	*offs += s;
	return true;
}

static TEE_Result op_attr_mlkem512_priv_from_obj(void *attr, void *src_attr)
{
	uint8_t **key = attr;
	uint8_t **src_key = src_attr;

	if (!*key || !*src_key)
		return TEE_ERROR_SECURITY;

	memcpy(*key, *src_key, KEY_SIZE_BYTES_MLKEM512_PRIV);
	return TEE_SUCCESS;
}

static void op_attr_mlkem512_priv_clear(void *attr)
{
	uint8_t **key = attr;

	assert(*key);
	memzero_explicit(*key, KEY_SIZE_BYTES_MLKEM512_PRIV);
}

static void op_attr_mlkem512_priv_free(void *attr)
{
	uint8_t **key = attr;

	op_attr_mlkem512_priv_clear(attr);
	free(*key);
}
```

**4.3 — Registering both in the `attr_ops[]` table**:

```c
static const struct attr_ops attr_ops[] = {
	/* ... existing entries ... */
	[ATTR_OPS_INDEX_MLKEM512_PUB] = {
		.from_user = op_attr_mlkem512_pub_from_user,
		.to_user = op_attr_mlkem512_pub_to_user,
		.to_binary = op_attr_mlkem512_pub_to_binary,
		.from_binary = op_attr_mlkem512_pub_from_binary,
		.from_obj = op_attr_mlkem512_pub_from_obj,
		.free = op_attr_mlkem512_pub_free,
		.clear = op_attr_mlkem512_pub_clear,
	},
	[ATTR_OPS_INDEX_MLKEM512_PRIV] = {
		.from_user = op_attr_mlkem512_priv_from_user,
		.to_user = op_attr_mlkem512_priv_to_user,
		.to_binary = op_attr_mlkem512_priv_to_binary,
		.from_binary = op_attr_mlkem512_priv_from_binary,
		.from_obj = op_attr_mlkem512_priv_from_obj,
		.free = op_attr_mlkem512_priv_free,
		.clear = op_attr_mlkem512_priv_clear,
	},
};
```

---

## Part 5 — The type-attrs array and object-properties table entry

### File: `core/tee/tee_svc_cryp.c`

```c
static
const struct tee_cryp_obj_type_attrs tee_cryp_obj_mlkem512_keypair_attrs[] = {
	{
	.attr_id = TEE_ATTR_MLKEM512_PRIVATE_VALUE,
	.flags = TEE_TYPE_ATTR_REQUIRED,
	.ops_index = ATTR_OPS_INDEX_MLKEM512_PRIV,
	RAW_DATA(struct mlkem512_keypair, priv)
	},

	{
	.attr_id = TEE_ATTR_MLKEM512_PUBLIC_VALUE,
	.flags = TEE_TYPE_ATTR_REQUIRED,
	.ops_index = ATTR_OPS_INDEX_MLKEM512_PUB,
	RAW_DATA(struct mlkem512_keypair, pub)
	},
};
```

In `tee_cryp_obj_props[]`:

```c
	PROP(TEE_TYPE_MLKEM512_KEYPAIR, 1, (KEY_SIZE_BYTES_MLKEM512_PUB * 8),
	     (KEY_SIZE_BYTES_MLKEM512_PUB * 8),
	     sizeof(struct mlkem512_keypair),
	     tee_cryp_obj_mlkem512_keypair_attrs),
```
---

## Part 6 — Wiring the keygen dispatch

### File: `core/tee/tee_svc_cryp.c`

In `syscall_obj_generate_key()`'s `switch (o->info.objectType)`:

```c
case TEE_TYPE_MLKEM512_KEYPAIR:
	res = tee_svc_obj_generate_key_mlkem512(o, type_props, key_size,
						params, param_count);
	if (res != TEE_SUCCESS)
		goto out;
	break;
```

The function itself, mirroring the real `tee_svc_obj_generate_key_ed25519`:

```c
static TEE_Result
tee_svc_obj_generate_key_mlkem512(struct tee_obj *o,
				  const struct tee_cryp_obj_type_props
							*type_props,
				  uint32_t key_size,
				  const TEE_Attribute *params,
				  uint32_t param_count)
{
	TEE_Result res;
	struct mlkem512_keypair *key = NULL;

	res = tee_svc_cryp_obj_populate_type(o, type_props, params,
					     param_count);
	if (res != TEE_SUCCESS)
		return res;

	key = o->attr;

	res = crypto_acipher_gen_mlkem512_key(key, key_size);
	if (res != TEE_SUCCESS)
		return res;

	set_attribute(o, type_props, TEE_ATTR_MLKEM512_PRIVATE_VALUE);
	set_attribute(o, type_props, TEE_ATTR_MLKEM512_PUBLIC_VALUE);

	return TEE_SUCCESS;
}
```

### Object allocation dispatch — `tee_obj_set_type()`

Also in `core/tee/tee_svc_cryp.c` — this is what
`TEE_AllocateTransientObject()` reaches:

```c
case TEE_TYPE_MLKEM512_KEYPAIR:
	res = crypto_acipher_alloc_mlkem512_keypair(o->attr, max_key_size);
	break;
```

**Process note, not a bug in the final result, but worth recording:** this
switch statement has many visually-similar `case TEE_TYPE_*_KEYPAIR:`
blocks. Every edit to this file was made by grepping for exact line
numbers first and confirming with `git diff` immediately afterward — a
`sed` pattern-match earlier in this project (documented in the companion
shift-cipher guide) once silently deleted an unrelated pre-existing case
(`TEE_ALG_DES3_CMAC`) by matching the wrong occurrence of a repeated
string. That specific class of mistake was avoided here by treating every
edit to a file with repeated patterns as requiring a `git diff` check
before moving on, without exception.

---

## Part 7 — The stack overflow saga (full postmortem)

> **Correction notice.** An earlier revision of this document concluded that the
> kernel stack could not be enlarged and fixed the crash by moving buffers to
> the heap inside the vendored library. That conclusion was wrong. The heap
> migration has been reverted and replaced by a one-line configuration fix.
> The earlier version remains available through `git log -p README.md`.

### 7.1 — First symptom

A TA call to `TEE_GenerateKey()` panicked with `Core data-abort ...
(translation fault)`, deep inside `mlk_poly_rej_uniform_x4` (called from
`mlk_gen_matrix` → `mlk_indcpa_keypair_derand` → `mlkem_keypair_derand` →
our `crypto_acipher_gen_mlkem512_key`). Disassembly showed `sub sp, sp, #0xb60`
(2912 bytes) in that function's prologue, followed immediately by a fault on
the next store: the signature of a genuine stack overflow.

ML-KEM-512's batched Keccak operations need roughly 10KB of transient memory
(noise polynomial buffers and 4-way Keccak state), while the default OP-TEE
per-thread kernel stack is **8KB**.

### 7.2 — Wrong attempt #1: `CFG_CORE_THREAD_STACK_SIZE` (a variable that does not exist)

`CFG_CORE_THREAD_STACK_SIZE` was bumped to 128KB and 256KB with zero effect.
The earlier revision blamed the wrong stack. The real reason is simpler:
**this variable does not exist anywhere in the OP-TEE source tree.** OP-TEE's
Makefile build silently ignores unrecognized `CFG_*` variables, so the stack
stayed at 8KB.

### 7.3 — Wrong attempt #2: `CFG_STACK_TMP_EXTRA` (a real variable for the wrong stack)

`CFG_STACK_TMP_EXTRA` is a real knob, and it was tried at 8192, 16384 and 65536
(applied values confirmed via `grep` of the generated `conf.mk`/`conf.h` after a
forced clean rebuild). The crash persisted identically. Reading
`core/arch/arm/kernel/thread_a64.S` shows why: syscall dispatch runs on the
main **per-thread** stack (`THREAD_CTX_KERN_SP`), not the tmp stack, so
enlarging the tmp stack could not help.

The boot hang seen earlier when combining a large thread-stack value with a
large `CFG_STACK_TMP_EXTRA` cannot have come from the combination, since the
first variable was a no-op. It came from the oversized tmp stack alone
(multiplied across `-smp 2` within QEMU's fixed TZDRAM).

### 7.4 — Interim workaround (reverted): moving buffers to the heap

Believing the thread stack was fixed at 8KB, the vendored source was modified:
`sampling.c` (`mlk_poly_rej_uniform_x4`) and `poly_k.c`
(`mlk_poly_getnoise_eta1_4x`) were changed to use `memalign(16, ...)` heap
buffers with `mlk_zeroize()` + `free()` on every exit path. It stopped the
crash but was the wrong fix:

- it made invasive changes to a vendored upstream library, complicating future
  upstream updates and review;
- it moved transient secret-dependent scratch data onto the shared secure heap,
  adding lifecycle risk that stack allocation avoids for free.

All of these changes have been reverted; `src/` is back to upstream.

### 7.5 — The real fix: `CFG_STACK_THREAD_EXTRA`

OP-TEE's supported knob for the main kernel thread stack is
`CFG_STACK_THREAD_EXTRA`. The final size is:

```
STACK_THREAD_SIZE = 8192 + CFG_STACK_THREAD_EXTRA
```

OP-TEE's own NXP SE050 crypto driver raises this same variable for its stack
headroom, which is strong precedent that this is the intended mechanism for
heavy crypto in core. In the build repo's `qemu_v8.mk`:

```makefile
OPTEE_OS_COMMON_FLAGS += CFG_STACK_THREAD_EXTRA=24576
```

That gives 8KB + 24KB = **32KB per thread**. The original stack allocations in
`sampling.c` and `poly_k.c` were restored, so transient buffers again live on
the stack and disappear automatically when the function returns.

The `calloc` allocations in `mlkem512_keygen.c` for `struct mlkem512_keypair`
(`pub`, `priv`) stay on the heap. That is correct and follows the RSA/ECC
pattern, because that object is **persistent**: it must outlive the generation
function and be returned to the TA.

**Rule of thumb:** transient scratchpads (Keccak state, noise buffers) belong
on the stack; long-lived key objects belong on the secure heap.

### 7.6 — Lessons for anyone hitting this class of bug

1. **Grep-verify every `CFG_*` variable against the OP-TEE source before
   trusting it.** A misspelled or nonexistent variable is silently ignored and
   produces no build error.
2. **Confirm which stack the failing code actually runs on** (read
   `thread_a64.S`) before choosing which knob to turn.
3. A `sub sp, sp, #<large>` prologue followed by a translation fault on the next
   store is close to a definitive stack-overflow signature. Fix it in the build
   configuration first; modify vendored code only as a last resort.

---

## Part 8 — Test TA/CA and how it's used

### `optee_examples/mlkem_test/` — mirrors the standard example layout

**`ta/include/ta_mlkem_test.h`** — UUID + one command ID
(`TA_MLKEM_CMD_GENERATE_KEY`).

**`ta/mlkem_test_ta.c`** — `generate_key()` handler:
1. Validates the two output param sizes against `MLKEM512_PUBLICKEYBYTES`
   (800) / `MLKEM512_SECRETKEYBYTES` (1632)
2. `TEE_AllocateTransientObject(TEE_TYPE_MLKEM512_KEYPAIR, 800*8, &key)`
3. `TEE_GenerateKey(key, 800*8, NULL, 0)`
4. `TEE_GetObjectBufferAttribute(key, TEE_ATTR_MLKEM512_PUBLIC_VALUE, ...)`
   and the equivalent for the private value, copying both out to the CA

**`host/main.c`** — allocates 800/1632-byte buffers, invokes the command,
and:
- Prints the full hex dump of both keys (16 bytes per line)
- Times `TEEC_InvokeCommand()` with `clock_gettime(CLOCK_MONOTONIC, ...)`
  around the call, run in a loop of 50 iterations, reporting
  average/min/max — this is the round-trip cost (normal-world → secure-
  world context switch + actual keygen + switch back), the fair number to
  compare against an equivalently-measured classical (RSA) baseline

**`ta/user_ta_header_defines.h`** — `TA_STACK_SIZE (4 * 1024)`. Note this is
the **TA userspace** stack, separate from the kernel/core stacks discussed in
Part 7. 4KB is sufficient because the TA is only thin plumbing around the
syscall; the heavy Keccak/NTT work runs in OP-TEE core, on the kernel thread
stack sized by `CFG_STACK_THREAD_EXTRA`.

**Build registration**: `mlkem_test/CMakeLists.txt` added, and
`optee_examples/CMakeLists.txt` (top-level) picks it up automatically via
its existing `file(GLOB dirs *)` auto-discovery — no manual registration
list to edit.

---

## Part 9 — How the output was verified

Three separate, escalating levels of proof were used — "doesn't crash and
produces the right number of bytes" is the weakest possible claim, so each
level below establishes something strictly stronger.

### 9.1 — Level 1: structural self-consistency

FIPS 203's secret-key format embeds `H(pk)` (SHA3-256 of the public key) at
a fixed offset (bytes 1568–1599 of the 1632-byte secret key). Independently
hashing the extracted public key and comparing it against those embedded
bytes confirms the object's internal structure is correct — a real, if
partial, correctness check obtainable from a single keypair with no
reference implementation needed.

### 9.2 — Level 2: official Known-Answer-Test (KAT) vectors

`mlkem-native`'s own repository ships official, deterministic test vectors
(`test/test_vectors/expected_test_vectors.h`) — a fixed 64-byte seed
(`d || z`, the `coins` input) and the exact expected `pk`/`sk` byte output
for ML-KEM-512.

**Procedure used:**
1. The real RNG call in `crypto_acipher_gen_mlkem512_key()`
   (`crypto_rng_read(coins, sizeof(coins))`) was temporarily replaced with
   a `memcpy()` from a hardcoded array containing the exact published `d`
   and `z` bytes.
2. The TA was rebuilt and run, producing a fully deterministic keypair.
3. The printed hex output was captured and converted to binary using a
   purpose-built script (`hex_to_bin_and_verify.py` — reads pasted hex from
   a **file**, not a shell argument, avoiding a length-truncation bug hit
   with an earlier `echo -n "..." | xxd -r -p` one-liner approach; extracts
   every 2-character hex byte pair via regex, so stray whitespace/labels in
   the pasted text don't break parsing; reports the exact byte count found,
   so an incomplete paste is caught immediately instead of silently
   producing a truncated file).
4. That binary was `cmp`-compared, byte for byte, against the official
   `expected_pk.bin`/`expected_sk.bin` extracted directly from the
   upstream repository's own header file.
5. **Separately**, the same fixed seed was fed into a native (non-TEE)
   build of mlkem-native, compiled and run directly on the host machine, as
   a second independent cross-check that the expected values themselves
   were extracted correctly.
6. The hardcoded seed was reverted back to the real
   `crypto_rng_read()` call immediately after verification — a fixed seed
   must never ship in a real build, since it makes every "generated" key
   fully predictable and provides no security whatsoever.

An exact match at this level means: the entire chain — OP-TEE's
`TEE_GenerateKey()` syscall dispatch, the object-attribute system, and the
real AArch64 NEON assembly backend — reproduces bit-for-bit the same output
the reference implementation's own maintainers verify against for a given
input. This is meaningfully stronger evidence than "the sizes are right":
it confirms the actual polynomial/lattice arithmetic is being computed
correctly through this entire new integration path.

### 9.3 — Level 3 (available, not yet performed): full round-trip

Not yet done, but the natural next step: encapsulate against the
TEE-generated public key using an independent tool (e.g. a host-native
mlkem-native or OpenSSL 3.x build), decapsulate the resulting ciphertext
using the TEE-generated secret key (fed back through the TA), and confirm
the shared secret matches on both sides. This would prove the *whole*
keypair is functionally usable together, not just that each half
individually matches known-good reference bytes.


## Part 10 — Complete file-change summary

| File | Change |
|---|---|
| `core/mlkem_native/mlkem_native.c` (new) | Vendored SCU bundle, unmodified from upstream |
| `core/mlkem_native/mlkem_native_asm.S` (new) | Vendored SCU assembly bundle, unmodified from upstream |
| `core/mlkem_native/mlkem_native.h` (new) | Vendored public header, unmodified from upstream |
| `core/mlkem_native/src/` (new dir) | Vendored source tree, unmodified from upstream |
| `build/qemu_v8.mk` | `OPTEE_OS_COMMON_FLAGS += CFG_STACK_THREAD_EXTRA=24576` (32KB per-thread kernel stack) |
| `core/mlkem_native/mlkem_native_config.h` (new) | Parameter set 512, native backends on, backend file macros, no randomized API, custom namespace prefix |
| `core/mlkem_native/mlkem512_keygen.c` (new) | `crypto_acipher_alloc_mlkem512_keypair()`, `crypto_acipher_gen_mlkem512_key()` — VFP enable/disable, RNG fill, calls into `mlkem_keypair_derand()` |
| `core/mlkem_native/sub.mk` (new) | Build file list + per-file warning relaxation |
| `core/sub.mk` | Added `subdirs-y += mlkem_native` |
| `lib/libutee/include/tee_api_defines.h` | `TEE_TYPE_MLKEM512_PUBLIC_KEY`, `TEE_TYPE_MLKEM512_KEYPAIR`, `TEE_ATTR_MLKEM512_PUBLIC_VALUE`, `TEE_ATTR_MLKEM512_PRIVATE_VALUE` |
| `core/include/crypto/crypto.h` | `struct mlkem512_keypair`, 2 function prototypes |
| `core/tee/tee_svc_cryp.c` | Ops-index + key-size constants; 14 attribute-ops functions; `attr_ops[]` table entries; `tee_cryp_obj_mlkem512_keypair_attrs[]`; `PROP(TEE_TYPE_MLKEM512_KEYPAIR, ...)` entry; `tee_svc_obj_generate_key_mlkem512()`; dispatch case in `syscall_obj_generate_key()`; dispatch case in `tee_obj_set_type()` |
| `optee_examples/mlkem_test/` (new) | Full example TA/CA — see Part 8 |

**Tried and reverted (not present in the final state):**
- `CFG_CORE_THREAD_STACK_SIZE`: a variable that does not exist in OP-TEE; silently ignored by the build
- `CFG_STACK_TMP_EXTRA`: a real variable, but for a stack the syscall path does not use
- Heap migration of buffers in `src/sampling.c` and `src/poly_k.c` (and the `polyvec` alignment attribute added for it): replaced by `CFG_STACK_THREAD_EXTRA`
- The hardcoded KAT seed in `mlkem512_keygen.c`: used only transiently for Part 9.2's verification, reverted immediately after

---

## Part 11 — Reproducing this from scratch: the short version

1. Clone `mlkem-native`, copy `mlkem/` contents into `core/mlkem_native/`
   as described in Part 1.
2. Write `mlkem_native_config.h` exactly as in Part 1.3 (both backend-file
   macros are mandatory, not optional, if the native backend flags are set).
3. Register the new directory in `core/sub.mk` (Part 1.5) — **not**
   `core/lib/`.
4. Add the four constants from Part 2.
5. Add the struct and two function prototypes from Part 3, and the real
   implementation (with VFP enable/disable) from Part 3's
   `mlkem512_keygen.c`.
6. Add all fourteen attribute-ops functions from Part 4, double-checking
   every signature against your own tree's real `struct attr_ops`
   definition before compiling (don't assume it matches this document
   exactly — OP-TEE versions differ).
7. Add the type-attrs array and `PROP(...)` table entry from Part 5, being
   careful to only reference macros that are actually visible in
   `tee_svc_cryp.c` itself (Part 5's bug).
8. Wire the two dispatch points from Part 6, verifying each with
   `git diff` before rebuilding, given how repetitive-looking these
   switch statements are.
9. Build with `CFG_STACK_THREAD_EXTRA=24576` in your platform makefile
   (Part 7.5). If you hit a stack-related crash deep in
   `mlk_poly_rej_uniform_x4` or a similar function, first confirm with `grep`
   that the variable actually exists in your OP-TEE tree and appears in the
   generated `conf.mk`. Do **not** modify the vendored source.
10. Build the test TA/CA from Part 8.
11. Verify using Part 9's KAT procedure before trusting the result.
