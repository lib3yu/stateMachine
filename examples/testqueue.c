/**
  ******************************************************************************
  * File Name          : testqueue.c
  * Description        : Producer/consumer demo for queue.c
  ******************************************************************************
  * @attention
  *
  * Copyright (c) lib3yu(neon).
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
***/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "queue.h"
/* Private includes ----------------------------------------------------------*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Public define 0 -----------------------------------------------------------*/
#define TEST_MESSAGE_COUNT 20
#define TEST_QUEUE_DEPTH   4
/* Public macro 0 ------------------------------------------------------------*/
/* Public typedef ------------------------------------------------------------*/
typedef struct {
    queue_t *queue;
    int message_count;
} thread_ctx_t;
/* Public variables ----------------------------------------------------------*/
/* Public define 1 -----------------------------------------------------------*/
/* Public macro 1 ------------------------------------------------------------*/
/* Public variables ----------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
static void *producer_thread(void *arg);
static void *consumer_thread(void *arg);
static void sleep_ms(int milliseconds);
/* Public define 2 -----------------------------------------------------------*/
/* Public macro 2 ------------------------------------------------------------*/
/* Public application code ---------------------------------------------------*/

static void *producer_thread(void *arg)
{
    thread_ctx_t *ctx = (thread_ctx_t *)arg;

    for (int i = 0; i < ctx->message_count; ++i)
    {
        int payload = i;
        int ret = enqueue(ctx->queue, &payload, 100);
        if (ret != 0)
        {
            fprintf(stderr, "producer: enqueue failed at %d (ret=%d)\n", i, ret);
            return NULL;
        }

        printf("[producer] sent %d\n", payload);
        sleep_ms(50); /* Slow slightly to exercise blocking */
    }

    return NULL;
}

static void *consumer_thread(void *arg)
{
    thread_ctx_t *ctx = (thread_ctx_t *)arg;

    for (int i = 0; i < ctx->message_count; ++i)
    {
        int payload = -1;
        int ret = dequeue(ctx->queue, &payload, 1000);
        if (ret != 0)
        {
            fprintf(stderr, "consumer: dequeue failed at %d (ret=%d)\n", i, ret);
            return NULL;
        }

        printf("[consumer] received %d\n", payload);
        sleep_ms(8);
    }

    return NULL;
}

static void sleep_ms(int milliseconds)
{
    struct timespec req = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L
    };

    nanosleep(&req, NULL);
}

/* Entry point ---------------------------------------------------------------*/
int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    queue_t queue;
    if (newqueue(&queue, sizeof(int), TEST_QUEUE_DEPTH) != 0)
    {
        fprintf(stderr, "Failed to create queue\n");
        return EXIT_FAILURE;
    }

    thread_ctx_t producer_ctx = {
        .queue = &queue,
        .message_count = TEST_MESSAGE_COUNT
    };
    thread_ctx_t consumer_ctx = {
        .queue = &queue,
        .message_count = TEST_MESSAGE_COUNT
    };

    pthread_t producer;
    pthread_t consumer;

    if (pthread_create(&producer, NULL, producer_thread, &producer_ctx) != 0)
    {
        fprintf(stderr, "Failed to start producer thread\n");
        delequeue(&queue);
        return EXIT_FAILURE;
    }

    if (pthread_create(&consumer, NULL, consumer_thread, &consumer_ctx) != 0)
    {
        fprintf(stderr, "Failed to start consumer thread\n");
        pthread_cancel(producer);
        pthread_join(producer, NULL);
        delequeue(&queue);
        return EXIT_FAILURE;
    }

    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    delequeue(&queue);
    printf("Queue test complete.\n");
    return EXIT_SUCCESS;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
