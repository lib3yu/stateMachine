/**
  ******************************************************************************
  * File Name          : queue.c
  * Description        : source for queue
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
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <limits.h>

/* Private function prototypes -----------------------------------------------*/

// 内部辅助函数：计算绝对超时时间
static void get_abstime(struct timespec *ts, int timeout_ms) 
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec + (timeout_ms / 1000);
    ts->tv_nsec = (tv.tv_usec * 1000) + ((timeout_ms % 1000) * 1000000);
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

/* Public application code ---------------------------------------------------*/

// 初始化队列
int newqueue(queue_t *q, size_t item_size, int num) 
{
    if ((q == NULL) || (item_size == 0) || (num <= 0))
    {
        return -1;
    }

    size_t capacity = (size_t)num;
    if (item_size > 0 && capacity > (SIZE_MAX / item_size))
    {
        return -1; // 乘法将会溢出
    }

    void *buffer = malloc(item_size * capacity);
    if (!buffer)
    {
        return -1;
    }

    q->buffer = buffer;
    q->item_size = item_size;
    q->capacity = num;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->shutting_down = 0;

    int ret = pthread_mutex_init(&q->lock, NULL);
    if (ret != 0)
    {
        free(q->buffer);
        q->buffer = NULL;
        return -1;
    }

    ret = pthread_cond_init(&q->not_full, NULL);
    if (ret != 0)
    {
        pthread_mutex_destroy(&q->lock);
        free(q->buffer);
        q->buffer = NULL;
        return -1;
    }

    ret = pthread_cond_init(&q->not_empty, NULL);
    if (ret != 0)
    {
        pthread_cond_destroy(&q->not_full);
        pthread_mutex_destroy(&q->lock);
        free(q->buffer);
        q->buffer = NULL;
        return -1;
    }
    
    return 0;
}

// 销毁队列
int delequeue(queue_t *q) 
{
    if (q == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&q->lock);
    q->shutting_down = 1;
    pthread_cond_broadcast(&q->not_full);
    pthread_cond_broadcast(&q->not_empty);

    void *buffer = q->buffer;
    q->buffer = NULL;
    q->count = 0;
    pthread_mutex_unlock(&q->lock);

    free(buffer);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    return 0;
}


// 入队 (timeout: -1 永久阻塞, 0 不阻塞, >0 毫秒)
int enqueue(queue_t *q, void *data, int timeout) 
{
    if ((q == NULL) || (data == NULL))
    {
        return -1;
    }

    pthread_mutex_lock(&q->lock);

    if (q->shutting_down || q->buffer == NULL)
    {
        pthread_mutex_unlock(&q->lock);
        return -3; // 队列正在关闭
    }

    while (q->count == q->capacity)
    {
        if (q->shutting_down)
        {
            pthread_mutex_unlock(&q->lock);
            return -3;
        }

        if (timeout == 0)
        {
            pthread_mutex_unlock(&q->lock);
            return -1; // 队列满且不等待
        }

        if (timeout > 0)
        {
            struct timespec ts;
            get_abstime(&ts, timeout);
            int ret = pthread_cond_timedwait(&q->not_full, &q->lock, &ts);
            if (ret == ETIMEDOUT)
            {
                pthread_mutex_unlock(&q->lock);
                return -2; // 超时
            }
            else if (ret != 0)
            {
                pthread_mutex_unlock(&q->lock);
                return -ret;
            }
        }
        else
        {
            int ret = pthread_cond_wait(&q->not_full, &q->lock);
            if (ret != 0)
            {
                pthread_mutex_unlock(&q->lock);
                return -ret;
            }
        }
    }

    // 内存复制数据到缓冲区
    memcpy((char *)q->buffer + (q->tail * q->item_size), data, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

// 出队
int dequeue(queue_t *q, void *out_data, int timeout) 
{
    if ((q == NULL) || (out_data == NULL))
    {
        return -1;
    }

    pthread_mutex_lock(&q->lock);

    while (q->count == 0)
    {
        if (q->shutting_down)
        {
            pthread_mutex_unlock(&q->lock);
            return -3;
        }

        if (timeout == 0)
        {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
        if (timeout > 0)
        {
            struct timespec ts;
            get_abstime(&ts, timeout);
            int ret = pthread_cond_timedwait(&q->not_empty, &q->lock, &ts);
            if (ret == ETIMEDOUT)
            {
                pthread_mutex_unlock(&q->lock);
                return -2;
            }
            else if (ret != 0)
            {
                pthread_mutex_unlock(&q->lock);
                return -ret;
            }
        }
        else
        {
            int ret = pthread_cond_wait(&q->not_empty, &q->lock);
            if (ret != 0)
            {
                pthread_mutex_unlock(&q->lock);
                return -ret;
            }
        }
    }

    if (q->buffer == NULL)
    {
        pthread_mutex_unlock(&q->lock);
        return -3;
    }

    // 从缓冲区复制数据到外部
    memcpy(out_data, (char *)q->buffer + (q->head * q->item_size), q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

