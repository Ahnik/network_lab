#define _POSIX_C_SOURCE 200809L

#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include "timer.h"

void *timer_thread(void *arg) {
    // Time for which the timer counts is passed as argument
    long delay_ms = *((long *) arg);

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_nsec += delay_ms * 1000000;

    pthread_mutex_lock(&lock);
    int result = pthread_cond_timedwait(&cond, &lock, &timeout);
    pthread_mutex_unlock(&lock);

    int *result_ptr = (int *) malloc(sizeof(int));
    if (result_ptr == NULL) return NULL;
    *result_ptr = result;
    return result_ptr;
}