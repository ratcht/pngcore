/**
* @file pngcore_core.c
* @brief Core API implementation for libpngcore
*/

#include "pngcore.h"
#include "pngcore/pngcore_types.h"
#include "pngcore/pngcore_png.h"
#include "pngcore/pngcore_zutil.h"
#include "pngcore/pngcore_crc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Error string table */
static const char* error_strings[] = {
  [PNGCORE_SUCCESS] = "Success",
  [PNGCORE_ERR] = "General error",
  [PNGCORE_ERR_NOT_PNG] = "Not a PNG file",
  [PNGCORE_ERR_CRC_MISMATCH] = "CRC mismatch",
  [PNGCORE_ERR_NOT_IMPLEMENTED] = "Not implemented",
  [PNGCORE_ERR_WRONG_CHUNK] = "Wrong chunk type",
  [PNGCORE_ERR_MEMORY] = "Memory allocation failed",
  [PNGCORE_ERR_IO] = "I/O error",
  [PNGCORE_ERR_NETWORK] = "Network error"
};

/* Convert internal error codes to public API codes */
static pngcore_error_code_t convert_error_code(ErrorCode internal_code) {
  switch (internal_code) {
    case SUCCESS: return PNGCORE_SUCCESS;
    case ERR_NOT_A_PNG: return PNGCORE_ERR_NOT_PNG;
    case ERR_CRC_MISMATCH: return PNGCORE_ERR_CRC_MISMATCH;
    case ERR_NOT_IMPLEMENTED: return PNGCORE_ERR_NOT_IMPLEMENTED;
    case ERR_WRONG_CHUNK: return PNGCORE_ERR_WRONG_CHUNK;
    default: return PNGCORE_ERR;
  }
}

/******************************************************************************
* PNG LOADING AND SAVING
*****************************************************************************/

pngcore_png_t* pngcore_load_file(const char *filename, pngcore_error_t *error) {
  if (!filename) {
    if (error) {
      error->code = PNGCORE_ERR_IO;
      snprintf(error->message, sizeof(error->message), "Filename is NULL");
    }
    return NULL;
  }
  
  /* Open file */
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    if (error) {
      error->code = PNGCORE_ERR_IO;
      snprintf(error->message, sizeof(error->message), 
      "Failed to open file: %s", filename);
    }
    return NULL;
  }
  
  /* Get file size */
  fseek(fp, 0, SEEK_END);
  size_t file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  
  /* Allocate buffer */
  uint8_t *buffer = malloc(file_size);
  if (!buffer) {
    fclose(fp);
    if (error) {
      error->code = PNGCORE_ERR_MEMORY;
      snprintf(error->message, sizeof(error->message), 
      "Failed to allocate %zu bytes", file_size);
    }
    return NULL;
  }
  
  /* Read file */
  size_t bytes_read = fread(buffer, 1, file_size, fp);
  fclose(fp);
  
  if (bytes_read != file_size) {
    free(buffer);
    if (error) {
      error->code = PNGCORE_ERR_IO;
      snprintf(error->message, sizeof(error->message), 
      "Failed to read complete file");
    }
    return NULL;
  }
  
  /* Parse PNG */
  pngcore_png_t *png = pngcore_load_buffer(buffer, file_size, error);
  free(buffer);
  
  return png;
}

pngcore_png_t* pngcore_load_buffer(const uint8_t *buffer, size_t size, pngcore_error_t *error) {
  if (!buffer || size == 0) {
    if (error) {
      error->code = PNGCORE_ERR;
      snprintf(error->message, sizeof(error->message), 
      "Invalid buffer or size");
    }
    return NULL;
  }
  
  /* Parse raw PNG */
  Error internal_error = {SUCCESS, ""};
  pngcore_raw_png_t *raw = pngcore_load_raw_png((U8*)buffer, size, 0, &internal_error);
  
  if (!raw) {
    if (error) {
      error->code = convert_error_code(internal_error.code);
      strncpy(error->message, internal_error.message, sizeof(error->message)-1);
    }
    return NULL;
  }
  
  /* Parse to structured PNG */
  pngcore_simple_png_t *simple = pngcore_parse_raw(raw, &internal_error);
  pngcore_free_raw_png(raw);
  
  if (!simple) {
    if (error) {
      error->code = convert_error_code(internal_error.code);
      strncpy(error->message, internal_error.message, sizeof(error->message)-1);
    }
    return NULL;
  }
  
  /* Create wrapper */
  pngcore_png_t *png = calloc(1, sizeof(pngcore_png_t));
  if (!png) {
    pngcore_free_simple_png(simple);
    if (error) {
      error->code = PNGCORE_ERR_MEMORY;
      snprintf(error->message, sizeof(error->message), 
      "Failed to allocate PNG structure");
    }
    return NULL;
  }
  
  png->internal = simple;
  return png;
}

int pngcore_save_file(const pngcore_png_t *png, const char *filename, pngcore_error_t *error) {
  if (!png || !filename) {
    if (error) {
      error->code = PNGCORE_ERR;
      snprintf(error->message, sizeof(error->message), 
      "Invalid PNG or filename");
    }
    return -1;
  }
  
  Error internal_error = {SUCCESS, ""};
  pngcore_write_png_file(filename, png->internal, &internal_error);
  
  if (internal_error.code != SUCCESS) {
    if (error) {
      error->code = convert_error_code(internal_error.code);
      strncpy(error->message, internal_error.message, sizeof(error->message)-1);
    }
    return -1;
  }
  
  return 0;
}

/******************************************************************************
* PNG CREATION
*****************************************************************************/

pngcore_png_t* pngcore_create(uint32_t width, uint32_t height, uint8_t bit_depth, uint8_t color_type) {
  pngcore_png_t *png = calloc(1, sizeof(pngcore_png_t));
  if (!png) return NULL;
  
  /* Create internal PNG structure */
  pngcore_simple_png_t *simple = pngcore_new_simple_png();
  if (!simple) {
    free(png);
    return NULL;
  }
  
  /* Set properties */
  simple->ihdr->p_data->width = width;
  simple->ihdr->p_data->height = height;
  simple->ihdr->p_data->bit_depth = bit_depth;
  simple->ihdr->p_data->color_type = color_type;
  simple->ihdr->p_data->compression = 0;
  simple->ihdr->p_data->filter = 0;
  simple->ihdr->p_data->interlace = 0;
  
  png->internal = simple;
  return png;
}

/******************************************************************************
* PNG PROPERTIES
*****************************************************************************/

uint32_t pngcore_get_width(const pngcore_png_t *png) {
  return (png && png->internal && png->internal->ihdr) ? 
  png->internal->ihdr->p_data->width : 0;
}

uint32_t pngcore_get_height(const pngcore_png_t *png) {
  return (png && png->internal && png->internal->ihdr) ? 
  png->internal->ihdr->p_data->height : 0;
}

uint8_t pngcore_get_bit_depth(const pngcore_png_t *png) {
  return (png && png->internal && png->internal->ihdr) ? 
  png->internal->ihdr->p_data->bit_depth : 0;
}

uint8_t pngcore_get_color_type(const pngcore_png_t *png) {
  return (png && png->internal && png->internal->ihdr) ? 
  png->internal->ihdr->p_data->color_type : 0;
}

/******************************************************************************
* DATA ACCESS
*****************************************************************************/

int pngcore_get_raw_data(const pngcore_png_t *png, uint8_t **data, size_t *size) {
  if (!png || !png->internal || !png->internal->idat || !data || !size) {
    return -1;
  }
  
  /* Calculate uncompressed size */
  uint32_t width = pngcore_get_width(png);
  uint32_t height = pngcore_get_height(png);
  uint8_t color_type = pngcore_get_color_type(png);
  uint8_t bit_depth = pngcore_get_bit_depth(png);
  
  /* Calculate bytes per pixel */
  int channels = 1;
  switch (color_type) {
    case PNGCORE_COLOR_RGB: channels = 3; break;
    case PNGCORE_COLOR_RGBA: channels = 4; break;
    case PNGCORE_COLOR_GRAYSCALE_ALPHA: channels = 2; break;
  }
  
  /* Calculate expected size (including filter bytes) */
  size_t row_bytes = width * channels * (bit_depth / 8);
  size_t expected_size = height * (row_bytes + 1); /* +1 for filter byte */
  
  /* Allocate buffer */
  *data = malloc(expected_size);
  if (!*data) {
    return -1;
  }
  
  /* Create temporary raw PNG for inflation */
  pngcore_raw_png_t temp_raw;
  pngcore_raw_chunk_t temp_chunk;
  temp_chunk.p_data = png->internal->idat->p_data->data;
  temp_chunk.length = png->internal->idat->p_data->length;
  temp_raw.chunks[1] = &temp_chunk;
  
  /* Inflate data */
  ssize_t inflated_size = pngcore_inflate_idat(*data, &temp_raw);
  if (inflated_size < 0) {
    free(*data);
    *data = NULL;
    return -1;
  }
  
  *size = inflated_size;
  return 0;
}

int pngcore_set_raw_data(pngcore_png_t *png, const uint8_t *data, size_t size) {
  if (!png || !png->internal || !data || size == 0) {
    return -1;
  }
  
  /* Deflate data */
  ssize_t result = pngcore_deflate_idat((U8*)data, size, png->internal);
  return (result >= 0) ? 0 : -1;
}

/******************************************************************************
* MEMORY MANAGEMENT
*****************************************************************************/

void pngcore_free(pngcore_png_t *png) {
  if (png) {
    if (png->internal) {
      pngcore_free_simple_png(png->internal);
    }
    free(png);
  }
}

/******************************************************************************
* VALIDATION
*****************************************************************************/

int pngcore_is_png_buffer(const uint8_t *buf, size_t size) {
  return pngcore_is_png_buf((U8*)buf, size, 0);
}

int pngcore_validate(const pngcore_png_t *png) {
  if (!png || !png->internal) {
    return 0;
  }
  
  /* Check basic structure */
  if (!png->internal->ihdr || !png->internal->idat || !png->internal->iend) {
    return 0;
  }
  
  /* Validate IHDR values */
  pngcore_ihdr_data_t *ihdr = png->internal->ihdr->p_data;
  if (ihdr->width == 0 || ihdr->height == 0) {
    return 0;
  }
  
  /* Validate bit depth */
  switch (ihdr->bit_depth) {
    case 1: case 2: case 4: case 8: case 16:
    break;
    default:
    return 0;
  }
  
  /* Validate color type */
  switch (ihdr->color_type) {
    case 0: case 2: case 3: case 4: case 6:
    break;
    default:
    return 0;
  }
  
  return 1;
}

/******************************************************************************
* CHUNK OPERATIONS
*****************************************************************************/

pngcore_chunk_t* pngcore_get_chunk(const pngcore_png_t *png, const char type[4]) {
  if (!png || !png->internal || !type) {
    return NULL;
  }
  
  /* For now, only support the three basic chunks */
  if (memcmp(type, "IHDR", 4) == 0) {
    pngcore_chunk_t *chunk = malloc(sizeof(pngcore_chunk_t));
    if (chunk) {
      chunk->internal = pngcore_ihdr_to_raw(png->internal->ihdr);
    }
    return chunk;
  } else if (memcmp(type, "IDAT", 4) == 0) {
    pngcore_chunk_t *chunk = malloc(sizeof(pngcore_chunk_t));
    if (chunk) {
      chunk->internal = pngcore_idat_to_raw(png->internal->idat);
    }
    return chunk;
  } else if (memcmp(type, "IEND", 4) == 0) {
    pngcore_chunk_t *chunk = malloc(sizeof(pngcore_chunk_t));
    if (chunk) {
      chunk->internal = pngcore_iend_to_raw(png->internal->iend);
    }
    return chunk;
  }
  
  return NULL;
}

const uint8_t* pngcore_chunk_get_data(const pngcore_chunk_t *chunk, size_t *size) {
  if (!chunk || !chunk->internal || !size) {
    return NULL;
  }
  
  *size = chunk->internal->length;
  return chunk->internal->p_data;
}

uint32_t pngcore_chunk_get_crc(const pngcore_chunk_t *chunk) {
  return (chunk && chunk->internal) ? chunk->internal->crc : 0;
}

/******************************************************************************
* COMPRESSION
*****************************************************************************/

int pngcore_inflate(uint8_t *dest, uint64_t *dest_len, const uint8_t *src, uint64_t src_len) {
  return pngcore_mem_inflate(dest, dest_len, (U8*)src, src_len);
}

int pngcore_deflate(uint8_t *dest, uint64_t *dest_len, const uint8_t *src, uint64_t src_len, int level) {
  return pngcore_mem_deflate(dest, dest_len, (U8*)src, src_len, level);
}

/******************************************************************************
* UTILITY FUNCTIONS
*****************************************************************************/

uint32_t pngcore_crc32(const uint8_t *buf, size_t len) {
  return pngcore_crc((unsigned char*)buf, len);
}

const char* pngcore_error_string(pngcore_error_code_t code) {
  if (code >= 0 && code < sizeof(error_strings)/sizeof(error_strings[0])) {
    return error_strings[code];
  }
  return "Unknown error";
}

void pngcore_error_clear(pngcore_error_t *error) {
  if (error) {
    error->code = PNGCORE_SUCCESS;
    error->message[0] = '\0';
  }
}