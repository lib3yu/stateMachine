/**
  ******************************************************************************
  * File Name          : queue.h
  * Description        : Header for queue
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
#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include <pthread.h>
/* Private includes ----------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* Public typedef ------------------------------------------------------------*/
typedef struct {
    void *buffer;           // 缓冲区指针
    size_t item_size;       // 单个消息大小
    int capacity;           // 总容量
    int count;              // 当前消息数量
    int head;               // 弹出位置
    int tail;               // 写入位置
    int shutting_down;      // 标记队列是否处于销毁状态
    
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} queue_t;

// 初始化队列
int newqueue(queue_t *q, size_t item_size, int num);
// 销毁队列
int delequeue(queue_t *q);
// 入队 (timeout: -1 永久阻塞, 0 不阻塞, >0 毫秒)
int enqueue(queue_t *q, void *data, int timeout);
// 出队
int dequeue(queue_t *q, void *out_data, int timeout);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* QUEUE_H */
