/**
* @file pngcore_raw.c
* @brief Raw PNG chunk handling
*/

#include "pngcore/pngcore_png.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int pngcore_is_png_buf(U8 *buf, size_t buf_size, size_t offset) {
  if (buf == NULL || offset + PNG_SIG_SIZE > buf_size) {
    return 0;
  }
  return is_png(buf + offset, PNG_SIG_SIZE);
}

pngcore_raw_png_t* pngcore_load_raw_png(U8 *buf, size_t buf_size, size_t offset, Error* error) {
  if (buf == NULL) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Buffer is NULL");
    }
    return NULL;
  }
  
  /* Allocate memory for raw_PNG struct */
  pngcore_raw_png_t *raw_png = (pngcore_raw_png_t *)malloc(sizeof(pngcore_raw_png_t));
  if (!raw_png) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate raw PNG");
    }
    return NULL;
  }
  
  if (offset + PNG_SIG_SIZE > buf_size) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Buffer too small for PNG signature");
    }
    free(raw_png);
    return NULL;
  }
  
  if (!is_png(buf + offset, PNG_SIG_SIZE)) {
    if (error) {
      error->code = ERR_NOT_A_PNG;
      snprintf(error->message, sizeof(error->message), "Not a PNG file");
    }
    free(raw_png);
    return NULL;
  }
  
  /* Load chunks starting after PNG signature */
  size_t i_offset = offset + PNG_SIG_SIZE;
  for (int i = 0; i < 3; i++) {
    raw_png->chunks[i] = pngcore_load_raw_chunk(buf, buf_size, i_offset, error);
    if (raw_png->chunks[i] == NULL) {
      if (error && error->code == SUCCESS) {
        error->code = ERR;
        snprintf(error->message, sizeof(error->message), "Error loading chunk %d", i);
      }
      /* Clean up previously allocated chunks */
      for (int j = 0; j < i; j++) {
        pngcore_free_raw_chunk(raw_png->chunks[j]);
      }
      free(raw_png);
      return NULL;
    }
    
    /* Move offset to next chunk */
    size_t chunk_size = CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE + 
    raw_png->chunks[i]->length + CHUNK_CRC_SIZE;
    i_offset += chunk_size;
  }
  
  return raw_png;
}

pngcore_raw_chunk_t* pngcore_load_raw_chunk(U8 *buf, size_t buf_size, size_t offset, Error *error) {
  if (buf == NULL) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Buffer is NULL");
    }
    return NULL;
  }
  
  /* Check if we have enough space for chunk header */
  if (offset + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE > buf_size) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Buffer too small for chunk header");
    }
    return NULL;
  }
  
  pngcore_raw_chunk_t *raw_ch = (pngcore_raw_chunk_t *)malloc(sizeof(pngcore_raw_chunk_t));
  if (raw_ch == NULL) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Failed to allocate memory for chunk");
    }
    return NULL;
  }
  
  size_t current_offset = offset;
  
  /* Read length (4 bytes, big-endian) */
  raw_ch->length = from_be_buffer(buf + current_offset);
  current_offset += CHUNK_LEN_SIZE;
  
  /* Read type (4 bytes) */
  memcpy(raw_ch->type, buf + current_offset, CHUNK_TYPE_SIZE);
  current_offset += CHUNK_TYPE_SIZE;
  
  /* Check if we have enough space for data + CRC */
  if (current_offset + raw_ch->length + CHUNK_CRC_SIZE > buf_size) {
    if (error) {
      error->code = ERR;
      snprintf(error->message, sizeof(error->message), "Buffer too small for chunk data and CRC");
    }
    free(raw_ch);
    return NULL;
  }
  
  /* Read data */
  if (raw_ch->length > 0) {
    raw_ch->p_data = (U8 *)malloc(raw_ch->length);
    if (raw_ch->p_data == NULL) {
      if (error) {
        error->code = ERR;
        snprintf(error->message, sizeof(error->message), "Failed to allocate memory for chunk data");
      }
      free(raw_ch);
      return NULL;
    }
    memcpy(raw_ch->p_data, buf + current_offset, raw_ch->length);
  } else {
    raw_ch->p_data = NULL;
  }
  current_offset += raw_ch->length;
  
  /* Read CRC (4 bytes, big-endian) */
  raw_ch->crc = from_be_buffer(buf + current_offset);
  
  return raw_ch;
}

void pngcore_free_raw_png(pngcore_raw_png_t *raw_png) {
  if (raw_png == NULL) {
    return;
  }
  for (int i = 0; i < 3; i++) {
    if (raw_png->chunks[i] != NULL) {
      pngcore_free_raw_chunk(raw_png->chunks[i]);
    }
  }
  free(raw_png);
}

void pngcore_free_raw_chunk(pngcore_raw_chunk_t *raw_chunk) {
  if (raw_chunk == NULL) {
    return;
  }
  if (raw_chunk->p_data != NULL) {
    free(raw_chunk->p_data);
  }
  free(raw_chunk);
}