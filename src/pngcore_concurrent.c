/**
 * @file pngcore_concurrent.c
 * @brief Concurrent PNG processing implementation
 */

#include "pngcore.h"
#include "pngcore/pngcore_concurrent.h"
#include "pngcore/pngcore_network.h"
#include "pngcore/pngcore_png.h"
#include "pngcore/pngcore_zutil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>

#define SEM_PROC 1  /* Process-shared semaphore */

/******************************************************************************
 * PRODUCER/CONSUMER FUNCTIONS
 *****************************************************************************/

int pngcore_producer(int producer_id, pngcore_concurrent_t *proc) {
  while (1) {
    /* Get next entry number to produce */
    sem_wait(&proc->sems[0]); /* Acquire mutex */
    
    if (*proc->entries_produced >= TOTAL_IMAGES) {
      sem_post(&proc->sems[0]); /* Release mutex */
      break; /* All entries produced */
    }
    
    int entry_num = *proc->next_entry_to_produce;
    (*proc->next_entry_to_produce)++;
    (*proc->entries_produced)++;
    
    sem_post(&proc->sems[0]); /* Release mutex */
    
    /* Fetch the entry data */
    pngcore_cbuf_entry_t entry;
    char url[512];
    snprintf(url, sizeof(url), "%s?img=%d&part=%d", 
              URL_ENDPOINT, proc->image_num, entry_num);

    sem_wait(&proc->sems[1]); /* Wait for empty slot */

    pngcore_http_response_t* response = pngcore_http_get(url);
    if (!response || response->data->seq != entry_num) {
      fprintf(stderr, "Producer %d: Failed to get entry %d\n", 
              producer_id, entry_num);
      if (response) pngcore_free_http_response(response);
      sem_post(&proc->sems[1]); /* Return empty slot */
      continue;
    }

    /* Copy to entry */
    memcpy(&entry.data, response->data->buf, response->data->size);
    entry.length = response->data->size;
    entry.sequence_num = entry_num;

    /* Free response */
    pngcore_free_http_response(response);
    
    /* Add to circular buffer */
    pngcore_cbuf_add(proc->circ_buf, &entry, proc->sems);

    sem_post(&proc->sems[2]); /* Signal filled slot */
  }
  
  return 0;
}

int pngcore_consumer(int consumer_id, pngcore_concurrent_t *proc) {
  while (1) {
    /* Check if all work is done */
    sem_wait(&proc->sems[0]); /* Acquire mutex */
    if (*proc->entries_consumed >= TOTAL_IMAGES) {
      sem_post(&proc->sems[0]); /* Release mutex */
      sem_post(&proc->sems[2]); /* Signal to wake other consumers */
      break;
    }
    sem_post(&proc->sems[0]); /* Release mutex */
    
    /* Get entry from buffer */
    sem_wait(&proc->sems[2]); /* Wait for filled slot */

    pngcore_cbuf_entry_t entry;
    if (pngcore_cbuf_get(proc->circ_buf, &entry, proc->sems) != 1) {
      continue; /* Failed to get entry */
    }
    
    sem_post(&proc->sems[1]); /* Signal empty slot */
    
    /* Sleep if configured */
    if (proc->consumer_delay_ms > 0) {
      usleep(proc->consumer_delay_ms * 1000);
    }

    /* Parse buffer to raw PNG */
    Error error = {SUCCESS, ""};
    pngcore_raw_png_t* png = pngcore_load_raw_png((U8*)entry.data, 
                                                  entry.length, 0, &error);
    if (!png) {
      fprintf(stderr, "Consumer %d: Failed to parse PNG for entry %d\n",
              consumer_id, entry.sequence_num);
      continue;
    }
    
    /* Inflate IDAT data into buffer at correct position */
    int offset = entry.sequence_num * INF_SIZE;
    
    if (offset + INF_SIZE < BUF_SIZE) {
      U64 temp_dest_len = 0;

      /* Inflate IDAT data into buffer */
      int ret = pngcore_mem_inflate(proc->idat_buf + offset, &temp_dest_len, 
                                    png->chunks[1]->p_data, png->chunks[1]->length);
      if (ret != 0) {
        fprintf(stderr, "mem_inf failed for img %d. ret = %d.\n", 
                entry.sequence_num, ret);
      }
    }
    
    /* Update consumed counter */
    sem_wait(&proc->sems[0]); /* Acquire mutex */
    (*proc->entries_consumed)++;
    sem_post(&proc->sems[0]); /* Release mutex */

    pngcore_free_raw_png(png);
  }
  
  return 0;
}

/******************************************************************************
 * PUBLIC API IMPLEMENTATION
 *****************************************************************************/

pngcore_concurrent_t* pngcore_concurrent_create(const pngcore_concurrent_config_t *config) {
  if (!config) return NULL;
  
  pngcore_concurrent_t *proc = calloc(1, sizeof(pngcore_concurrent_t));
  if (!proc) return NULL;
  
  /* Copy configuration */
  proc->buffer_size = config->buffer_size;
  proc->num_producers = config->num_producers;
  proc->num_consumers = config->num_consumers;
  proc->consumer_delay_ms = config->consumer_delay;
  proc->image_num = config->image_num;
  
  /* Calculate shared memory sizes */
  size_t cbuf_struct_size = sizeof(pngcore_cbuf_t);
  size_t cbuf_data_array_size = sizeof(pngcore_cbuf_entry_t) * proc->buffer_size;
  size_t coordination_vars_size = 3 * sizeof(int);
  size_t total_cbuf_size = cbuf_struct_size + cbuf_data_array_size + coordination_vars_size;
  
  /* Create shared memory segments */
  proc->shmid_cbuf = shmget(IPC_PRIVATE, total_cbuf_size, 
                            IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  proc->shmid_idat = shmget(IPC_PRIVATE, BUF_SIZE, 
                            IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  proc->shmid_sems = shmget(IPC_PRIVATE, sizeof(sem_t) * 3, 
                            IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  
  if (proc->shmid_cbuf == -1 || proc->shmid_idat == -1 || proc->shmid_sems == -1) {
    perror("shmget");
    free(proc);
    return NULL;
  }

  /* Attach to shared memory regions */
  void *cbuf_mem = shmat(proc->shmid_cbuf, NULL, 0);
  proc->idat_buf = shmat(proc->shmid_idat, NULL, 0);
  proc->sems = shmat(proc->shmid_sems, NULL, 0);

  if (cbuf_mem == (void *) -1 || proc->idat_buf == (void *) -1 || 
    proc->sems == (void *) -1) {
    perror("shmat");
    if (cbuf_mem != (void *) -1) shmdt(cbuf_mem);
    if (proc->idat_buf != (void *) -1) shmdt(proc->idat_buf);
    if (proc->sems != (void *) -1) shmdt(proc->sems);
    shmctl(proc->shmid_cbuf, IPC_RMID, NULL);
    shmctl(proc->shmid_idat, IPC_RMID, NULL);
    shmctl(proc->shmid_sems, IPC_RMID, NULL);
    free(proc);
    return NULL;
  }

  /* Setup pointers */
  proc->circ_buf = (pngcore_cbuf_t*)cbuf_mem;
  proc->circ_buf->data = (pngcore_cbuf_entry_t*)((char*)cbuf_mem + sizeof(pngcore_cbuf_t));
  
  char *coord_mem = (char*)cbuf_mem + sizeof(pngcore_cbuf_t) + cbuf_data_array_size;
  proc->entries_produced = (int*)coord_mem;
  proc->entries_consumed = (int*)(coord_mem + sizeof(int));
  proc->next_entry_to_produce = (int*)(coord_mem + 2 * sizeof(int));

  /* Initialize shared memory */
  pngcore_cbuf_init(proc->circ_buf, proc->buffer_size);
  memset(proc->idat_buf, 0, BUF_SIZE);
  
  *proc->entries_produced = 0;
  *proc->entries_consumed = 0;
  *proc->next_entry_to_produce = 0;
  
  /* Initialize semaphores */
  if (sem_init(&proc->sems[0], SEM_PROC, 1) != 0) {    /* mutex */
    perror("sem_init(mutex)");
    goto cleanup;
  }
  if (sem_init(&proc->sems[1], SEM_PROC, proc->buffer_size) != 0) { /* empty */
    perror("sem_init(empty)");
    goto cleanup;
  }
  if (sem_init(&proc->sems[2], SEM_PROC, 0) != 0) {    /* filled */
    perror("sem_init(filled)");
    goto cleanup;
  }
  
  /* Allocate PID arrays */
  proc->producer_pids = calloc(proc->num_producers, sizeof(pid_t));
  proc->consumer_pids = calloc(proc->num_consumers, sizeof(pid_t));
  if (!proc->producer_pids || !proc->consumer_pids) {
    goto cleanup;
  }
  
  return proc;

cleanup:
  shmdt(cbuf_mem);
  shmdt(proc->idat_buf);
  shmdt(proc->sems);
  shmctl(proc->shmid_cbuf, IPC_RMID, NULL);
  shmctl(proc->shmid_idat, IPC_RMID, NULL);
  shmctl(proc->shmid_sems, IPC_RMID, NULL);
  free(proc->producer_pids);
  free(proc->consumer_pids);
  free(proc);
  return NULL;
}

int pngcore_concurrent_run(pngcore_concurrent_t *proc) {
  if (!proc) return -1;
  
  pid_t pid = 0;
  int state;
  struct timeval tv;
  
  /* Record start time */
  if (gettimeofday(&tv, NULL) != 0) {
    perror("gettimeofday");
    return -1;
  }
  proc->start_time = (tv.tv_sec) + tv.tv_usec/1000000.;

  /* Create producer processes */
  for (int i = 0; i < proc->num_producers; i++) {
    pid = fork();
    if (pid > 0) {
      proc->producer_pids[i] = pid;
    } else if (pid == 0) {
      /* Child process */
      pngcore_producer(i, proc);
      exit(0);
    } else {
      perror("fork");
      return -1;
    }
  }

  /* Create consumer processes */
  for (int i = 0; i < proc->num_consumers; i++) {
    pid = fork();
    if (pid > 0) {
      proc->consumer_pids[i] = pid;
    } else if (pid == 0) {
      /* Child process */
      pngcore_consumer(i, proc);
      exit(0);
    } else {
      perror("fork");
      return -1;
    }
  }

  /* Wait for all producers to finish */
  for (int i = 0; i < proc->num_producers; i++) {
    waitpid(proc->producer_pids[i], &state, 0);
  }
  
  /* Wait for all consumers to finish */
  for (int i = 0; i < proc->num_consumers; i++) {
    waitpid(proc->consumer_pids[i], &state, 0);
  }
  
  /* Record end time */
  if (gettimeofday(&tv, NULL) != 0) {
    perror("gettimeofday");
    return -1;
  }
  proc->end_time = (tv.tv_sec) + tv.tv_usec/1000000.;
  
  return 0;
}

pngcore_png_t* pngcore_concurrent_get_result(pngcore_concurrent_t *proc) {
  if (!proc) return NULL;
  
  /* Create PNG structure */
  pngcore_png_t *result = pngcore_create(400, 6 * TOTAL_IMAGES, 8, PNGCORE_COLOR_RGBA);
  if (!result) return NULL;
  
  /* Deflate the assembled data */
  ssize_t deflate_res = pngcore_deflate_idat(proc->idat_buf, 
                                              INF_SIZE * TOTAL_IMAGES, 
                                              result->internal);
  if (deflate_res == -1) {
    pngcore_free(result);
    return NULL;
  }
  
  return result;
}

void pngcore_concurrent_destroy(pngcore_concurrent_t *proc) {
  if (!proc) return;
  
  /* Detach and remove shared memory */
  if (proc->circ_buf) shmdt(proc->circ_buf);
  if (proc->idat_buf) shmdt(proc->idat_buf);
  if (proc->sems) shmdt(proc->sems);
  
  shmctl(proc->shmid_cbuf, IPC_RMID, NULL);
  shmctl(proc->shmid_idat, IPC_RMID, NULL);
  shmctl(proc->shmid_sems, IPC_RMID, NULL);
  
  free(proc->producer_pids);
  free(proc->consumer_pids);
  free(proc);
}

double pngcore_concurrent_get_time(const pngcore_concurrent_t *proc) {
  if (!proc) return 0.0;
  return proc->end_time - proc->start_time;
}