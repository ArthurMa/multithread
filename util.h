#include "thread_pool.h"

#ifndef _UTIL_H_
#define _UTIL_H_

struct request{
    int seat_id;
    int user_id;
    int customer_priority;
    char* resource;
};

typedef struct arg_bind {
	struct request req;
	int connfd;
} arg_b;

extern pool_t* threadpool;

void parse_request(arg_b*);
void process_request(arg_b*);

#endif
