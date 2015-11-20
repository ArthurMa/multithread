#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "thread_pool.h"
#include "seats.h"

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  Feel free to make any modifications you want to the function prototypes and structs
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

#define MAX_THREADS 20
#define STANDBY_SIZE 10

#define PRIORITY 3

typedef struct pool_task{
    void (*function)(void *);
    void *argument;
    struct pool_task *next;
} pool_task_t;

typedef struct pri_queue {
  pool_task_t* front;
  pool_task_t* rear;
  int priority;
} pri_queue_t;

struct pool_t {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  pthread_t *cleanup_thread;
  pri_queue_t task_queue[PRIORITY];
  int thread_count;
  int task_queue_size_limit;
  int task_count;
  int shutdown;
};

static void *thread_do_work(void *pool);

/*
 * Create a threadpool, initialize variables, etc
 *
 */
pool_t *pool_create(int queue_size, int num_threads)
{
  if (num_threads > MAX_THREADS || num_threads <= 0)
    num_threads = MAX_THREADS;
  pool_t* pool = (pool_t*)malloc(sizeof(pool_t));
  pool->threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
  pool->cleanup_thread = (pthread_t*)malloc(sizeof(pthread_t));
  pool->thread_count = num_threads;
  pool->task_queue_size_limit = queue_size;
  pool->task_count = 0;
  pool->shutdown = 0;
  pthread_mutex_init(&pool->lock, NULL);
  pthread_cond_init(&pool->notify, NULL);

  int i;
  for (i = 0; i < num_threads; i++) {
    pthread_create(&(pool->threads[i]), NULL, thread_do_work, (void*)pool);
  }
  pthread_create(pool->cleanup_thread, NULL, thread_cleanup, (void*)pool);
  //printf("created\n");
  for (i = 0; i < PRIORITY; i++) {
      pool->task_queue[i].front = NULL;
      pool->task_queue[i].rear = NULL;
      pool->task_queue[i].priority = i+1;
  }

  return pool;
}



/*
 * Add a task to the threadpool
 *
 */
int pool_add_task(pool_t *pool, void (*function)(void *), void *argument, int pri)
{
    int err = 0;
    pool_task_t* task = NULL;
    pthread_mutex_lock(&pool->lock);

    if (pool->task_count < pool->task_queue_size_limit) {
      task = (pool_task_t*)malloc(sizeof(pool_task_t));
      task->function = function;
      task->argument = argument;
      task->next = NULL;

      if (pool->task_queue[pri].front == NULL) {
        pool->task_queue[pri].front = pool->task_queue[pri].rear = task;
      }
      else {
        pool->task_queue[pri].rear->next = task;
        pool->task_queue[pri].rear = task;
      }

      pool->task_count++;
    }

    pthread_cond_signal(&pool->notify);
    pthread_mutex_unlock(&pool->lock);
        
    return err;
}

void pool_wait_all(pool_t *pool) {

  int i;
  void* ret;
  for (i = 0; i < pool->task_count; i++) {
    pthread_join(pool->threads[i], (void**)&ret);
  }
  return;
}

void pool_cancel_all(pool_t *pool) {
  pthread_mutex_lock(&pool->lock);
  pool->shutdown = 1;
  pthread_mutex_unlock(&pool->lock);

  int i;
  for (i = 0; i < pool->task_count; i++) {
    pthread_cancel(pool->threads[i]);
  }
  pthread_cancel(*pool->cleanup_thread);
  return;
}

/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */

void queue_destroy(pool_t* pool) {
  int i;
  for (i = 0; i < PRIORITY; i++) {
    pool_task_t* temp;
    while(pool->task_queue[i].front != NULL) {
      temp = pool->task_queue[i].front;
      pool->task_queue[i].front = pool->task_queue[i].front->next;
      free(temp);
    }
    pool->task_queue[i].front = pool->task_queue[i].rear = NULL;
  }
  return;
}

int pool_destroy(pool_t *pool)
{
    int err = 0;
    pool_cancel_all(pool);

    pthread_cond_destroy(&pool->notify);
    pthread_mutex_destroy(&pool->lock);

    queue_destroy(pool); 

    free(pool->threads);
    free(pool->cleanup_thread);
    free(pool);

    
    return err;
}

/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *m_pool)
{ 
    pool_t *pool = (pool_t*)m_pool;

    while(!pool->shutdown) {

      pthread_mutex_lock(&pool->lock);
      
      while (pool->task_count <= 0) {
        pthread_cond_wait(&pool->notify, &pool->lock);
      }

      int i;
      pool_task_t* task;
      for (i = 0; i < PRIORITY; i--) {
        task = pool->task_queue[i].front;
        if (!task)
          continue;
        if (pool->task_queue[i].front == pool->task_queue[i].rear) {
          pool->task_queue[i].front = pool->task_queue[i].rear = NULL;
          break;
        } else {
          pool->task_queue[i].front = pool->task_queue[i].front->next;
          break;
        }
      }

      pool->task_count--;

      pthread_mutex_unlock(&pool->lock);

      task->function(task->argument);
      free(task);
      task = NULL;
    }

    pthread_exit(NULL);
    return(NULL);
}

void *thread_cleanup(void *m_pool) {
  pool_t *pool = (pool_t*)m_pool;
  while(!pool->shutdown) {
    sleep(1); //about 1 sec
    check_seats();
  }
  pthread_exit(NULL);
  return(NULL);
}
