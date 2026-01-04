/**
  ******************************************************************************
  * File Name          : motor3hsm.c
  * Description        : Motor state machine core implementation
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
#include "motor3hsm.h"

/* Private includes ----------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

/* Private define 0 ----------------------------------------------------------*/
/* Private macro 0 -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/

/* 事件类型（内部实现） */
typedef enum {
    MOTOR_EV_NONE,
    MOTOR_EV_CYCLE,
    MOTOR_EV_START_REQUESTED,
    MOTOR_EV_STOP_REQUESTED,
    MOTOR_EV_FAULT_RESET_REQUESTED,
    MOTOR_EV_MODE_CHANGE_REQUESTED,
    MOTOR_EV_PARAM_UPDATE_REQUESTED,
} Motor_Event_t;

/* 参数类型枚举 */
typedef enum {
    MOTOR_PARAM_TARGET_VELOCITY,
    MOTOR_PARAM_TARGET_POSITION,
    MOTOR_PARAM_ACCELERATION,
} Motor_ParamType_t;

/* 参数值结构体 */
typedef struct {
    Motor_ParamType_t type;
    Motor_ParamPayload_t val;
} Motor_Param_t;

/* Private variables ---------------------------------------------------------*/

/* Private define 1 ----------------------------------------------------------*/
/* Private macro 1 -----------------------------------------------------------*/

/* 参数转名称（用于调试打印）*/
#define _param2str(enum_)   \
    (enum_ == MOTOR_PARAM_TARGET_VELOCITY)  ? "目标速度" :     \
    (enum_ == MOTOR_PARAM_TARGET_POSITION) ? "目标位置" :       \
    (enum_ == MOTOR_PARAM_ACCELERATION)     ? "加速度" : "未知参数"

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private define 2 ----------------------------------------------------------*/
/* Private macro 2 -----------------------------------------------------------*/
/* Private function code -----------------------------------------------------*/

/* 命令转名称（用于调试打印）*/
static const char *cmd_name(Motor_CmdType_t type)
{
    switch (type) {
        case MOTOR_CMD_START: return "START";
        case MOTOR_CMD_STOP: return "STOP";
        case MOTOR_CMD_RST_FAULT: return "RST_FAULT";
        case MOTOR_CMD_SET_MODE: return "SET_MODE";
        case MOTOR_CMD_SET_TARGET_VELOCITY: return "SET_VELOCITY";
        case MOTOR_CMD_SET_TARGET_POSITION: return "SET_POSITION";
        case MOTOR_CMD_SET_ACCELERATION: return "SET_ACCELERATION";
        default: return "UNKNOWN";
    }
}

/* Public application code ---------------------------------------------------*/

void *motor_thread(void *arg)
{
    (void)arg;

    printf("[Motor] Thread started\n");

    while (!ctx.exit_app) {
        Motor_Cmd_t cmd;
        Motor_Param_t param;
        struct event ev;

        memset(&cmd, 0, sizeof(cmd));
        memset(&param, 0, sizeof(param));
        ev.type = MOTOR_EV_NONE;
        ev.data = NULL;

        /* 从队列接收命令，10ms超时 */
        if (dequeue(&cmdQueue, &cmd, 10) == 0) {
            /* 命令到事件的转换 */
            switch (cmd.type) {
                case MOTOR_CMD_START:
                    ev.type = MOTOR_EV_START_REQUESTED;
                    printf("[Motor] Received: %s\n", cmd_name(cmd.type));
                    break;
                case MOTOR_CMD_STOP:
                    ev.type = MOTOR_EV_STOP_REQUESTED;
                    printf("[Motor] Received: %s\n", cmd_name(cmd.type));
                    break;
                case MOTOR_CMD_RST_FAULT:
                    ev.type = MOTOR_EV_FAULT_RESET_REQUESTED;
                    printf("[Motor] Received: %s\n", cmd_name(cmd.type));
                    break;
                case MOTOR_CMD_SET_MODE:
                    ev.type = MOTOR_EV_MODE_CHANGE_REQUESTED;
                    ev.data = &cmd.data.mode;
                    printf("[Motor] Received: %s, mode=%d\n", cmd_name(cmd.type), cmd.data.mode);
                    break;
                case MOTOR_CMD_SET_TARGET_VELOCITY: {
                    static Motor_Param_t param;
                    param.type = MOTOR_PARAM_TARGET_VELOCITY;
                    param.val = cmd.data;
                    ev.type = MOTOR_EV_PARAM_UPDATE_REQUESTED;
                    ev.data = &param;
                    printf("[Motor] Received: %s %s=%d\n", cmd_name(cmd.type),
                           _param2str(param.type), cmd.data.targetVelocity);
                    break;
                }
                case MOTOR_CMD_SET_TARGET_POSITION: {
                    static Motor_Param_t param;
                    param.type = MOTOR_PARAM_TARGET_POSITION;
                    param.val = cmd.data;
                    ev.type = MOTOR_EV_PARAM_UPDATE_REQUESTED;
                    ev.data = &param;
                    printf("[Motor] Received: %s %s=%d\n", cmd_name(cmd.type),
                           _param2str(param.type), cmd.data.targetPosition);
                    break;
                }
                case MOTOR_CMD_SET_ACCELERATION: {
                    static Motor_Param_t param;
                    param.type = MOTOR_PARAM_ACCELERATION;
                    param.val = cmd.data;
                    ev.type = MOTOR_EV_PARAM_UPDATE_REQUESTED;
                    ev.data = &param;
                    printf("[Motor] Received: %s %s=%u\n", cmd_name(cmd.type),
                           _param2str(param.type), cmd.data.acceleration);
                    break;
                }
                default:
                    ev.type = MOTOR_EV_NONE;
                    break;
            }
            /* TODO: stateM_handleEvent(&fsm, &ev); */
        }

        /* 无命令时发送 CYCLE 事件 */
        if (ev.type == MOTOR_EV_NONE) {
            /* printf("[Motor] Cycle\n"); */
            /* TODO: ev.type = MOTOR_EV_CYCLE; stateM_handleEvent(&fsm, &ev); */
        }

        /* 10ms周期 */
        usleep(10000);
    }

    printf("[Motor] Thread exiting\n");
    return NULL;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */
