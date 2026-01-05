/**
  ******************************************************************************
  * File Name          : state402.h
  * Description        : Public interface for CiA402 state machine
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
#ifndef USERAPP_STATE402_H
#define USERAPP_STATE402_H

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
    STATE402_CMD_NONE,

    /* 状态转换命令 */
    STATE402_CMD_ENABLE,           /* Ready → Operation Enabled */
    STATE402_CMD_DISABLE,          /* Operation Enabled → Ready */

    /* 故障管理命令 */
    STATE402_CMD_INJECT_FAULT,     /* 任何状态 → Fault（显式故障注入） */
    STATE402_CMD_CLEAR_FAULT,      /* 清除故障条件 */
    STATE402_CMD_FAULT_RESET,      /* Fault → Ready（故障复位） */
} State402_CmdType_t;

/* 故障类型枚举 */
typedef enum {
    STATE402_FAULT_NONE = 0,
    STATE402_FAULT_OVERCURRENT,      /* 过流故障 */
    STATE402_FAULT_STALL,            /* 堵转故障 */
    STATE402_FAULT_UNDERVOLTAGE,     /* 欠压故障 */
    STATE402_FAULT_OVERTEMPERATURE,  /* 过温故障 */
    STATE402_FAULT_EXTERNAL_INTERLOCK, /* 外部互锁触发 */
    STATE402_FAULT_EMERGENCY_STOP,   /* 急停按钮触发 */
} State402_FaultType_t;

/* 命令数据负载 */
typedef union {
    int32_t fault_type;  /* 故障类型（for INJECT_FAULT） */
} State402_ParamPayload_t;

/* 命令消息 */
typedef struct {
    State402_CmdType_t type;
    State402_ParamPayload_t data;
} State402_Cmd_t;

/* 上下文（公共部分） */
typedef struct {
    int exit_app;
    int fault_active;               /* 当前故障状态标志 */
    State402_FaultType_t current_fault;  /* 当前故障类型 */
} State402_Context_t;

/* Public variables ----------------------------------------------------------*/
extern State402_Context_t state402_ctx;
extern queue_t state402_cmdQueue;

/* Public define 1 -----------------------------------------------------------*/
/* Public macro 1 ------------------------------------------------------------*/
/* Public variables ----------------------------------------------------------*/
/* Inline functions 1 --------------------------------------------------------*/
/* Public function prototypes ------------------------------------------------*/
void *state402_thread(void *arg);

/* Inline functions 2 --------------------------------------------------------*/
/* Public define 2 -----------------------------------------------------------*/
/* Public macro 2 ------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* USERAPP_STATE402_H */
