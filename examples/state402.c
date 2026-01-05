/**
  ******************************************************************************
  * File Name          : state402.c
  * Description        : CiA402 state machine core implementation
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
#include "state402.h"

/* Private includes ----------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

/* Private define 0 ----------------------------------------------------------*/
/* Private macro 0 -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/

/* 状态枚举（内部实现） */
typedef enum {
    STATE402_STATE_INITIALIZATION,
    STATE402_STATE_READY_STANDBY,
    STATE402_STATE_OPERATION_ENABLED,
    STATE402_STATE_FAULT,
    MAX_STATE402_STATE_NUM = 4
} State402_State_t;

/* 事件类型（内部实现） */
typedef enum {
    STATE402_EV_NONE,
    STATE402_EV_CYCLE,
    STATE402_EV_FAULT_ACTIVE,
    STATE402_EV_ENABLE_REQUESTED,
    STATE402_EV_DISABLE_REQUESTED,
    STATE402_EV_FAULT_RESET_REQUESTED,
} State402_Event_t;

/* Private variables ---------------------------------------------------------*/
/* 状态机实例 */
static struct stateMachine fsm;

/* 状态数组声明 */
static struct state stateLayer[MAX_STATE402_STATE_NUM];

/* Private define 1 ----------------------------------------------------------*/
/* Private macro 1 -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/* 守卫函数声明 */
static bool Guard_SelfCheckPassed(void *param, struct event *e);
static bool Guard_FaultCleared(void *param, struct event *e);

/* Entry动作函数声明 */
static void EnterAction_Initialization(void *stateData, struct event *e);
static void EnterAction_ReadyStandby(void *stateData, struct event *e);
static void EnterAction_OperationEnabled(void *stateData, struct event *e);
static void EnterAction_Fault(void *stateData, struct event *e);

/* Private define 2 ----------------------------------------------------------*/
/* Private macro 2 -----------------------------------------------------------*/
/* Private function code -----------------------------------------------------*/

/* ===== 守卫函数实现 ===== */
static bool Guard_SelfCheckPassed(void *param, struct event *e) {
    (void)param; (void)e;
    printf("[Guard] 自检通过\n");
    return true;
}

static bool Guard_FaultCleared(void *param, struct event *e) {
    (void)param; (void)e;
    if (!state402_ctx.fault_active) {
        printf("[Guard] 故障已清除\n");
        return true;
    }
    printf("[Guard] 故障未清除，复位失败\n");
    return false;
}

/* ===== Entry动作函数实现 ===== */
static void EnterAction_Initialization(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入初始化状态\n");
}

static void EnterAction_ReadyStandby(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入待机状态（Ready）\n");
}

static void EnterAction_OperationEnabled(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入使能运行状态（Operation Enabled）\n");
}

static void EnterAction_Fault(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    const char *fault_name = (state402_ctx.current_fault == STATE402_FAULT_OVERCURRENT) ? "过流" :
                            (state402_ctx.current_fault == STATE402_FAULT_STALL) ? "堵转" :
                            (state402_ctx.current_fault == STATE402_FAULT_UNDERVOLTAGE) ? "欠压" :
                            (state402_ctx.current_fault == STATE402_FAULT_OVERTEMPERATURE) ? "过温" :
                            (state402_ctx.current_fault == STATE402_FAULT_EXTERNAL_INTERLOCK) ? "外部互锁" :
                            (state402_ctx.current_fault == STATE402_FAULT_EMERGENCY_STOP) ? "急停" : "未知";
    printf(">> [State] 进入故障状态（Fault: %s）\n", fault_name);
}

/* ===== 状态数组初始化 ===== */
static struct state stateLayer[MAX_STATE402_STATE_NUM] = {
    /* INITIALIZATION：初始化状态 */
    [STATE402_STATE_INITIALIZATION] = {
        .parentState = NULL,
        .data = &state402_ctx,
        .entryState = NULL,
        .entryAction = EnterAction_Initialization,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { STATE402_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_FAULT] },
            { STATE402_EV_CYCLE, NULL, Guard_SelfCheckPassed, NULL, &stateLayer[STATE402_STATE_READY_STANDBY] },
            { STATE402_EV_CYCLE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_INITIALIZATION] }
        },
        .numTransitions = 3,
    },
    /* READY_STANDBY：待机状态 */
    [STATE402_STATE_READY_STANDBY] = {
        .parentState = NULL,
        .data = &state402_ctx,
        .entryState = NULL,
        .entryAction = EnterAction_ReadyStandby,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { STATE402_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_FAULT] },
            { STATE402_EV_ENABLE_REQUESTED, NULL, NULL, NULL, &stateLayer[STATE402_STATE_OPERATION_ENABLED] },
            { STATE402_EV_CYCLE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_READY_STANDBY] }
        },
        .numTransitions = 3,
    },
    /* OPERATION_ENABLED：使能运行状态 */
    [STATE402_STATE_OPERATION_ENABLED] = {
        .parentState = NULL,
        .data = &state402_ctx,
        .entryState = NULL,
        .entryAction = EnterAction_OperationEnabled,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { STATE402_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_FAULT] },
            { STATE402_EV_DISABLE_REQUESTED, NULL, NULL, NULL, &stateLayer[STATE402_STATE_READY_STANDBY] },
            { STATE402_EV_CYCLE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_OPERATION_ENABLED] }
        },
        .numTransitions = 3,
    },
    /* FAULT：故障状态 */
    [STATE402_STATE_FAULT] = {
        .parentState = NULL,
        .data = &state402_ctx,
        .entryState = NULL,
        .entryAction = EnterAction_Fault,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { STATE402_EV_FAULT_RESET_REQUESTED, NULL, Guard_FaultCleared, NULL, &stateLayer[STATE402_STATE_READY_STANDBY] },
            { STATE402_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_FAULT] },
            { STATE402_EV_CYCLE, NULL, NULL, NULL, &stateLayer[STATE402_STATE_FAULT] }
        },
        .numTransitions = 3,
    },
};

/* Public application code ---------------------------------------------------*/

void *state402_thread(void *arg)
{
    (void)arg;

    printf("[State402] Thread started\n");

    /* 初始化状态机 */
    stateM_init(&fsm, &stateLayer[STATE402_STATE_INITIALIZATION],
                &stateLayer[STATE402_STATE_FAULT]);
    printf("[State402] State machine initialized\n");

    while (!state402_ctx.exit_app) {
        State402_Cmd_t cmd;
        struct event ev;

        memset(&cmd, 0, sizeof(cmd));
        ev.type = STATE402_EV_NONE;
        ev.data = NULL;

        /* 从队列接收命令，10ms超时 */
        if (dequeue(&state402_cmdQueue, &cmd, 10) == 0) {
            /* 命令到事件的转换 */
            switch (cmd.type) {
                case STATE402_CMD_ENABLE:
                    ev.type = STATE402_EV_ENABLE_REQUESTED;
                    printf("[State402] Received: ENABLE\n");
                    break;
                case STATE402_CMD_DISABLE:
                    ev.type = STATE402_EV_DISABLE_REQUESTED;
                    printf("[State402] Received: DISABLE\n");
                    break;
                case STATE402_CMD_REPORT_FAULT:
                    /* 模拟底层上报故障 */
                    state402_ctx.fault_active = 1;
                    state402_ctx.current_fault = cmd.data.fault_type;
                    ev.type = STATE402_EV_FAULT_ACTIVE;
                    printf("[State402] HW Report: FAULT (type=%d)\n", cmd.data.fault_type);
                    break;
                case STATE402_CMD_REPORT_CLEAR:
                    /* 模拟底层上报故障清除 */
                    state402_ctx.fault_active = 0;
                    state402_ctx.current_fault = STATE402_FAULT_NONE;
                    printf("[State402] HW Report: FAULT_CLEARED\n");
                    break;
                case STATE402_CMD_FAULT_RESET:
                    /* 检查故障是否已清除 */
                    if (!state402_ctx.fault_active) {
                        ev.type = STATE402_EV_FAULT_RESET_REQUESTED;
                        printf("[State402] Received: FAULT_RESET (fault cleared)\n");
                    } else {
                        printf("[State402] Received: FAULT_RESET (fault still active, ignored)\n");
                    }
                    break;
                default:
                    ev.type = STATE402_EV_NONE;
                    break;
            }
        }

        /* 处理事件或发送CYCLE维持状态机 */
        if (ev.type != STATE402_EV_NONE) {
            stateM_handleEvent(&fsm, &ev);
        } else {
            ev.type = STATE402_EV_CYCLE;
            stateM_handleEvent(&fsm, &ev);
        }

        /* 10ms周期 */
        usleep(10000);
    }

    printf("[State402] Thread exiting\n");
    return NULL;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */
