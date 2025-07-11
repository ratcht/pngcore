/**
* @file pngcore_png.c
* @brief PNG parsing and creation operations
*/

#include "pngcore/pngcore_png.h"
#include "pngcore/pngcore_crc.h"
#include "pngcore/pngcore_zutil.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/******************************************************************************
* SIMPLE PNG OPERATIONS
*****************************************************************************/

pngcore_simple_png_t* pngcore_new_simple_png(void) {
  pngcore_simple_png_t *png = (pngcore_simple_png_t *)malloc(sizeof(pngcore_simple_png_t));
  if (!png) return NULL;
  
  png->ihdr = NULL;
  png->idat = NULL;
  png->iend = NULL;
  
  png->ihdr = (pngcore_ihdr_t *)malloc(sizeof(pngcore_ihdr_t));
  if (!png->ihdr) {
    free(png);
    return NULL;
  }
  png->ihdr->p_data = (pngcore_ihdr_data_t *)malloc(sizeof(pngcore_ihdr_data_t));
  if (!png->ihdr->p_data) {
    free(png->ihdr);
    free(png);
    return NULL;
  }
  
  png->idat = (pngcore_idat_t *)malloc(sizeof(pngcore_idat_t));
  if (!png->idat) {
    free(png->ihdr->p_data);
    free(png->ihdr);
    free(png);
    return NULL;
  }
  png->idat->p_data = (pngcore_idat_data_t *)malloc(sizeof(pngcore_idat_data_t));
  if (!png->idat->p_data) {
    free(png->idat);
    free(png->ihdr->p_data);
    free(png->ihdr);
    free(png);
    return NULL;
  }
  png->idat->p_data->data = NULL;
  png->idat->p_data->length = 0;
  
  png->iend = (pngcore_iend_t *)malloc(sizeof(pngcore_iend_t));
  if (!png->iend) {
    free(png->idat->p_data);
    free(png->idat);
    free(png->ihdr->p_data);
    free(png->ihdr);
    free(png);
    return NULL;
  }
  
  return png;
}

pngcore_simple_png_t* pngcore_parse_raw(pngcore_raw_png_t *raw_png, Error *error) {
  pngcore_simple_png_t *png = (pngcore_simple_png_t *)malloc(sizeof(pngcore_simple_png_t));
  if (!png) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate PNG structure");
    }
    return NULL;
  }
  
  /* Initialize as NULL */
  png->ihdr = NULL;
  png->idat = NULL;
  png->iend = NULL;
  
  /* Parse IHDR */
  png->ihdr = pngcore_parse_ihdr(raw_png->chunks[0], error);
  if (png->ihdr == NULL || error->code != SUCCESS) {
    if (error->code == ERR_CRC_MISMATCH) {
      /* Not fatal, continue */
      return png;
    } else {
      pngcore_free_simple_png(png);
      return NULL;
    }
  }
  
  /* Parse IDAT */
  png->idat = pngcore_parse_idat(raw_png->chunks[1], error);
  if (png->idat == NULL || error->code != SUCCESS) {
    if (error->code == ERR_CRC_MISMATCH) {
      /* Not fatal, continue */
      return png;
    } else {
      pngcore_free_simple_png(png);
      return NULL;
    }
  }
  
  /* Parse IEND */
  png->iend = pngcore_parse_iend(raw_png->chunks[2], error);
  if (png->iend == NULL || error->code != SUCCESS) {
    if (error->code == ERR_CRC_MISMATCH) {
      /* Not fatal, continue */
      return png;
    } else {
      pngcore_free_simple_png(png);
      return NULL;
    }
  }
  
  return png;
}

/******************************************************************************
* CHUNK PARSING
*****************************************************************************/

pngcore_ihdr_t* pngcore_parse_ihdr(pngcore_raw_chunk_t *raw_chunk, Error *error) {
  if (raw_chunk == NULL) {
    if (error) {
      error->code = ERR_WRONG_CHUNK;
      snprintf(error->message, sizeof(error->message), "IHDR chunk is NULL");
    }
    return NULL;
  }
  
  if (memcmp(raw_chunk->type, "IHDR", 4) != 0) {
    if (error) {
      error->code = ERR_WRONG_CHUNK;
      snprintf(error->message, sizeof(error->message), 
      "Expected IHDR chunk, got %.4s", raw_chunk->type);
    }
    return NULL;
  }
  
  pngcore_ihdr_t *ihdr = (pngcore_ihdr_t *)malloc(sizeof(pngcore_ihdr_t));
  if (!ihdr) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate IHDR");
    }
    return NULL;
  }
  
  ihdr->p_data = (pngcore_ihdr_data_t *)malloc(sizeof(pngcore_ihdr_data_t));
  if (!ihdr->p_data) {
    free(ihdr);
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate IHDR data");
    }
    return NULL;
  }
  
  /* Parse IHDR data */
  ihdr->p_data->width = from_be_buffer(raw_chunk->p_data);
  ihdr->p_data->height = from_be_buffer(raw_chunk->p_data + 4);
  ihdr->p_data->bit_depth = raw_chunk->p_data[8];
  ihdr->p_data->color_type = raw_chunk->p_data[9];
  ihdr->p_data->compression = raw_chunk->p_data[10];
  ihdr->p_data->filter = raw_chunk->p_data[11];
  ihdr->p_data->interlace = raw_chunk->p_data[12];
  
  ihdr->crc = raw_chunk->crc;
  
  /* Verify CRC */
  U32 total_len = 4 + raw_chunk->length;
  U8 *crc_buf = (unsigned char *)malloc(total_len);
  if (crc_buf) {
    memcpy(crc_buf, raw_chunk->type, 4);
    memcpy(crc_buf + 4, raw_chunk->p_data, raw_chunk->length);
    
    U32 computed_crc = pngcore_crc(crc_buf, total_len);
    free(crc_buf);
    
    if (computed_crc != ihdr->crc) {
      if (error) {
        error->code = ERR_CRC_MISMATCH;
        snprintf(error->message, sizeof(error->message),
        "IHDR chunk CRC error: computed %X, expected %X", 
        computed_crc, ihdr->crc);
      }
    }
  }
  
  return ihdr;
}

pngcore_idat_t* pngcore_parse_idat(pngcore_raw_chunk_t *raw_chunk, Error *error) {
  if (raw_chunk == NULL) {
    if (error) {
      error->code = ERR_WRONG_CHUNK;
      snprintf(error->message, sizeof(error->message), "IDAT chunk is NULL");
    }
    return NULL;
  }
  
  if (memcmp(raw_chunk->type, "IDAT", 4) != 0) {
    if (error) {
      error->code = ERR_WRONG_CHUNK;
      snprintf(error->message, sizeof(error->message), 
      "Expected IDAT chunk, got %.4s", raw_chunk->type);
    }
    return NULL;
  }
  
  pngcore_idat_t *idat = (pngcore_idat_t *)malloc(sizeof(pngcore_idat_t));
  if (!idat) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate IDAT");
    }
    return NULL;
  }
  
  /* Verify CRC first */
  U32 total_len = 4 + raw_chunk->length;
  U8 *crc_buf = (unsigned char *)malloc(total_len);
  if (crc_buf) {
    memcpy(crc_buf, raw_chunk->type, 4);
    memcpy(crc_buf + 4, raw_chunk->p_data, raw_chunk->length);
    
    U32 computed_crc = pngcore_crc(crc_buf, total_len);
    free(crc_buf);
    
    if (computed_crc != raw_chunk->crc) {
      if (error) {
        error->code = ERR_CRC_MISMATCH;
        snprintf(error->message, sizeof(error->message),
        "IDAT chunk CRC error: computed %X, expected %X", 
        computed_crc, raw_chunk->crc);
      }
    }
  }
  
  /* Copy data */
  idat->p_data = (pngcore_idat_data_t *)malloc(sizeof(pngcore_idat_data_t));
  if (!idat->p_data) {
    free(idat);
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate IDAT data");
    }
    return NULL;
  }
  
  idat->p_data->length = raw_chunk->length;
  idat->p_data->data = (U8 *)malloc(idat->p_data->length);
  if (!idat->p_data->data) {
    free(idat->p_data);
    free(idat);
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate IDAT buffer");
    }
    return NULL;
  }
  
  memcpy(idat->p_data->data, raw_chunk->p_data, raw_chunk->length);
  idat->crc = raw_chunk->crc;
  
  return idat;
}

pngcore_iend_t* pngcore_parse_iend(pngcore_raw_chunk_t *raw_chunk, Error *error) {
  if (raw_chunk == NULL) {
    if (error) {
      error->code = ERR_WRONG_CHUNK;
      snprintf(error->message, sizeof(error->message), "IEND chunk is NULL");
    }
    return NULL;
  }
  
  if (memcmp(raw_chunk->type, "IEND", 4) != 0) {
    if (error) {
      error->code = ERR_WRONG_CHUNK;
      snprintf(error->message, sizeof(error->message), 
      "Expected IEND chunk, got %.4s", raw_chunk->type);
    }
    return NULL;
  }
  
  pngcore_iend_t *iend = (pngcore_iend_t *)malloc(sizeof(pngcore_iend_t));
  if (!iend) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate IEND");
    }
    return NULL;
  }
  
  iend->crc = raw_chunk->crc;
  
  return iend;
}

/******************************************************************************
* CONVERSION TO RAW
*****************************************************************************/

pngcore_raw_chunk_t* pngcore_ihdr_to_raw(pngcore_ihdr_t *ihdr) {
  pngcore_raw_chunk_t *raw_ch = (pngcore_raw_chunk_t *)malloc(sizeof(pngcore_raw_chunk_t));
  if (!raw_ch) return NULL;
  
  raw_ch->length = DATA_IHDR_SIZE;
  memcpy(raw_ch->type, "IHDR", 4);
  
  raw_ch->p_data = (U8 *)malloc(DATA_IHDR_SIZE);
  if (!raw_ch->p_data) {
    free(raw_ch);
    return NULL;
  }
  
  /* Convert to big-endian and copy width/height */
  U32 width_be = htonl(ihdr->p_data->width);
  U32 height_be = htonl(ihdr->p_data->height);
  memcpy(raw_ch->p_data, &width_be, 4);
  memcpy(raw_ch->p_data + 4, &height_be, 4);
  
  /* Copy single-byte fields */
  raw_ch->p_data[8] = ihdr->p_data->bit_depth;
  raw_ch->p_data[9] = ihdr->p_data->color_type;
  raw_ch->p_data[10] = ihdr->p_data->compression;
  raw_ch->p_data[11] = ihdr->p_data->filter;
  raw_ch->p_data[12] = ihdr->p_data->interlace;
  
  /* Calculate CRC */
  U32 total_len = 4 + raw_ch->length;
  U8 *crc_buf = (unsigned char *)malloc(total_len);
  if (crc_buf) {
    memcpy(crc_buf, raw_ch->type, 4);
    memcpy(crc_buf + 4, raw_ch->p_data, raw_ch->length);
    raw_ch->crc = pngcore_crc(crc_buf, total_len);
    free(crc_buf);
  }
  
  return raw_ch;
}

pngcore_raw_chunk_t* pngcore_idat_to_raw(pngcore_idat_t *idat) {
  pngcore_raw_chunk_t *raw_ch = (pngcore_raw_chunk_t *)malloc(sizeof(pngcore_raw_chunk_t));
  if (!raw_ch) return NULL;
  
  raw_ch->length = idat->p_data->length;
  memcpy(raw_ch->type, "IDAT", 4);
  
  raw_ch->p_data = (U8 *)malloc(idat->p_data->length);
  if (!raw_ch->p_data) {
    free(raw_ch);
    return NULL;
  }
  
  memcpy(raw_ch->p_data, idat->p_data->data, idat->p_data->length);
  
  /* Calculate CRC */
  U32 total_len = 4 + raw_ch->length;
  U8 *crc_buf = (unsigned char *)malloc(total_len);
  if (crc_buf) {
    memcpy(crc_buf, raw_ch->type, 4);
    memcpy(crc_buf + 4, raw_ch->p_data, raw_ch->length);
    raw_ch->crc = pngcore_crc(crc_buf, total_len);
    free(crc_buf);
  }
  
  return raw_ch;
}

pngcore_raw_chunk_t* pngcore_iend_to_raw(pngcore_iend_t *iend) {
  pngcore_raw_chunk_t *raw_ch = (pngcore_raw_chunk_t *)malloc(sizeof(pngcore_raw_chunk_t));
  if (!raw_ch) return NULL;
  
  raw_ch->length = 0;
  memcpy(raw_ch->type, "IEND", 4);
  raw_ch->p_data = NULL;
  
  /* Calculate CRC for just the type */
  raw_ch->crc = pngcore_crc((unsigned char *)raw_ch->type, 4);
  
  return raw_ch;
}

pngcore_raw_png_t* pngcore_png_to_raw(pngcore_simple_png_t *png, Error *error) {
  if (png == NULL) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "PNG is NULL");
    }
    return NULL;
  }
  
  pngcore_raw_png_t *raw_png = (pngcore_raw_png_t *)malloc(sizeof(pngcore_raw_png_t));
  if (!raw_png) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate raw PNG");
    }
    return NULL;
  }
  
  raw_png->chunks[0] = pngcore_ihdr_to_raw(png->ihdr);
  raw_png->chunks[1] = pngcore_idat_to_raw(png->idat);
  raw_png->chunks[2] = pngcore_iend_to_raw(png->iend);
  
  return raw_png;
}

/******************************************************************************
* FILE I/O
*****************************************************************************/

void pngcore_write_png_file(const char *filename, pngcore_simple_png_t *png, Error *error) {
  if (png == NULL) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "PNG is NULL");
    }
    return;
  }
  
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), 
      "Failed to open file %s", filename);
    }
    return;
  }
  
  pngcore_raw_png_t *raw_png = pngcore_png_to_raw(png, error);
  if (raw_png == NULL || error->code != SUCCESS) {
    fclose(fp);
    return;
  }
  
  /* Write PNG signature */
  U8 png_sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  fwrite(png_sig, 1, sizeof(png_sig), fp);
  
  /* Write chunks */
  for (int i = 0; i < 3; i++) {
    pngcore_raw_chunk_t *chunk = raw_png->chunks[i];
    
    /* Write length (big-endian) */
    U32 len_be = htonl(chunk->length);
    fwrite(&len_be, 1, 4, fp);
    
    /* Write type */
    fwrite(chunk->type, 1, 4, fp);
    
    /* Write data if present */
    if (chunk->length > 0 && chunk->p_data != NULL) {
      fwrite(chunk->p_data, 1, chunk->length, fp);
    }
    
    /* Write CRC (big-endian) */
    U32 crc_be = htonl(chunk->crc);
    fwrite(&crc_be, 1, 4, fp);
  }
  
  pngcore_free_raw_png(raw_png);
  fclose(fp);
}

int pngcore_write_file(const char *path, const void *in, size_t len) {
  FILE *fp = NULL;
  
  if (path == NULL) {
    fprintf(stderr, "pngcore_write_file: file name is null!\n");
    return -1;
  }
  
  if (in == NULL) {
    fprintf(stderr, "pngcore_write_file: input data is null!\n");
    return -1;
  }
  
  fp = fopen(path, "wb");
  if (fp == NULL) {
    perror("fopen");
    return -2;
  }
  
  if (fwrite(in, 1, len, fp) != len) {
    fprintf(stderr, "pngcore_write_file: incomplete write!\n");
    fclose(fp);
    return -3;
  }
  
  return fclose(fp);
}

/******************************************************************************
* COMPRESSION OPERATIONS
*****************************************************************************/

ssize_t pngcore_inflate_idat(U8 *dest_buf, pngcore_raw_png_t *src_png) {
  U64 temp_dest_len = 0;
  U8 *idat_data = src_png->chunks[1]->p_data;
  U32 idat_len = src_png->chunks[1]->length;
  
  /* Inflate IDAT data */
  int ret = pngcore_mem_inflate(dest_buf, &temp_dest_len, idat_data, idat_len);
  if (ret != 0) {
    fprintf(stderr, "pngcore_mem_inflate failed. ret = %d.\n", ret);
    return -1;
  }
  return temp_dest_len;
}

ssize_t pngcore_deflate_idat(U8 *src_buf, U64 src_len, pngcore_simple_png_t *dest_png) {
  /* Compress data */
  U8 *dest_buf = malloc(src_len);
  if (!dest_buf) {
    return -1;
  }
  
  U64 dest_len = 0;
  
  int ret = pngcore_mem_deflate(dest_buf, &dest_len, src_buf, src_len, Z_DEFAULT_COMPRESSION);
  if (ret != 0) {
    fprintf(stderr, "pngcore_mem_deflate failed. ret = %d.\n", ret);
    free(dest_buf);
    return -1;
  }
  
  /* Resize buffer to actual size */
  U8 *tmp = realloc(dest_buf, dest_len);
  if (!tmp && dest_len != 0) {
    free(dest_buf);
    return -1;
  }
  dest_buf = tmp;
  
  /* Set IDAT data */
  if (dest_png->idat->p_data->data) {
    free(dest_png->idat->p_data->data);
  }
  dest_png->idat->p_data->length = dest_len;
  dest_png->idat->p_data->data = dest_buf;
  
  return dest_len;
}

/******************************************************************************
* MEMORY MANAGEMENT
*****************************************************************************/

void pngcore_free_simple_png(pngcore_simple_png_t *png) {
  if (png == NULL) {
    return;
  }
  if (png->ihdr != NULL) {
    pngcore_free_ihdr(png->ihdr);
  }
  if (png->idat != NULL) {
    pngcore_free_idat(png->idat);
  }
  if (png->iend != NULL) {
    pngcore_free_iend(png->iend);
  }
  free(png);
}

void pngcore_free_ihdr(pngcore_ihdr_t *ihdr) {
  if (ihdr == NULL) {
    return;
  }
  if (ihdr->p_data != NULL) {
    free(ihdr->p_data);
  }
  free(ihdr);
}

void pngcore_free_idat(pngcore_idat_t *idat) {
  if (idat == NULL) {
    return;
  }
  if (idat->p_data != NULL) {
    if (idat->p_data->data != NULL) {
      free(idat->p_data->data);
    }
    free(idat->p_data);
  }
  free(idat);
}

void pngcore_free_iend(pngcore_iend_t *iend) {
  if (iend == NULL) {
    return;
  }
  free(iend);
}