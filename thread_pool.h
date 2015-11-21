#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

typedef struct pool_t pool_t;

//stat for benchmarking
typedef struct {
	clock_t total_time;
	int req_count;
	pthread_mutex_t lock;
} stat_t;

extern stat_t st;


pool_t *pool_create(int thread_count, int queue_size);

int pool_add_task(pool_t *pool, void (*routine)(void *), void *arg,int);

int pool_destroy(pool_t *pool);

void pool_wait_all(pool_t*);

void *thread_cleanup(void *m_pool);

void stat_init(stat_t* stat);

#endif
