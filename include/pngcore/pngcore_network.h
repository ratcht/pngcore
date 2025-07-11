/**
* @file pngcore_network.h
* @brief Network operations for PNG fetching
*/

#ifndef PNGCORE_NETWORK_H
#define PNGCORE_NETWORK_H

#include "pngcore_types.h"
#include <curl/curl.h>

/* Receive buffer structure */
typedef struct {
  char *buf;       /* memory to hold a copy of received data */
  size_t size;     /* size of valid data in buf in bytes */
  size_t max_size; /* max capacity of buf in bytes */
  int seq;         /* >=0 sequence number extracted from http header */
  /* <0 indicates an invalid seq number */
} pngcore_recv_buf_t;

/* HTTP response structure */
struct pngcore_http_response {
  pngcore_recv_buf_t *data;
  pngcore_recv_buf_t *header;
};

/* Buffer operations */
int pngcore_recv_buf_init(pngcore_recv_buf_t *ptr, size_t max_size);
int pngcore_recv_buf_cleanup(pngcore_recv_buf_t *ptr);

/* CURL callbacks */
size_t pngcore_header_cb(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t pngcore_write_cb(char *p_recv, size_t size, size_t nmemb, void *p_userdata);

/* HTTP operations */
pngcore_http_response_t* pngcore_http_get(const char *url);
void pngcore_free_http_response(pngcore_http_response_t *response);

#endif /* PNGCORE_NETWORK_H */