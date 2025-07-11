/**
* @file pngcore_concurrent.h
* @brief Concurrent PNG processing with producer-consumer pattern
*/

#ifndef PNGCORE_CONCURRENT_H
#define PNGCORE_CONCURRENT_H

#include "pngcore_types.h"
#include <semaphore.h>
#include <sys/types.h>

/* Circular buffer entry */
typedef struct {
  U8 data[MAX_IMG_STRIP_SIZE];  /* image data */
  size_t length;                /* actual length of data */
  int sequence_num;             /* sequence number (0-49) */
} pngcore_cbuf_entry_t;

/* Circular buffer structure */
typedef struct {
  pngcore_cbuf_entry_t *data;
  size_t capacity;        /* maximum number of segments */
  size_t count;          /* current number of items in buffer */
  size_t head;           /* next write position */
  size_t tail;           /* next read position */
} pngcore_cbuf_t;

/* Concurrent processor structure */
typedef struct pngcore_concurrent {
  /* Configuration */
  int buffer_size;
  int num_producers;
  int num_consumers;
  int consumer_delay_ms;
  int image_num;
  
  /* Shared memory IDs */
  int shmid_cbuf;
  int shmid_idat;
  int shmid_sems;
  
  /* Shared memory pointers */
  pngcore_cbuf_t *circ_buf;
  U8 *idat_buf;
  sem_t *sems;  /* 0: mutex, 1: empty, 2: filled */
  
  /* Coordination variables */
  int *entries_produced;
  int *entries_consumed;
  int *next_entry_to_produce;
  
  /* Process IDs */
  pid_t *producer_pids;
  pid_t *consumer_pids;
  
  /* Timing */
  double start_time;
  double end_time;
} pngcore_concurrent_t;

/* Circular buffer operations */
void pngcore_cbuf_init(pngcore_cbuf_t *cb, size_t capacity);
int pngcore_cbuf_add(pngcore_cbuf_t *cb, pngcore_cbuf_entry_t *src_data, sem_t *sems);
int pngcore_cbuf_get(pngcore_cbuf_t *cb, pngcore_cbuf_entry_t *dest_data, sem_t *sems);

/* Producer and consumer functions */
int pngcore_producer(int producer_id, pngcore_concurrent_t *proc);
int pngcore_consumer(int consumer_id, pngcore_concurrent_t *proc);

#endif /* PNGCORE_CONCURRENT_H */