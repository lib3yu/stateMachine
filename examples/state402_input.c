/**
  ******************************************************************************
  * File Name          : state402_input.c
  * Description        : Auto script and main entry for state402
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
#include <pthread.h>

/* Private define 0 ----------------------------------------------------------*/
#define SCRIPT_DELAY_MS 200

/* Private macro 0 -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/

/* 自动脚本步骤结构 */
typedef struct {
    State402_CmdType_t cmd;
    State402_ParamPayload_t data;
    uint32_t delay_ms;
    const char *label;
} State402_AutoStep_t;

/* Private variables ---------------------------------------------------------*/
queue_t state402_cmdQueue;
State402_Context_t state402_ctx = {0};

/* Private define 1 ----------------------------------------------------------*/
/* Private macro 1 -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void dispatch_command(State402_CmdType_t type, State402_ParamPayload_t data);
static void *auto_flow_thread(void *arg);

/* Private define 2 ----------------------------------------------------------*/
/* Private macro 2 -----------------------------------------------------------*/
/* Private function code -----------------------------------------------------*/

/* 发送命令到队列 */
static void dispatch_command(State402_CmdType_t type, State402_ParamPayload_t data)
{
    State402_Cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = type;
    cmd.data = data;

    while (enqueue(&state402_cmdQueue, &cmd, 100) != 0) {
        if (state402_ctx.exit_app) return;
        usleep(10000);
    }
}

/* 自动脚本线程 */
static void *auto_flow_thread(void *arg)
{
    (void)arg;

    /* 脚本定义：演示完整的 CiA402 控制流程 */
    static const State402_AutoStep_t script[] = {
        /* 正常流程：上电 → 初始化 → 待机 */
        { STATE402_CMD_NONE, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 1. 自检通过" },

        /* 使能测试 */
        { STATE402_CMD_ENABLE, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 2. Enable → Operation Enabled" },
        { STATE402_CMD_DISABLE, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 3. Disable → Ready" },

        /* 运行态故障测试 */
        { STATE402_CMD_ENABLE, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 4. 重新 Enable" },
        { STATE402_CMD_INJECT_FAULT, .data.fault_type = STATE402_FAULT_OVERCURRENT, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 5. 注入过流故障" },

        /* 故障恢复流程 */
        { STATE402_CMD_FAULT_RESET, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 6. 尝试复位（应失败）" },
        { STATE402_CMD_CLEAR_FAULT, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 7. 清除故障" },
        { STATE402_CMD_FAULT_RESET, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 8. 复位到Ready" },

        /* 待机态故障测试 */
        { STATE402_CMD_INJECT_FAULT, .data.fault_type = STATE402_FAULT_EMERGENCY_STOP, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 9. 急停触发" },
        { STATE402_CMD_CLEAR_FAULT, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 10. 解除急停" },
        { STATE402_CMD_FAULT_RESET, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 11. 故障复位" },

        /* 最终退出 */
        { STATE402_CMD_NONE, .data = {0}, .delay_ms = SCRIPT_DELAY_MS, .label = "[Script] 12. 测试结束" },
    };

    const size_t n = sizeof(script) / sizeof(script[0]);
    printf("[Script] Starting auto flow (%zu steps)\n", n);

    for (size_t i = 0; i < n && !state402_ctx.exit_app; i++) {
        const State402_AutoStep_t *step = &script[i];

        /* 延迟 */
        if (step->delay_ms) {
            usleep(step->delay_ms * 1000);
        }

        /* 打印标签 */
        if (step->label) {
            puts(step->label);
        }

        /* 发送命令 */
        if (step->cmd != STATE402_CMD_NONE) {
            dispatch_command(step->cmd, step->data);
        }
    }

    /* 等待最后的命令被处理 */
    sleep(1);
    state402_ctx.exit_app = 1;

    printf("[Script] Auto flow finished\n");
    return NULL;
}

/* Public application code ---------------------------------------------------*/

/* Entry point ---------------------------------------------------------------*/
int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== State402 Example (CiA402 Simplified) ===\n");

    newqueue(&state402_cmdQueue, sizeof(State402_Cmd_t), 10);

    pthread_t th_state402, th_auto;
    pthread_create(&th_state402, NULL, state402_thread, NULL);
    pthread_create(&th_auto, NULL, auto_flow_thread, NULL);

    pthread_join(th_auto, NULL);
    pthread_join(th_state402, NULL);
    delequeue(&state402_cmdQueue);

    printf("=== Example Finished ===\n");
    return 0;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */
