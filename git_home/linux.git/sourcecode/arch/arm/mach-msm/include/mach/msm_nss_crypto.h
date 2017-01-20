#ifndef __MSM_NSS_CRYPTO_H
#define __MSM_NSS_CRYPTO_H


/*
 * platform specific data for crypto H/W
 */
struct nss_crypto_platform_data {
	uint32_t crypto_pbase; /* crypto register space start */
	uint32_t crypto_pbase_sz; /* crypto register space size */
	uint32_t bam_pbase; /* crypto bam register space start */
	uint32_t bam_pbase_sz; /* crypto bam register space size */
	uint32_t bam_ee; /* crypto bam execution environment */
};


#endif /* __MSM_NSS_CRYPTO_H */
