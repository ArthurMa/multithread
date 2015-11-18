#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

typedef struct pool_t pool_t;


pool_t *pool_create(int thread_count, int queue_size);

int pool_add_task(pool_t *pool, void (*routine)(void *), void *arg,int);

int pool_destroy(pool_t *pool);

void pool_wait_all(pool_t*);

void *thread_cleanup(void *m_pool);

#endif
