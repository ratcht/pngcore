/**
* @file pngcore_network.c
* @brief Network operations for PNG fetching
*/

#include "pngcore.h"
#include "pngcore/pngcore_network.h"
#include "pngcore/pngcore_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/******************************************************************************
* BUFFER OPERATIONS
*****************************************************************************/

int pngcore_recv_buf_init(pngcore_recv_buf_t *ptr, size_t max_size) {
  void *p = NULL;
  
  if (ptr == NULL) {
    return 1;
  }
  
  p = malloc(max_size);
  if (p == NULL) {
    return 2;
  }
  
  ptr->buf = p;
  ptr->size = 0;
  ptr->max_size = max_size;
  ptr->seq = -1; /* initialize seq to invalid */
  return 0;
}

int pngcore_recv_buf_cleanup(pngcore_recv_buf_t *ptr) {
  if (ptr == NULL) {
    return 1;
  }
  
  free(ptr->buf);
  ptr->buf = NULL;
  ptr->size = 0;
  ptr->max_size = 0;
  ptr->seq = -1;
  return 0;
}

/******************************************************************************
* CURL CALLBACKS
*****************************************************************************/

/**
* @brief cURL header callback to extract image sequence number
*/
size_t pngcore_header_cb(char *p_recv, size_t size, size_t nmemb, void *userdata) {
  int realsize = size * nmemb;
  pngcore_http_response_t *response = (pngcore_http_response_t *)userdata;
  pngcore_recv_buf_t *header_buf = response->header;
  pngcore_recv_buf_t *data_buf = response->data;
  
  /* Store header data */
  if (header_buf->size + realsize + 1 > header_buf->max_size) {
    size_t new_size = header_buf->max_size + max(BUF_INC, realsize + 1);   
    char *q = realloc(header_buf->buf, new_size);
    if (q == NULL) {
      perror("realloc");
      return -1;
    }
    header_buf->buf = q;
    header_buf->max_size = new_size;
  }
  
  memcpy(header_buf->buf + header_buf->size, p_recv, realsize);
  header_buf->size += realsize;
  header_buf->buf[header_buf->size] = 0;
  
  /* Extract sequence number if present */
  if (realsize > strlen(FRAGMENT_HEADER) &&
  strncmp(p_recv, FRAGMENT_HEADER, strlen(FRAGMENT_HEADER)) == 0) {
    /* extract img sequence number */
    data_buf->seq = atoi(p_recv + strlen(FRAGMENT_HEADER));
  }
  
  return realsize;
}

/**
* @brief Write callback function to save received data
*/
size_t pngcore_write_cb(char *p_recv, size_t size, size_t nmemb, void *p_userdata) {
  size_t realsize = size * nmemb;
  pngcore_recv_buf_t *p = (pngcore_recv_buf_t *)p_userdata;
  
  if (p->size + realsize + 1 > p->max_size) {
    size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
    char *q = realloc(p->buf, new_size);
    if (q == NULL) {
      perror("realloc");
      return -1;
    }
    p->buf = q;
    p->max_size = new_size;
  }
  
  memcpy(p->buf + p->size, p_recv, realsize);
  p->size += realsize;
  p->buf[p->size] = 0;
  
  return realsize;
}

/******************************************************************************
* HTTP OPERATIONS
*****************************************************************************/

/**
* @brief Thread-safe function to fetch data from URL
*/
pngcore_http_response_t* pngcore_http_get(const char *url) {
  CURL *curl_handle;
  CURLcode res;
  pngcore_http_response_t *response = NULL;
  
  if (url == NULL) {
    fprintf(stderr, "pngcore_http_get: URL is null\n");
    return NULL;
  }
  
  /* Allocate response structure */
  response = malloc(sizeof(pngcore_http_response_t));
  if (response == NULL) {
    fprintf(stderr, "pngcore_http_get: failed to allocate response structure\n");
    return NULL;
  }
  
  /* Allocate data buffer */
  response->data = malloc(sizeof(pngcore_recv_buf_t));
  if (response->data == NULL) {
    fprintf(stderr, "pngcore_http_get: failed to allocate data buffer\n");
    free(response);
    return NULL;
  }
  
  /* Allocate header buffer */
  response->header = malloc(sizeof(pngcore_recv_buf_t));
  if (response->header == NULL) {
    fprintf(stderr, "pngcore_http_get: failed to allocate header buffer\n");
    free(response->data);
    free(response);
    return NULL;
  }
  
  /* Initialize buffers */
  if (pngcore_recv_buf_init(response->data, BUF_SIZE) != 0) {
    fprintf(stderr, "pngcore_http_get: failed to initialize data buffer\n");
    free(response->header);
    free(response->data);
    free(response);
    return NULL;
  }
  
  if (pngcore_recv_buf_init(response->header, BUF_SIZE) != 0) {
    fprintf(stderr, "pngcore_http_get: failed to initialize header buffer\n");
    pngcore_recv_buf_cleanup(response->data);
    free(response->header);
    free(response->data);
    free(response);
    return NULL;
  }
  
  /* Initialize CURL */
  curl_handle = curl_easy_init();
  if (curl_handle == NULL) {
    fprintf(stderr, "pngcore_http_get: curl_easy_init returned NULL\n");
    pngcore_recv_buf_cleanup(response->header);
    pngcore_recv_buf_cleanup(response->data);
    free(response->header);
    free(response->data);
    free(response);
    return NULL;
  }
  
  /* Set URL */
  curl_easy_setopt(curl_handle, CURLOPT_URL, url);
  
  /* Set write callback */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, pngcore_write_cb); 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)response->data);
  
  /* Set header callback */
  curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, pngcore_header_cb); 
  curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)response);
  
  /* Set user agent */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libpngcore/1.0");
  
  /* Perform request */
  res = curl_easy_perform(curl_handle);
  
  /* Clean up curl handle */
  curl_easy_cleanup(curl_handle);
  
  if (res != CURLE_OK) {
    fprintf(stderr, "pngcore_http_get: curl_easy_perform() failed: %s\n",
      curl_easy_strerror(res));
    pngcore_recv_buf_cleanup(response->header);
    pngcore_recv_buf_cleanup(response->data);
    free(response->header);
    free(response->data);
    free(response);
    return NULL;
  }
    
  return response;
}
  
/**
* @brief Free HTTP response structure
*/
void pngcore_free_http_response(pngcore_http_response_t *response) {
  if (response == NULL) {
    return;
  }
  
  if (response->data != NULL) {
    pngcore_recv_buf_cleanup(response->data);
    free(response->data);
  }
  
  if (response->header != NULL) {
    pngcore_recv_buf_cleanup(response->header);
    free(response->header);
  }
  
  free(response);
}
  
/******************************************************************************
* PUBLIC API IMPLEMENTATION
*****************************************************************************/

pngcore_http_response_t* pngcore_fetch_url(const char *url) {
  return pngcore_http_get(url);
}

void pngcore_free_response(pngcore_http_response_t *response) {
  pngcore_free_http_response(response);
}

int pngcore_response_get_sequence(const pngcore_http_response_t *response) {
  if (response && response->data) {
    return response->data->seq;
  }
  return -1;
}

const uint8_t* pngcore_response_get_data(const pngcore_http_response_t *response, size_t *size) {
  if (response && response->data && size) {
    *size = response->data->size;
    return (const uint8_t*)response->data->buf;
  }
  return NULL;
}