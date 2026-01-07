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
/* ===== 状态枚举（统一） ===== */
typedef enum {
    MOTOR_STATE_POWER_UP,      /* 上电 */
    MOTOR_STATE_INIT,          /* 初始化 */
    MOTOR_STATE_ALIGNMENT,     /* 对齐 */
    MOTOR_STATE_STOPPED,       /* 已停止 */
    MOTOR_STATE_RUNNING,       /* 运行中 */
    MOTOR_STATE_STOPPING,      /* 停止中 */
    MOTOR_STATE_FAULTED,       /* 故障锁定 */
    MAX_MOTOR_STATE_NUM = 7
} Motor_State_t;

/* 事件类型（内部实现） */
typedef enum {
    MOTOR_EV_NONE,
    MOTOR_EV_CYCLE,
    MOTOR_EV_FAULT_ACTIVE, /* 故障激活事件（自动注入） */
    MOTOR_EV_START_REQUESTED,
    MOTOR_EV_STOP_REQUESTED,
    MOTOR_EV_FAULT_RESET_REQUESTED,
    MOTOR_EV_MODE_CHANGE_REQUESTED,
    MOTOR_EV_PARAM_UPDATE_REQUESTED,
} Motor_Event_t;

/* Private variables ---------------------------------------------------------*/
/* 状态机实例 */
static struct stateMachine fsm;

/* 状态数组声明 */
static struct state stateLayer[MAX_MOTOR_STATE_NUM];
/* 运动模式层状态数组（RUNNING的子状态） */
static struct state motionLayer[MAX_MOTOR_MOTION_NUM];

/* Private define 1 ----------------------------------------------------------*/
/* Private macro 1 -----------------------------------------------------------*/

/* 参数转名称（用于调试打印）*/
#define _param2str(enum_)   \
    (enum_ == MOTOR_PARAM_TARGET_VELOCITY)  ? "目标速度" :     \
    (enum_ == MOTOR_PARAM_TARGET_POSITION) ? "目标位置" :       \
    (enum_ == MOTOR_PARAM_ACCELERATION)     ? "加速度" : "未知参数"

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* 故障检查函数 */
static int CheckFaultActive(struct stateMachine *fsm);

/* 守卫函数声明 */
static bool Guard_PowerGood(struct stateMachine *fsm, void *param, struct event *e);
static bool Guard_InitSuccess(struct stateMachine *fsm, void *param, struct event *e);
static bool Guard_AlignSuccess(struct stateMachine *fsm, void *param, struct event *e);
static bool Guard_IsStopped(struct stateMachine *fsm, void *param, struct event *e);
static bool Guard_CanChangeMode(struct stateMachine *fsm, void *param, struct event *e);

/* Entry动作函数声明 */
static void EnterAction_PowerUp(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_Init(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_Alignment(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_Stopped(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_Running(struct stateMachine *fsm, void *stateData, struct event *e);
static void ExitAction_Running(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_Stopping(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_Faulted(struct stateMachine *fsm, void *stateData, struct event *e);

/* Transition 动作函数声明 */
static void Action_PrepareModeChange(struct stateMachine *fsm, void *currentStateData, struct event *event, void *newStateData);
static void Action_UpdateParams(struct stateMachine *fsm, void *currentStateData, struct event *event, void *newStateData);

/* 运动模式层Entry/Exit动作函数声明 */
static void EnterAction_MotionPVM(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_MotionPPM(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_MotionCSV(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_MotionCSP(struct stateMachine *fsm, void *stateData, struct event *e);
static void EnterAction_MotionCST(struct stateMachine *fsm, void *stateData, struct event *e);
static void ExitAction_MotionPVM(struct stateMachine *fsm, void *stateData, struct event *e);
static void ExitAction_MotionPPM(struct stateMachine *fsm, void *stateData, struct event *e);
static void ExitAction_MotionCSV(struct stateMachine *fsm, void *stateData, struct event *e);
static void ExitAction_MotionCSP(struct stateMachine *fsm, void *stateData, struct event *e);
static void ExitAction_MotionCST(struct stateMachine *fsm, void *stateData, struct event *e);

/* Private define 2 ----------------------------------------------------------*/
/* Private macro 2 -----------------------------------------------------------*/
/* Private function code -----------------------------------------------------*/

/* ===== 故障检查函数实现 ===== */
/* 返回1表示故障激活，0表示无故障 */
static int CheckFaultActive(struct stateMachine *fsm) {
    Context_t *ctx = (Context_t *)fsm->userData;
    if (ctx->fault_active) {
        // printf("[CheckFaultActive] 检测到故障！\n");
        return 1;
    }
    return 0;
}

/* ===== 守卫函数实现 ===== */
static bool Guard_PowerGood(struct stateMachine *fsm, void *param, struct event *e) {
    (void)fsm; (void)param; (void)e;
    printf("[Guard] 电源良好检查通过\n");
    return true;
}

static bool Guard_InitSuccess(struct stateMachine *fsm, void *param, struct event *e) {
    (void)fsm; (void)param; (void)e;
    printf("[Guard] 初始化成功，进入对齐流程\n");
    return true;
}

static bool Guard_AlignSuccess(struct stateMachine *fsm, void *param, struct event *e) {
    (void)fsm; (void)param; (void)e;
    printf("[Guard] 对齐成功\n");
    return true;
}

static bool Guard_IsStopped(struct stateMachine *fsm, void *param, struct event *e) {
    (void)fsm; (void)param; (void)e;
    printf("[Guard] 速度归零，停止完成\n");
    return true;
}

/* ===== Entry动作函数实现 ===== */
static void EnterAction_PowerUp(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf(">> [State] 进入上电状态 (硬件自检与电源检查)\n");
}

static void EnterAction_Init(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf(">> [State] 进入初始化状态 (引导初始化:外设/参数)\n");
}

static void EnterAction_Alignment(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf(">> [State] 进入对齐状态 (电机相位对齐)\n");
}

static void EnterAction_Stopped(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf(">> [State] 进入停止状态 (伺服停止/待机)\n");
}

static void EnterAction_Running(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf(">> [State] 进入运行状态\n");
}

static void EnterAction_Stopping(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf(">> [State] 进入停止中状态 (减速停车中/抱闸介入)\n");
}

static void EnterAction_Faulted(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf(">> [State] 进入故障锁定状态 (等待外部干预)\n");
}

/* ===== Exit 和 Transition 动作函数实现 ===== */

static bool Guard_CanChangeMode(struct stateMachine *fsm, void *param, struct event *e) {
    (void)param;
    printf("[Guard] 检查模式切换条件...");
    Context_t *ctx = (Context_t *)fsm->userData;
    if (ctx->fault_active) {
        printf("不允许：存在故障\n");
        return false;
    }
    Motor_MotionMode_t newMode = *(Motor_MotionMode_t*)e->data;
    if (newMode == ctx->lastMotion) {
        printf("不允许：目标模式与当前模式相同\n");
        return false;
    }
    printf("允许\n");
    return true;
}

static void ExitAction_Running(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)stateData; (void)e;
    Context_t *ctx = (Context_t *)fsm->userData;
    printf("<< [State] 退出运行状态。当前运动模式: %d\n", ctx->lastMotion);
}

static void Action_PrepareModeChange(struct stateMachine *fsm, void *currentStateData, struct event *e, void *newStateData) {
    (void)currentStateData; (void)newStateData;
    Context_t *ctx = (Context_t *)fsm->userData;
    Motor_MotionMode_t newMode = *(Motor_MotionMode_t*)e->data;
    printf("[Action] 准备切换到运动模式 %d\n", newMode);
    ctx->pendingMotion = newMode;
}

static void Action_UpdateParams(struct stateMachine *fsm, void *currentStateData, struct event *e, void *newStateData) {
    (void)fsm; (void)currentStateData; (void)newStateData;
    Motor_Param_t *param = (Motor_Param_t *)e->data;
    if (!param) return;
    printf("[Action] 参数更新: 类型=%s\n", _param2str(param->type));
}

/* ===== 运动模式层Entry/Exit动作函数实现 ===== */
static void EnterAction_MotionPVM(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 进入轮廓速度模式(PVM)\n");
}

static void EnterAction_MotionPPM(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 进入轮廓位置模式(PPM)\n");
}

static void EnterAction_MotionCSV(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 进入循环速度模式(CSV)\n");
}

static void EnterAction_MotionCSP(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 进入循环位置模式(CSP)\n");
}

static void EnterAction_MotionCST(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 进入循环力矩模式(CST)\n");
}

static void ExitAction_MotionPVM(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 退出轮廓速度模式(PVM)\n");
}

static void ExitAction_MotionPPM(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 退出轮廓位置模式(PPM)\n");
}

static void ExitAction_MotionCSV(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 退出循环速度模式(CSV)\n");
}

static void ExitAction_MotionCSP(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 退出循环位置模式(CSP)\n");
}

static void ExitAction_MotionCST(struct stateMachine *fsm, void *stateData, struct event *e) {
    (void)fsm; (void)stateData; (void)e;
    printf("  [Motion] 退出循环力矩模式(CST)\n");
}

/* ===== 状态数组初始化 ===== */
static struct state stateLayer[MAX_MOTOR_STATE_NUM] = {
    /* POWER_UP：上电状态 */
    [MOTOR_STATE_POWER_UP] = {
        .parentState = NULL,
        .data = NULL,
        .entryState = NULL,
        .entryAction = EnterAction_PowerUp,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED] },
            { MOTOR_EV_CYCLE, NULL, Guard_PowerGood, NULL, &stateLayer[MOTOR_STATE_INIT] },
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_POWER_UP] }
        },
        .numTransitions = 3,
    },
    /* INIT：初始化状态 */
    [MOTOR_STATE_INIT] = {
        .parentState = NULL,
        .data = NULL,
        .entryState = NULL,
        .entryAction = EnterAction_Init,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED] },
            { MOTOR_EV_CYCLE, NULL, Guard_InitSuccess, NULL, &stateLayer[MOTOR_STATE_ALIGNMENT] },
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_INIT] }
        },
        .numTransitions = 3,
    },
    /* ALIGNMENT：对齐状态 */
    [MOTOR_STATE_ALIGNMENT] = {
        .parentState = NULL,
        .data = NULL,
        .entryState = NULL,
        .entryAction = EnterAction_Alignment,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED] },
            { MOTOR_EV_CYCLE, NULL, Guard_AlignSuccess, NULL, &stateLayer[MOTOR_STATE_STOPPED] },
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_ALIGNMENT] }
        },
        .numTransitions = 3,
    },
    /* STOPPED：已停止状态 */
    [MOTOR_STATE_STOPPED] = {
        .parentState = NULL,
        .data = NULL,
        .entryState = NULL,
        .entryAction = EnterAction_Stopped,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED] },
            { MOTOR_EV_START_REQUESTED, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_RUNNING] },
            { MOTOR_EV_PARAM_UPDATE_REQUESTED, NULL, NULL, Action_UpdateParams, &stateLayer[MOTOR_STATE_STOPPED] },
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_STOPPED] }
        },
        .numTransitions = 4,
    },
    /* RUNNING：运行状态 */
    [MOTOR_STATE_RUNNING] = {
        .parentState = NULL,
        .data = NULL,
        .entryState = NULL,
        .entryAction = EnterAction_Running,
        .exitAction = ExitAction_Running,
        .transitions = (struct transition[]){
            { MOTOR_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED] },
            { MOTOR_EV_STOP_REQUESTED, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_STOPPING] },
            { MOTOR_EV_MODE_CHANGE_REQUESTED, NULL, Guard_CanChangeMode, Action_PrepareModeChange, &stateLayer[MOTOR_STATE_RUNNING] },
            { MOTOR_EV_PARAM_UPDATE_REQUESTED, NULL, NULL, Action_UpdateParams, &stateLayer[MOTOR_STATE_RUNNING] },
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_RUNNING] }
        },
        .numTransitions = 5,
    },
    /* STOPPING：停止中状态 */
    [MOTOR_STATE_STOPPING] = {
        .parentState = NULL,
        .data = NULL,
        .entryState = NULL,
        .entryAction = EnterAction_Stopping,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_FAULT_ACTIVE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED] },
            { MOTOR_EV_CYCLE, NULL, Guard_IsStopped, NULL, &stateLayer[MOTOR_STATE_STOPPED] },
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_STOPPING] }
        },
        .numTransitions = 3,
    },
    /* FAULTED：故障锁定状态 */
    [MOTOR_STATE_FAULTED] = {
        .parentState = NULL,
        .data = NULL,
        .entryState = NULL,
        .entryAction = EnterAction_Faulted,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_FAULT_RESET_REQUESTED, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_INIT] },
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED] }
        },
        .numTransitions = 2,
    },
};

/* ===== 运动模式层状态数组初始化 ===== */
/* 注意：stateLayer 已在本文件之前定义，此处可直接静态引用 &stateLayer[MOTOR_STATE_RUNNING] */
/* 故障事件会自动冒泡到父状态RUNNING处理 */
static struct state motionLayer[MAX_MOTOR_MOTION_NUM] = {
    /* 轮廓速度模式 (PVM) - 默认模式 */
    [MOTOR_MOTION_PVM] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .data = (void*)"PVM",
        .entryState = NULL,
        .entryAction = EnterAction_MotionPVM,
        .exitAction = ExitAction_MotionPVM,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_PVM] }  /* 自循环维持当前模式 */
        },
        .numTransitions = 1,
    },
    /* 轮廓位置模式 (PPM) */
    [MOTOR_MOTION_PPM] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .data = (void*)"PPM",
        .entryState = NULL,
        .entryAction = EnterAction_MotionPPM,
        .exitAction = ExitAction_MotionPPM,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_PPM] }
        },
        .numTransitions = 1,
    },
    /* 循环速度模式 (CSV) */
    [MOTOR_MOTION_CSV] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .data = (void*)"CSV",
        .entryState = NULL,
        .entryAction = EnterAction_MotionCSV,
        .exitAction = ExitAction_MotionCSV,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_CSV] }
        },
        .numTransitions = 1,
    },
    /* 循环位置模式 (CSP) */
    [MOTOR_MOTION_CSP] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .data = (void*)"CSP",
        .entryState = NULL,
        .entryAction = EnterAction_MotionCSP,
        .exitAction = ExitAction_MotionCSP,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_CSP] }
        },
        .numTransitions = 1,
    },
    /* 循环力矩模式 (CST) */
    [MOTOR_MOTION_CST] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .data = (void*)"CST",
        .entryState = NULL,
        .entryAction = EnterAction_MotionCST,
        .exitAction = ExitAction_MotionCST,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_CST] }
        },
        .numTransitions = 1,
    },
};

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

    /* 初始化状态机 */
    stateM_init(&fsm, &stateLayer[MOTOR_STATE_POWER_UP],
                &stateLayer[MOTOR_STATE_FAULTED], &ctx);
    printf("[Motor] State machine initialized\n");

    while (!ctx.exit_app) {
        Motor_Cmd_t cmd;
        struct event ev;

        memset(&cmd, 0, sizeof(cmd));
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
                    ev.data = &cmd.data.motion;
                    printf("[Motor] Received: %s, mode=%d\n", cmd_name(cmd.type), cmd.data.motion);
                    break;
                case MOTOR_CMD_SET_TARGET_VELOCITY: {
                    ctx.current_param.type = MOTOR_PARAM_TARGET_VELOCITY;
                    ctx.current_param.val = cmd.data;
                    ev.type = MOTOR_EV_PARAM_UPDATE_REQUESTED;
                    ev.data = &ctx.current_param;
                    printf("[Motor] Received: %s %s=%d\n", cmd_name(cmd.type),
                           _param2str(ctx.current_param.type), cmd.data.targetVelocity);
                    break;
                }
                case MOTOR_CMD_SET_TARGET_POSITION: {
                    ctx.current_param.type = MOTOR_PARAM_TARGET_POSITION;
                    ctx.current_param.val = cmd.data;
                    ev.type = MOTOR_EV_PARAM_UPDATE_REQUESTED;
                    ev.data = &ctx.current_param;
                    printf("[Motor] Received: %s %s=%d\n", cmd_name(cmd.type),
                           _param2str(ctx.current_param.type), cmd.data.targetPosition);
                    break;
                }
                case MOTOR_CMD_SET_ACCELERATION: {
                    ctx.current_param.type = MOTOR_PARAM_ACCELERATION;
                    ctx.current_param.val = cmd.data;
                    ev.type = MOTOR_EV_PARAM_UPDATE_REQUESTED;
                    ev.data = &ctx.current_param;
                    printf("[Motor] Received: %s %s=%u\n", cmd_name(cmd.type),
                           _param2str(ctx.current_param.type), cmd.data.acceleration);
                    break;
                }
                default:
                    ev.type = MOTOR_EV_NONE;
                    break;
            }
        }

        /* 故障检查优先
         * 故障检测放在主循环而非守卫函数的原因：
         * 1. 故障需要立即响应，优先级高于所有命令事件
         * 2. 避免在每个状态的转换中都重复定义故障守卫
         * 3. 简化 motionLayer，故障事件自动冒泡到父状态RUNNING处理
         */
        /* 故障检查优先处理 */
        if (CheckFaultActive(&fsm)) {
            ev.type = MOTOR_EV_FAULT_ACTIVE;
            ev.data = NULL;
            stateM_handleEvent(&fsm, &ev);
        } else if (ev.type != MOTOR_EV_NONE) {
            /* 有命令事件 */
            stateM_handleEvent(&fsm, &ev);
        } else {
            /* 无命令：发送CYCLE事件维持状态机运行 */
            ev.type = MOTOR_EV_CYCLE;
            ev.data = NULL;
            stateM_handleEvent(&fsm, &ev);
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
