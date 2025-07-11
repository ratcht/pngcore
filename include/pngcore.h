/**
* @file pngcore.h
* @brief Public API for PNG System Library
* @author Your Name
* @date 2024
* 
* A high-performance PNG processing library with concurrent processing,
* network capabilities, and shared memory support.
*/

#ifndef PNGCORE_H
#define PNGCORE_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>


/* Version */
#define PNGCORE_VERSION_MAJOR 1
#define PNGCORE_VERSION_MINOR 0
#define PNGCORE_VERSION_PATCH 0

/* Constants */
#define PNGCORE_MAX_CHUNK_SIZE 10000
#define PNGCORE_DEFAULT_BUFFER_SIZE 1048576  /* 1MB */

/* Error codes */
typedef enum {
  PNGCORE_SUCCESS = 0,
  PNGCORE_ERR = 1,
  PNGCORE_ERR_NOT_PNG = 2,
  PNGCORE_ERR_CRC_MISMATCH = 3,
  PNGCORE_ERR_NOT_IMPLEMENTED = 4,
  PNGCORE_ERR_WRONG_CHUNK = 5,
  PNGCORE_ERR_MEMORY = 6,
  PNGCORE_ERR_IO = 7,
  PNGCORE_ERR_NETWORK = 8
} pngcore_error_code_t;

/* Error structure */
typedef struct {
  pngcore_error_code_t code;
  char message[256];
} pngcore_error_t;

/* Forward declarations */
typedef struct pngcore_png pngcore_png_t;
typedef struct pngcore_chunk pngcore_chunk_t;
typedef struct pngcore_concurrent pngcore_concurrent_t;
typedef struct pngcore_http_response pngcore_http_response_t;

/* PNG color types */
typedef enum {
  PNGCORE_COLOR_GRAYSCALE = 0,
  PNGCORE_COLOR_RGB = 2,
  PNGCORE_COLOR_INDEXED = 3,
  PNGCORE_COLOR_GRAYSCALE_ALPHA = 4,
  PNGCORE_COLOR_RGBA = 6
} pngcore_color_type_t;

/******************************************************************************
* Core PNG Functions
*****************************************************************************/

/* Loading and saving */
pngcore_png_t* pngcore_load_file(const char *filename, pngcore_error_t *error);
pngcore_png_t* pngcore_load_buffer(const uint8_t *buffer, size_t size, pngcore_error_t *error);
int pngcore_save_file(const pngcore_png_t *png, const char *filename, pngcore_error_t *error);

/* Creation */
pngcore_png_t* pngcore_create(uint32_t width, uint32_t height, uint8_t bit_depth, uint8_t color_type);

/* Properties */
uint32_t pngcore_get_width(const pngcore_png_t *png);
uint32_t pngcore_get_height(const pngcore_png_t *png);
uint8_t pngcore_get_bit_depth(const pngcore_png_t *png);
uint8_t pngcore_get_color_type(const pngcore_png_t *png);

/* Data access */
int pngcore_get_raw_data(const pngcore_png_t *png, uint8_t **data, size_t *size);
int pngcore_set_raw_data(pngcore_png_t *png, const uint8_t *data, size_t size);

/* Memory management */
void pngcore_free(pngcore_png_t *png);

/* Validation */
int pngcore_is_png_buffer(const uint8_t *buf, size_t size);
int pngcore_validate(const pngcore_png_t *png);

/******************************************************************************
* Chunk Operations
*****************************************************************************/

pngcore_chunk_t* pngcore_get_chunk(const pngcore_png_t *png, const char type[4]);
const uint8_t* pngcore_chunk_get_data(const pngcore_chunk_t *chunk, size_t *size);
uint32_t pngcore_chunk_get_crc(const pngcore_chunk_t *chunk);

/******************************************************************************
* Compression
*****************************************************************************/

int pngcore_inflate(uint8_t *dest, uint64_t *dest_len, const uint8_t *src, uint64_t src_len);
int pngcore_deflate(uint8_t *dest, uint64_t *dest_len, const uint8_t *src, uint64_t src_len, int level);

/******************************************************************************
* Network Operations
*****************************************************************************/

pngcore_http_response_t* pngcore_fetch_url(const char *url);
void pngcore_free_response(pngcore_http_response_t *response);
int pngcore_response_get_sequence(const pngcore_http_response_t *response);
const uint8_t* pngcore_response_get_data(const pngcore_http_response_t *response, size_t *size);

/******************************************************************************
* Concurrent Processing
*****************************************************************************/

typedef struct {
  int buffer_size;      /* Circular buffer size */
  int num_producers;    /* Number of producer processes */
  int num_consumers;    /* Number of consumer processes */
  int consumer_delay;   /* Consumer delay in milliseconds */
  int image_num;        /* Image number to fetch */
} pngcore_concurrent_config_t;

pngcore_concurrent_t* pngcore_concurrent_create(const pngcore_concurrent_config_t *config);
int pngcore_concurrent_run(pngcore_concurrent_t *proc);
pngcore_png_t* pngcore_concurrent_get_result(pngcore_concurrent_t *proc);
void pngcore_concurrent_destroy(pngcore_concurrent_t *proc);
double pngcore_concurrent_get_time(const pngcore_concurrent_t *proc);

/******************************************************************************
* Utility Functions
*****************************************************************************/

uint32_t pngcore_crc32(const uint8_t *buf, size_t len);
const char* pngcore_error_string(pngcore_error_code_t code);
void pngcore_error_clear(pngcore_error_t *error);

#endif /* PNGCORE_H */