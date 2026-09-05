#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Thread function for running the timer
void *timer_thread(void *arg);

#endif