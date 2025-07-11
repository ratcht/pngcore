/**
* @file pngcore_buffer.c
* @brief Circular buffer operations
*/

#include "pngcore/pngcore_concurrent.h"
#include <string.h>
#include <stdbool.h>

void pngcore_cbuf_init(pngcore_cbuf_t *cb, size_t capacity) {
  cb->head = 0;
  cb->tail = 0;
  cb->count = 0;
  cb->capacity = capacity;
  /* cb->data should be set to point to the shared memory location */
}

int pngcore_cbuf_add(pngcore_cbuf_t *cb, pngcore_cbuf_entry_t *src_data, sem_t *sems) {
  /* sems[0] = mutex, sems[1] = empty, sems[2] = filled */
  
  sem_wait(&sems[0]);  /* Lock mutex */
  
  /* Copy data */
  memcpy(&cb->data[cb->head], src_data, sizeof(pngcore_cbuf_entry_t));
  cb->head = (cb->head + 1) % cb->capacity;
  cb->count++;
  
  sem_post(&sems[0]);  /* Unlock mutex */
  
  return 1;
}

int pngcore_cbuf_get(pngcore_cbuf_t *cb, pngcore_cbuf_entry_t *dest_data, sem_t *sems) {
  /* sems[0] = mutex, sems[1] = empty, sems[2] = filled */
  
  sem_wait(&sems[0]);  /* Lock mutex */
  
  if (cb->count <= 0) {
    sem_post(&sems[0]);  /* Unlock mutex */
    return -1;
  }
  
  /* Copy data */
  memcpy(dest_data, &cb->data[cb->tail], sizeof(pngcore_cbuf_entry_t));
  cb->tail = (cb->tail + 1) % cb->capacity;
  cb->count--;
  
  sem_post(&sems[0]);  /* Unlock mutex */
  
  return 1;
}