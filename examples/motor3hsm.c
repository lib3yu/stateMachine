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
/* Private variables ---------------------------------------------------------*/

/* Private define 1 ----------------------------------------------------------*/
/* Private macro 1 -----------------------------------------------------------*/
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
        memset(&cmd, 0, sizeof(cmd));

        /* 从队列接收命令，10ms超时 */
        if (dequeue(&cmdQueue, &cmd, 10) == 0) {
            printf("[Motor] Received cmd: %s\n", cmd_name(cmd.type));
            /* TODO: 转换为事件并处理状态机 */
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
