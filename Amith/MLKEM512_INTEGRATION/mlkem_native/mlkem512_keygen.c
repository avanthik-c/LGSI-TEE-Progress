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
/* key_size is expressed in BITS by the GP API; this is what
 * TEE_AllocateTransientObject()'s caller must pass */
#define MLKEM512_KEY_SIZE_BITS UL(MLKEM512_PK_SIZE * 8)

TEE_Result crypto_acipher_alloc_mlkem512_keypair(struct mlkem512_keypair *s,
						 size_t key_size_bits)
{
	if (!s || key_size_bits != MLKEM512_KEY_SIZE_BITS)
		return TEE_ERROR_BAD_PARAMETERS;

	memset(s, 0, sizeof(*s));

	/* Force 16-byte alignment for NEON vector instructions */
	s->pub = memalign(16, MLKEM512_PK_SIZE);
	s->priv = memalign(16, MLKEM512_SK_SIZE);

	if (!s->pub || !s->priv) {
		free(s->pub);
		free(s->priv);
		return TEE_ERROR_OUT_OF_MEMORY;
	}

	/* memalign doesn't zero memory like calloc does */
	memset(s->pub, 0, MLKEM512_PK_SIZE);
	memset(s->priv, 0, MLKEM512_SK_SIZE);

	return TEE_SUCCESS;
}

TEE_Result crypto_acipher_gen_mlkem512_key(struct mlkem512_keypair *key,
					   size_t key_size_bits)
{
	/* Force 16-byte alignment on the local stack array */
	uint8_t 
coins[2 * MLKEM_SYMBYTES] __attribute__((aligned(16)));
	int mlk_rc;
	uint32_t vfp_state;

	if (key_size_bits != MLKEM512_KEY_SIZE_BITS)
		return TEE_ERROR_BAD_PARAMETERS;

	/* crypto_rng_read() is the kernel-side RNG - the same one that
	 * backs TEE_GenerateRandom() from TA userspace. This IS the real
	 * TEE hardware/software RNG, not a userspace convenience wrapper. */
	
	// Uncomment below to restore randomness!!!!.
	//if (crypto_rng_read(coins, sizeof(coins)) != TEE_SUCCESS)
	//	return TEE_ERROR_BAD_STATE;
	
	/* --- TEMPORARY: hardcoded NIST/mlkem-native KAT seed for verification --- */
	static const uint8_t kat_coins[64] = {
		0x93, 0x4d, 0x60, 0xb3, 0x56, 0x24, 0xd7, 0x40, 0xb3, 0x0a, 0x7f, 0x22,
		0x7a, 0xf2, 0xae, 0x7c, 0x67, 0x8e, 0x4e, 0x04, 0xe1, 0x3c, 0x5f, 0x50,
		0x9e, 0xad, 0xe2, 0xb7, 0x9a, 0xea, 0x77, 0xe2, 0x3e, 0x2a, 0x2e, 0xa6,
		0xc9, 0xc4, 0x76, 0xfc, 0x49, 0x37, 0xb0, 0x13, 0xc9, 0x93, 0xa7, 0x93,
		0xd6, 0xc0, 0xab, 0x99, 0x60, 0x69, 0x5b, 0xa8, 0x38, 0xf6, 0x49, 0xda,
		0x53, 0x9c, 0xa3, 0xd0
	};
	memcpy(coins, kat_coins, sizeof(coins));
	/* --- END TEMPORARY --- */
	
	
	/* Enable NEON/FPU instructions for the kernel thread */
	vfp_state = thread_kernel_enable_vfp();

	mlk_rc = mlkem_keypair_derand(key->pub, key->priv, coins);

	/* Immediately disable/restore NEON state */
	thread_kernel_disable_vfp(vfp_state);

	memzero_explicit(coins, sizeof(coins));

	if (mlk_rc != 0) {
		EMSG("mlkem_keypair_derand failed: %d", mlk_rc);
		return TEE_ERROR_BAD_STATE;
	}

	return TEE_SUCCESS;
}
