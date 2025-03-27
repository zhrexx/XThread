#include <stdio.h>
#include <stdlib.h>
#include "../src/xthread.h"

#define BUFFER_SIZE 10
#define TOTAL_ITEMS 50

int buffer[BUFFER_SIZE];
int buffer_count = 0;
int next_in = 0;
int next_out = 0;

mtx_t mutex;
cnd_t not_full;
cnd_t not_empty;

int producer(void *arg) {
    int produced = 0;
    while (produced < TOTAL_ITEMS) {
        mtx_lock(&mutex);

        while (buffer_count == BUFFER_SIZE) {
            cnd_wait(&not_full, &mutex);
        }

        buffer[next_in] = produced;
        next_in = (next_in + 1) % BUFFER_SIZE;
        buffer_count++;

        printf("Produced: %d (Buffer count: %d)\n", produced, buffer_count);

        cnd_signal(&not_empty);

        mtx_unlock(&mutex);

        produced++;

        thrd_sleep(&(struct timespec){.tv_nsec = 50000000}, NULL);
    }

    return 0;
}

int consumer(void *arg) {
    int consumed = 0;
    while (consumed < TOTAL_ITEMS) {
        mtx_lock(&mutex);

        while (buffer_count == 0) {
            cnd_wait(&not_empty, &mutex);
        }

        int item = buffer[next_out];
        next_out = (next_out + 1) % BUFFER_SIZE;
        buffer_count--;

        printf("Consumed: %d (Buffer count: %d)\n", item, buffer_count);

        cnd_signal(&not_full);

        mtx_unlock(&mutex);

        consumed++;

        thrd_sleep(&(struct timespec){.tv_nsec = 75000000}, NULL);
    }

    return 0;
}

int main() {
    mtx_init(&mutex, MTX_RECURSIVE);
    cnd_init(&not_full);
    cnd_init(&not_empty);

    thrd_t producer_thread, consumer_thread;

    if (thrd_create(&producer_thread, producer, NULL) != THRD_SUCCESS) {
        fprintf(stderr, "Failed to create producer thread\n");
        return 1;
    }

    if (thrd_create(&consumer_thread, consumer, NULL) != THRD_SUCCESS) {
        fprintf(stderr, "Failed to create consumer thread\n");
        return 1;
    }

    int producer_result, consumer_result;
    thrd_join(producer_thread, &producer_result);
    thrd_join(consumer_thread, &consumer_result);

    mtx_destroy(&mutex);
    cnd_destroy(&not_full);
    cnd_destroy(&not_empty);

    printf("Producer-Consumer example completed successfully\n");
    return 0;
}
