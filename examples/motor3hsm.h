/**
  ******************************************************************************
  * File Name          : motor3hsm.h
  * Description        : Public interface for motor3hsm
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
#ifndef USERAPP_MOTOR3HSM_H
#define USERAPP_MOTOR3HSM_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "stateMachine.h"
#include "queue.h"

/* Public define 0 -----------------------------------------------------------*/
/* Public macro 0 ------------------------------------------------------------*/
/* Public typedef ------------------------------------------------------------*/

/* 命令类型（外部接口） */
typedef enum {
    MOTOR_CMD_NONE,
    MOTOR_CMD_START,
    MOTOR_CMD_STOP,
    MOTOR_CMD_RST_FAULT,
    MOTOR_CMD_SET_MODE,
    MOTOR_CMD_SET_TARGET_VELOCITY,
    MOTOR_CMD_SET_TARGET_POSITION,
    MOTOR_CMD_SET_ACCELERATION,
} Motor_CmdType_t;

/* 命令数据负载 */
typedef union {
    int mode;
    int32_t targetVelocity;
    int32_t targetPosition;
    uint32_t acceleration;
} Motor_ParamPayload_t;

/* 命令消息 */
typedef struct {
    Motor_CmdType_t type;
    Motor_ParamPayload_t data;
} Motor_Cmd_t;

/* 上下文（公共部分） */
typedef struct {
    int exit_app;
    int fault_active;
} Context_t;

/* Public variables ----------------------------------------------------------*/
extern Context_t ctx;
extern queue_t cmdQueue;

/* Public define 1 -----------------------------------------------------------*/
/* Public macro 1 ------------------------------------------------------------*/
/* Public variables ----------------------------------------------------------*/
/* Inline functions 1 --------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
void *motor_thread(void *arg);

/* Inline functions 2 --------------------------------------------------------*/
/* Public define 2 -----------------------------------------------------------*/
/* Public macro 2 ------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* USERAPP_MOTOR3HSM_H */
