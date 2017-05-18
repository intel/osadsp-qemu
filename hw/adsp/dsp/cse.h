#ifndef __CSE_H__
#define __CSE_H__

#include <stdint.h>

#define CSE_HEADER_MAKER   0x44504324	//"DPC$"

struct CsePartitionDirHeader
{
	uint32_t header_marker;
	uint32_t nb_entries;
	uint8_t  header_version;
	uint8_t  entry_version;
	uint8_t  header_length;
	uint8_t  checksum;
	uint8_t  partition_name[4];
};

struct CsePartitionDirEntry
{
	uint8_t  entry_name[12];
	uint32_t offset; /* b0..b24 = offset | b25 = Huffman | b26..b31=reserved */
	uint32_t length;
	uint32_t reserved;
};

#endif
