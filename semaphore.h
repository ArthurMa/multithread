#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

typedef struct m_sem_t {
    int value;
    pthread_mutex_t lock;
    pthread_cond_t notify;
} m_sem_t;

int tsem_init(m_sem_t*, int);
int tsem_wait(m_sem_t *s);
int tsem_post(m_sem_t *s);

#endif