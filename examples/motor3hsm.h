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

/* 运动模式枚举（第三层：运动模式层） */
typedef enum {
    MOTOR_MOTION_PVM = 0,         /* 轮廓速度（默认） */
    MOTOR_MOTION_PPM,             /* 轮廓位置 */
    MOTOR_MOTION_CSV,             /* 循环速度 */
    MOTOR_MOTION_CSP,             /* 循环位置 */
    MOTOR_MOTION_CST,             /* 循环力矩 */
    MAX_MOTOR_MOTION_NUM
} Motor_MotionMode_t;

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
    Motor_MotionMode_t motion;
    int32_t targetVelocity;
    int32_t targetPosition;
    uint32_t acceleration;
} Motor_CmdPayload_t;

/* 命令消息 */
typedef struct {
    Motor_CmdType_t type;
    Motor_CmdPayload_t data;
} Motor_Cmd_t;

/* 参数类型枚举 */
typedef enum {
    MOTOR_PARAM_TARGET_VELOCITY,
    MOTOR_PARAM_TARGET_POSITION,
    MOTOR_PARAM_ACCELERATION,
} Motor_ParamType_t;

/* 参数值结构体 */
typedef struct {
    Motor_ParamType_t type;
    Motor_CmdPayload_t val;
} Motor_Param_t;

/* 上下文（公共部分） */
typedef struct {
    int exit_app;
    int fault_active;
    Motor_MotionMode_t pendingMotion;  /* 待切换的运动模式 */
    Motor_MotionMode_t lastMotion;     /* 上一次的运动模式 */
    Motor_Param_t current_param;       /* 当前参数（线程安全） */
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
