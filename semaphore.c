#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "semaphore.h"

int tsem_init(m_sem_t* s, int value) {
	s->value = value;
	pthread_cond_init(&s->notify, NULL);
	pthread_mutex_init(&s->lock, NULL);
	return 0;
}

int tsem_wait(m_sem_t *s)
{
    //TODO
    pthread_mutex_lock(&s->lock);
    while (s->value == 0) {
    	pthread_cond_wait(&s->notify, &s->lock);
    }
    s->value = 0;
    pthread_mutex_unlock(&s->lock);
    return 0;
}

int tsem_post(m_sem_t *s)
{
    //TODO
    int prior_value;
    pthread_mutex_lock(&s->lock);
    prior_value = s->value;
    s->value = 1;
    pthread_mutex_unlock(&s->lock);
    if (prior_value == 0) 
    	pthread_cond_signal(&s->notify);
    return 0;
}
