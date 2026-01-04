/**
  ******************************************************************************
  * File Name          : motor3hsm_input.c
  * Description        : Auto script and main entry for motor3hsm
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
#include <pthread.h>

/* Private define 0 ----------------------------------------------------------*/
#define SCRIPT_DELAY_MS 200

/* Private macro 0 -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/

/* 自动脚本步骤结构 */
typedef struct {
    Motor_CmdType_t cmd;
    Motor_ParamPayload_t data;
    uint32_t delay_ms;
    const char *label;
    int fault_flag;
} MotorAutoStep_t;

/* Private variables ---------------------------------------------------------*/
queue_t cmdQueue;
Context_t ctx = {0};

/* Private define 1 ----------------------------------------------------------*/
/* Private macro 1 -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void dispatch_command(Motor_CmdType_t type, Motor_ParamPayload_t data);
static void *auto_flow_thread(void *arg);

/* Private define 2 ----------------------------------------------------------*/
/* Private macro 2 -----------------------------------------------------------*/
/* Private function code -----------------------------------------------------*/

/* 发送命令到队列 */
static void dispatch_command(Motor_CmdType_t type, Motor_ParamPayload_t data)
{
    Motor_Cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = type;
    cmd.data = data;

    while (enqueue(&cmdQueue, &cmd, 100) != 0) {
        if (ctx.exit_app) return;
        usleep(10000);
    }
}

/* 自动脚本线程 */
static void *auto_flow_thread(void *arg)
{
    (void)arg;

    /* 脚本定义：演示完整的电机控制流程 */
    static const MotorAutoStep_t script[] = {
        { MOTOR_CMD_SET_MODE,           .data.mode = 0,        .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 设置PVM模式", .fault_flag = -1 },
        { MOTOR_CMD_SET_ACCELERATION,  .data.acceleration = 200, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 设置加速度200", .fault_flag = -1 },
        { MOTOR_CMD_START,             .data = {0},           .delay_ms = SCRIPT_DELAY_MS * 2, .label = "[Script] 启动电机", .fault_flag = -1 },
        { MOTOR_CMD_SET_TARGET_VELOCITY, .data.targetVelocity = 1200, .delay_ms = SCRIPT_DELAY_MS * 2, .label = "[Script] 设置速度1200", .fault_flag = -1 },
        { MOTOR_CMD_SET_TARGET_POSITION, .data.targetPosition = 500,  .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 设置位置500", .fault_flag = -1 },
        { MOTOR_CMD_NONE,              .data = {0},           .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 注入故障", .fault_flag = 1 },
        { MOTOR_CMD_RST_FAULT,         .data = {0},           .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 复位故障", .fault_flag = 0 },
        { MOTOR_CMD_STOP,              .data = {0},           .delay_ms = SCRIPT_DELAY_MS * 2, .label = "[Script] 停止电机", .fault_flag = -1 },
    };

    const size_t n = sizeof(script) / sizeof(script[0]);
    printf("[Script] Starting auto flow (%zu steps)\n", n);

    for (size_t i = 0; i < n && !ctx.exit_app; i++) {
        const MotorAutoStep_t *step = &script[i];

        /* 延迟 */
        if (step->delay_ms) {
            usleep(step->delay_ms * 1000);
        }

        /* 设置故障标志 */
        if (step->fault_flag >= 0) {
            ctx.fault_active = step->fault_flag;
        }

        /* 打印标签 */
        if (step->label) {
            puts(step->label);
        }

        /* 发送命令 */
        if (step->cmd != MOTOR_CMD_NONE) {
            dispatch_command(step->cmd, step->data);
        }
    }

    /* 等待最后的命令被处理 */
    sleep(1);
    ctx.exit_app = 1;

    printf("[Script] Auto flow finished\n");
    return NULL;
}

/* Public application code ---------------------------------------------------*/

/* Entry point ---------------------------------------------------------------*/
int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== Motor3HSM Example ===\n");

    newqueue(&cmdQueue, sizeof(Motor_Cmd_t), 10);

    pthread_t th_motor, th_auto;
    pthread_create(&th_motor, NULL, motor_thread, NULL);
    pthread_create(&th_auto, NULL, auto_flow_thread, NULL);

    pthread_join(th_auto, NULL);
    pthread_join(th_motor, NULL);
    delequeue(&cmdQueue);

    printf("=== Example Finished ===\n");
    return 0;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */
