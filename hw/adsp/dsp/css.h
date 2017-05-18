
#ifndef __CSS_H__
#define __CSS_H__

#include <stdint.h>


/*
 * RSA Key and Crypto
 */
#define MAN_RSA_KEY_MODULUS_LEN 	256
#define MAN_RSA_KEY_EXPONENT_LEN 	4
#define MAN_RSA_SIGNATURE_LEN 		256

struct fw_version {
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t hotfix_version;
	uint16_t build_version;
} __attribute__((packed));

struct css_header {
	uint32_t header_type;
	uint32_t header_len;
	uint32_t header_version;
	uint32_t reserved0;
	uint32_t module_vendor;
	uint32_t date;
	uint32_t size;
	uint8_t header_id[4];
	uint32_t padding;
	struct fw_version version;
	uint32_t svn;
	uint32_t reserved[18];
	uint32_t modulus_size;
	uint32_t exponent_size;
	uint8_t modulus[MAN_RSA_KEY_MODULUS_LEN];
	uint8_t exponent[MAN_RSA_KEY_EXPONENT_LEN];
	//RSA of entire manifest - modulus/exponent/signature
	uint8_t signature[MAN_RSA_SIGNATURE_LEN];
} __attribute__((packed));

#endif
