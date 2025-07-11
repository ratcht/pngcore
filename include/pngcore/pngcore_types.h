/**
* @file pngcore_types.h
* @brief Internal type definitions for PNG System Library
*/

#ifndef PNGCORE_TYPES_H
#define PNGCORE_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* Type shortcuts */
typedef unsigned char U8;
typedef unsigned int U32;
typedef unsigned long int U64;

/* PNG signature and chunk sizes */
#define CHUNK_LEN_SIZE  4
#define CHUNK_TYPE_SIZE 4
#define CHUNK_CRC_SIZE  4
#define DATA_IHDR_SIZE 13
#define PNG_SIG_SIZE    8
#define PNG_SIG 0x89504E470D0A1A0AULL

/* Buffer sizes */
#define BUF_SIZE 1048576  /* 1MB */
#define BUF_INC  524288   /* 0.5MB */
#define MAX_IMG_STRIP_SIZE 10000

/* Network constants */
#define URL_ENDPOINT "http://ece252-1.uwaterloo.ca:2530/image"
#define FRAGMENT_HEADER "X-Ece252-Fragment: "

/* Image processing constants */
#define NUM_MACHINES 3
#define TOTAL_IMAGES 50
#define INF_SIZE 6*(400*4 + 1)

/* Internal error codes */
typedef enum {
  SUCCESS = 0,
  ERR = 1,
  ERR_NOT_A_PNG = 2,
  ERR_CRC_MISMATCH = 3,
  ERR_NOT_IMPLEMENTED = 4,
  ERR_WRONG_CHUNK = 5
} ErrorCode;

typedef struct {
  ErrorCode code;
  char message[256];
} Error;

/* Utility macro */
#define max(a, b) \
({ __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  _a > _b ? _a : _b; })
  
/* Inline utility function */
inline static U32 from_be_buffer(U8 *buf) {
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

#endif /* PNGCORE_TYPES_H */