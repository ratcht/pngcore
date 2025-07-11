/**
* @file pngcore_png.h
* @brief PNG structure definitions and operations
*/

#ifndef PNGCORE_PNG_H
#define PNGCORE_PNG_H

#include "pngcore_types.h"
#include <stdio.h>

/******************************************************************************
* RAW STRUCTURES (Unparsed)
*****************************************************************************/

typedef struct {
  U32 length;  /* length of data in the chunk, host byte order */
  U8  type[4]; /* chunk type */
  U8  *p_data; /* pointer to location where the actual data are */
  U32 crc;     /* CRC field */
} pngcore_raw_chunk_t;

typedef struct {
  pngcore_raw_chunk_t *chunks[3]; /* 0: IHDR, 1: IDAT, 2: IEND */
} pngcore_raw_png_t;

/******************************************************************************
* PARSED STRUCTURES
*****************************************************************************/

/* IHDR data */
typedef struct {
  U32 width;        /* width in pixels, big endian */
  U32 height;       /* height in pixels, big endian */
  U8  bit_depth;    /* num of bits per sample or per palette index */
  U8  color_type;   /* 0: Grayscale; 2: Truecolor; 3: Indexed-color
  4: Greyscale with alpha; 6: Truecolor with alpha */
  U8  compression;  /* only method 0 is defined for now */
  U8  filter;       /* only method 0 is defined for now */
  U8  interlace;    /* 0: no interlace; 1: Adam7 interlace */
} pngcore_ihdr_data_t;

typedef struct {
  pngcore_ihdr_data_t *p_data;
  U32 crc;
} pngcore_ihdr_t;

/* IDAT data */
typedef struct {
  U32 length;  /* length of data in the chunk */
  U8  *data;   /* compressed image data */
} pngcore_idat_data_t;

typedef struct {
  pngcore_idat_data_t *p_data;
  U32 crc;
} pngcore_idat_t;

/* IEND */
typedef struct {
  U32 crc;
} pngcore_iend_t;

/* Complete PNG structure */
typedef struct {
  pngcore_ihdr_t *ihdr;
  pngcore_idat_t *idat;  /* only handles one IDAT chunk */
  pngcore_iend_t *iend;
} pngcore_simple_png_t;

/* Public PNG structure */
struct pngcore_png {
  pngcore_simple_png_t *internal;
  void *user_data;
};

/* Public chunk structure */
struct pngcore_chunk {
  pngcore_raw_chunk_t *internal;
};

/******************************************************************************
* RAW PNG FUNCTIONS
*****************************************************************************/

/* PNG validation */
static inline int is_png(U8 *buf, size_t n) {
  if (n < PNG_SIG_SIZE) {
    return 0;
  }
  U64 buf_signature = 0x00;
  for (int i = 0; i < PNG_SIG_SIZE; i++) {
    buf_signature = (buf_signature << 8) | buf[i];
  }
  return buf_signature == PNG_SIG ? 1 : 0;
}

/* Raw PNG operations */
int pngcore_is_png_buf(U8 *buf, size_t buf_size, size_t offset);
pngcore_raw_png_t* pngcore_load_raw_png(U8 *buf, size_t buf_size, size_t offset, Error *error);
pngcore_raw_chunk_t* pngcore_load_raw_chunk(U8 *buf, size_t buf_size, size_t offset, Error *error);
void pngcore_free_raw_png(pngcore_raw_png_t *raw_png);
void pngcore_free_raw_chunk(pngcore_raw_chunk_t *raw_chunk);

/******************************************************************************
* PARSED PNG FUNCTIONS
*****************************************************************************/

/* Creation and parsing */
pngcore_simple_png_t* pngcore_new_simple_png(void);
pngcore_simple_png_t* pngcore_parse_raw(pngcore_raw_png_t *raw_png, Error *error);

/* Chunk parsing */
pngcore_ihdr_t* pngcore_parse_ihdr(pngcore_raw_chunk_t *raw_chunk, Error *error);
pngcore_idat_t* pngcore_parse_idat(pngcore_raw_chunk_t *raw_chunk, Error *error);
pngcore_iend_t* pngcore_parse_iend(pngcore_raw_chunk_t *raw_chunk, Error *error);

/* Conversion back to raw */
pngcore_raw_chunk_t* pngcore_ihdr_to_raw(pngcore_ihdr_t *ihdr);
pngcore_raw_chunk_t* pngcore_idat_to_raw(pngcore_idat_t *idat);
pngcore_raw_chunk_t* pngcore_iend_to_raw(pngcore_iend_t *iend);
pngcore_raw_png_t* pngcore_png_to_raw(pngcore_simple_png_t *png, Error *error);

/* File I/O */
void pngcore_write_png_file(const char *filename, pngcore_simple_png_t *png, Error *error);
int pngcore_write_file(const char *path, const void *in, size_t len);

/* Memory management */
void pngcore_free_simple_png(pngcore_simple_png_t *png);
void pngcore_free_ihdr(pngcore_ihdr_t *ihdr);
void pngcore_free_idat(pngcore_idat_t *idat);
void pngcore_free_iend(pngcore_iend_t *iend);

/* IDAT operations */
ssize_t pngcore_inflate_idat(U8 *dest_buf, pngcore_raw_png_t *src_png);
ssize_t pngcore_deflate_idat(U8 *src_buf, U64 src_len, pngcore_simple_png_t *dest_png);

#endif /* PNGCORE_PNG_H */