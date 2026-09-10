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

**Bug hit here:** the first version of this file set the two
`USE_NATIVE_BACKEND_*` flags without the two `*_BACKEND_FILE` macros. This
compiled fine in isolation but failed as soon as `mlkem_native.c` was
actually built, with the exact `#error` shown above — the two backend flags
are a "turn this on" switch, and the `_FILE` macros are "and here's the
header that implements it," and mlkem-native requires both together by
design (it has no default file to fall back to, since the right header
differs per architecture/backend combination).

`MLK_CONFIG_NO_RANDOMIZED_API` means the library never generates its own
randomness internally — every entry point takes explicit randomness as a
parameter (`coins`), which is filled from OP-TEE's real RNG, never
mlkem-native's own (host-only, testing-oriented) `randombytes()`.

`MLK_CONFIG_NAMESPACE_PREFIX mlkem` renames every internal symbol from the
library's default (long, parameter-set-embedding) names to short
`mlkem_`-prefixed ones — e.g. the callable keygen function becomes
`mlkem_keypair_derand()` instead of
`PQCP_MLKEM_NATIVE_MLKEM512_keypair_derand()`.

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

This is the function that actually calls into the vendored library. Final
working version, reflecting the heap-migration fix described in Part 7:

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

**Bug hit here:** the first version referenced a macro
`MLKEM512_KEY_SIZE_BITS` here — but that macro was only ever `#define`d
inside `mlkem512_keygen.c`, a *different translation unit*. C preprocessor
defines are file-local unless placed in a shared header, so this failed to
compile with `'MLKEM512_KEY_SIZE_BITS' undeclared here`. Fixed by using
`KEY_SIZE_BYTES_MLKEM512_PUB * 8` directly — a macro already defined in
*this* file from Part 4.1.

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

This was the hardest bug in the whole integration, and the eventual fix
involved a real architectural change, not a config tweak — worth recording
in full since the same class of bug will recur for any sufficiently
buffer-heavy crypto routine wired into OP-TEE core.

### 7.1 — First symptom

TA call to `TEE_GenerateKey()` panicked with `Core data-abort ...
(translation fault)`, deep inside `mlk_poly_rej_uniform_x4` (called from
`mlk_gen_matrix` → `mlk_indcpa_keypair_derand` → `mlkem_keypair_derand` →
our `crypto_acipher_gen_mlkem512_key`).

### 7.2 — Wrong hypothesis #1: per-thread stack

`CFG_CORE_THREAD_STACK_SIZE` was bumped (128, 256KB tried). **This had zero
effect** — because the syscall-dispatch call chain
(`el0_svc → thread_scall_handler → scall_do_call → syscall_obj_generate_key
→ ...`) does not run on the large per-thread stack at all. This was
confirmed by reading the actual entry assembly
(`core/arch/arm/kernel/thread_a64.S`), which shows the SVC/syscall path
switches `sp` to a *different*, much smaller stack region before dispatch.

### 7.3 — Correct hypothesis: the tmp stack

The real stack in use is `STACK_TMP_SIZE`
(`core/arch/arm/include/kernel/thread_private_arch.h`), default
`2048 + STACK_TMP_OFFS` bytes on AArch64 — overridable via
`CFG_STACK_TMP_EXTRA` (confirmed as a real, existing knob — an SE050 crypto
driver in-tree enforces a *minimum* of 8192 for exactly this reason, strong
precedent that "crypto library needs more tmp-stack headroom" is a known,
expected class of problem).

`CFG_STACK_TMP_EXTRA` was tried at 8192, then 16384, then 65536 — each
confirmed to actually be applied (`grep`-checked in the generated
`conf.mk`/`conf.h` after a **forced clean rebuild**, since Buildroot's
caching had silently no-op'd a config change at least once earlier in this
project) — and **the crash persisted identically at every value**,
confirmed via `addr2line`/`objdump` disassembly to be the exact same
instruction every time: `stp x0, x1, [sp, #8]`, immediately following
`sub sp, sp, #0xb60` (2912 bytes) in `mlk_poly_rej_uniform_x4`'s own
prologue.

**Combining a large `CFG_CORE_THREAD_STACK_SIZE` with a large
`CFG_STACK_TMP_EXTRA` at the same time caused the board to hang during
boot** (before reaching the login prompt) — very likely exhausting QEMU's
fixed TZDRAM secure-memory allocation once multiplied across `-smp 2`
(two CPUs) and multiple thread contexts. The thread-stack change was
reverted (it was never the actual problem) once this was understood.

### 7.4 — The real fix: move the buffers off the stack entirely

Rather than keep guessing at a "big enough" tmp-stack size, the actual fix
was to **stop allocating multi-kilobyte buffers on the stack in the first
place**, inside the vendored mlkem-native source itself:

**`core/mlkem_native/src/sampling.c`** — `mlk_poly_rej_uniform_x4()` was
refactored so its large local buffer (`buf[4][...]`, ~2.7KB) and its 4-way
parallel Keccak context (`statex`) are allocated via
`memalign(16, ...)` instead of as stack locals, with matching `free()` (and
`mlk_zeroize()` before free, to preserve the same "don't leave key material
lying around" property stack-based cleanup gave for free) on every exit
path including error paths.

**`core/mlkem_native/src/poly_k.c`** — `mlk_poly_getnoise_eta1_4x()`
received the identical treatment for its own large local buffers
(`buf[4][...]`, `extkey[4][...]`).

**`core/mlkem_native/src/polyvec.h`** — `__attribute__((aligned(16)))`
added directly to the `polyvec` struct definition, to guarantee correct
16-byte alignment for NEON vector loads/stores now that some data moved
off the (already-aligned-by-the-compiler) stack onto the heap.

**`core/mlkem_native/mlkem_native_config.h`** — with the root cause fixed,
`MLK_CONFIG_USE_NATIVE_BACKEND_FIPS202` (which had been temporarily
disabled during an earlier, incorrect bisection step, falling back to
portable C for Keccak) was re-enabled, restoring full AArch64 NEON
acceleration for both the arithmetic and hashing backends.

After this change, keygen completed successfully with **no stack-size
override needed at all** beyond OP-TEE's own defaults.

### 7.5 — Lesson for anyone hitting this class of bug

A `sub sp, sp, #<large>` in a disassembled function prologue, followed
immediately by a translation-fault on the very next store instruction, is
close to a definitive signature of a genuine stack overflow at that exact
call site — worth checking the actual disassembly (`objdump -d`,
cross-referenced with `addr2line`/`symbolize.py`) before assuming a config
knob alone will fix it, since the correct fix may be architectural
(reduce the frame size) rather than numerical (grow the stack further).

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
the **TA userspace** stack (separate from every stack discussed in Part 7,
which are all *kernel/core* stacks) — 4KB was sufficient here because,
after the Part 7 fix, none of the actual heavy computation happens with
large stack frames anymore; the TA side is just thin plumbing around the
syscall.

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
| `core/mlkem_native/src/` (new dir) | Vendored source tree, unmodified from upstream, except: |
| `core/mlkem_native/src/sampling.c` | `mlk_poly_rej_uniform_x4()` refactored: stack buffers → `memalign(16,...)` heap buffers |
| `core/mlkem_native/src/poly_k.c` | `mlk_poly_getnoise_eta1_4x()` refactored: same stack→heap treatment |
| `core/mlkem_native/src/polyvec.h` | Added `__attribute__((aligned(16)))` to the `polyvec` struct |
| `core/mlkem_native/mlkem_native_config.h` (new) | Parameter set 512, native backends on, backend file macros, no randomized API, custom namespace prefix |
| `core/mlkem_native/mlkem512_keygen.c` (new) | `crypto_acipher_alloc_mlkem512_keypair()`, `crypto_acipher_gen_mlkem512_key()` — VFP enable/disable, RNG fill, calls into `mlkem_keypair_derand()` |
| `core/mlkem_native/sub.mk` (new) | Build file list + per-file warning relaxation |
| `core/sub.mk` | Added `subdirs-y += mlkem_native` |
| `lib/libutee/include/tee_api_defines.h` | `TEE_TYPE_MLKEM512_PUBLIC_KEY`, `TEE_TYPE_MLKEM512_KEYPAIR`, `TEE_ATTR_MLKEM512_PUBLIC_VALUE`, `TEE_ATTR_MLKEM512_PRIVATE_VALUE` |
| `core/include/crypto/crypto.h` | `struct mlkem512_keypair`, 2 function prototypes |
| `core/tee/tee_svc_cryp.c` | Ops-index + key-size constants; 14 attribute-ops functions; `attr_ops[]` table entries; `tee_cryp_obj_mlkem512_keypair_attrs[]`; `PROP(TEE_TYPE_MLKEM512_KEYPAIR, ...)` entry; `tee_svc_obj_generate_key_mlkem512()`; dispatch case in `syscall_obj_generate_key()`; dispatch case in `tee_obj_set_type()` |
| `optee_examples/mlkem_test/` (new) | Full example TA/CA — see Part 8 |

**Reverted / not present in the final state:**
- `CFG_CORE_THREAD_STACK_SIZE` override in `core/arch/arm/plat-vexpress/conf.mk` — tried, found to be the wrong stack entirely, removed
- `CFG_STACK_TMP_EXTRA` override in the same file — tried at multiple values while the real bug was still present, ultimately unnecessary once Part 7's heap migration fixed the actual root cause, removed
- The hardcoded KAT seed in `mlkem512_keygen.c` — used only transiently for Part 9.2's verification, reverted immediately after

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
9. Build. If you hit a stack-related crash deep in `mlk_poly_rej_uniform_x4`
   or a similar function, go straight to Part 7's real fix (heap migration)
   rather than iterating on stack-size config values.
10. Build the test TA/CA from Part 8.
11. Verify using Part 9's KAT procedure before trusting the result.
