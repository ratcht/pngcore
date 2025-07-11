/**
* @file pngcore_zutil.c
* @brief In-memory deflation and inflation routines
* Based on the zlib example zpipe.c
*/

#include "pngcore/pngcore_zutil.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
* @brief Deflate in memory data from source to dest
*/
int pngcore_mem_deflate(U8 *dest, U64 *dest_len, U8 *source, U64 source_len, int level) {
  z_stream strm;              /* zlib stream structure */
  U8 out[PNGCORE_ZLIB_CHUNK];  /* output buffer */
  int ret = 0;                /* zlib return code */
  int have = 0;               /* amount of data returned from deflate() */
  int def_len = 0;            /* accumulated deflated data length */
  U8 *p_dest = dest;          /* current position in dest buffer */
  
  /* Initialize deflate */
  strm.zalloc = Z_NULL;
  strm.zfree  = Z_NULL;
  strm.opaque = Z_NULL;
  
  ret = deflateInit(&strm, level);
  if (ret != Z_OK) {
    return ret;
  }
  
  /* Set input data stream */
  strm.avail_in = source_len;
  strm.next_in = source;
  
  /* Compress until end of data */
  do {
    strm.avail_out = PNGCORE_ZLIB_CHUNK;
    strm.next_out = out;
    ret = deflate(&strm, Z_FINISH);
    assert(ret != Z_STREAM_ERROR);
    have = PNGCORE_ZLIB_CHUNK - strm.avail_out;
    memcpy(p_dest, out, have);
    p_dest += have;
    def_len += have;
  } while (strm.avail_out == 0);
  
  assert(strm.avail_in == 0);
  assert(ret == Z_STREAM_END);
  
  /* Clean up and return */
  (void) deflateEnd(&strm);
  *dest_len = def_len;
  return Z_OK;
}

/**
* @brief Inflate in memory data from source to dest
*/
int pngcore_mem_inflate(U8 *dest, U64 *dest_len, U8 *source, U64 source_len) {
  z_stream strm;              /* zlib stream structure */
  U8 out[PNGCORE_ZLIB_CHUNK];  /* output buffer */
  int ret = 0;                /* zlib return code */
  int have = 0;               /* amount of data returned from inflate() */
  int inf_len = 0;            /* accumulated inflated data length */
  U8 *p_dest = dest;          /* current position in dest buffer */
  
  /* Initialize inflate */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  ret = inflateInit(&strm);
  if (ret != Z_OK) {
    return ret;
  }
  
  /* Set input data stream */
  strm.avail_in = source_len;
  strm.next_in = source;
  
  /* Run inflate() on input until output buffer not full */
  do {
    strm.avail_out = PNGCORE_ZLIB_CHUNK;
    strm.next_out = out;
    
    ret = inflate(&strm, Z_NO_FLUSH);
    assert(ret != Z_STREAM_ERROR);
    switch(ret) {
      case Z_NEED_DICT:
      ret = Z_DATA_ERROR;
      /* fall through */
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
      (void) inflateEnd(&strm);
      return ret;
    }
    have = PNGCORE_ZLIB_CHUNK - strm.avail_out;
    memcpy(p_dest, out, have);
    p_dest += have;
    inf_len += have;
  } while (strm.avail_out == 0);
  
  /* Clean up and return */
  (void) inflateEnd(&strm);
  *dest_len = inf_len;
  
  return (ret == Z_STREAM_END) ? Z_OK : Z_DATA_ERROR;
}

/* Report a zlib or i/o error */
void pngcore_zerr(int ret) {
  fputs("pngcore_zutil: ", stderr);
  switch (ret) {
    case Z_STREAM_ERROR:
    fputs("invalid compression level\n", stderr);
    break;
    case Z_DATA_ERROR:
    fputs("invalid or incomplete deflate data\n", stderr);
    break;
    case Z_MEM_ERROR:
    fputs("out of memory\n", stderr);
    break;
    case Z_VERSION_ERROR:
    fputs("zlib version mismatch!\n", stderr);
    break;
    default:
    fprintf(stderr, "zlib returns err %d!\n", ret);
  }
}