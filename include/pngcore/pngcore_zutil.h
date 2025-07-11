/**
* @file pngcore_zutil.h
* @brief In-memory deflation and inflation using zlib
* Based on the zlib example zpipe.c
*/

#ifndef PNGCORE_ZUTIL_H
#define PNGCORE_ZUTIL_H

#include "pngcore_types.h"
#include <zlib.h>

/* Compression chunk size */
#define PNGCORE_ZLIB_CHUNK 16384  /* 16K */

/* Compression/decompression functions */
int pngcore_mem_deflate(U8 *dest, U64 *dest_len, U8 *source, U64 source_len, int level);
int pngcore_mem_inflate(U8 *dest, U64 *dest_len, U8 *source, U64 source_len);
void pngcore_zerr(int ret);

#endif /* PNGCORE_ZUTIL_H */