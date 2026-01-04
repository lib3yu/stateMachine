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
/* ===== 第一层：域层（显式定义） ===== */
typedef enum {
    MOTOR_DOMAIN_NORMAL_OPERATION,    /* 正常运作域 */
    MOTOR_DOMAIN_FAULT_MANAGEMENT,    /* 故障处理域 */
    MAX_MOTOR_DOMAIN_NUM
} Motor_Domain_t;

/* ===== 第二层：主状态层（分域定义） ===== */
/* 正常运作域 (NORMAL_OPERATION) 的状态 */
typedef enum {
    MOTOR_NORMAL_STATE_POWER_UP,      /* 上电 */
    MOTOR_NORMAL_STATE_INIT,          /* 初始化 */
    MOTOR_NORMAL_STATE_ALIGNMENT,     /* 对齐 */
    MOTOR_NORMAL_STATE_STOPPED,       /* 已停止 */
    MOTOR_NORMAL_STATE_RUNNING,       /* 运行中（复合状态，包含子状态） */
    MOTOR_NORMAL_STATE_STOPPING,      /* 停止中 */
    MAX_MOTOR_NORMAL_NUM
} Motor_NormalState_t;

/* 故障处理域 (FAULT_MANAGEMENT) 的状态 */
typedef enum {
    MOTOR_FAULT_STATE_FAULTING,      /* 故障处理 */
    MOTOR_FAULT_STATE_FAULTED,       /* 故障锁定 */
    MAX_MOTOR_FAULT_NUM
} Motor_FaultState_t;

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
/* 状态数组声明（按域分层） */
/* 正常运作域状态数组 */
static struct state normalStateLayer[MAX_MOTOR_NORMAL_NUM];
/* 故障处理域状态数组 */
static struct state faultStateLayer[MAX_MOTOR_FAULT_NUM];
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
/* 守卫函数声明 */
static bool Guard_PowerGood(void *param, struct event *e);
static bool Guard_InitSuccess(void *param, struct event *e);
static bool Guard_AlignSuccess(void *param, struct event *e);
static bool Guard_IsStopped(void *param, struct event *e);
static bool Guard_FaultActive(void *param, struct event *e);
static bool Guard_SevereError(void *param, struct event *e);

/* Entry动作函数声明 */
static void EnterAction_PowerUp(void *stateData, struct event *e);
static void EnterAction_Init(void *stateData, struct event *e);
static void EnterAction_Alignment(void *stateData, struct event *e);
static void EnterAction_Stopped(void *stateData, struct event *e);
static void EnterAction_Running(void *stateData, struct event *e);
static void EnterAction_Stopping(void *stateData, struct event *e);
static void EnterAction_Faulting(void *stateData, struct event *e);
static void EnterAction_Faulted(void *stateData, struct event *e);

/* 运动模式层Entry/Exit动作函数声明 */
static void EnterAction_MotionPVM(void *stateData, struct event *e);
static void EnterAction_MotionPPM(void *stateData, struct event *e);
static void EnterAction_MotionCSV(void *stateData, struct event *e);
static void EnterAction_MotionCSP(void *stateData, struct event *e);
static void EnterAction_MotionCST(void *stateData, struct event *e);
static void ExitAction_MotionPVM(void *stateData, struct event *e);
static void ExitAction_MotionPPM(void *stateData, struct event *e);
static void ExitAction_MotionCSV(void *stateData, struct event *e);
static void ExitAction_MotionCSP(void *stateData, struct event *e);
static void ExitAction_MotionCST(void *stateData, struct event *e);

/* Private define 2 ----------------------------------------------------------*/
/* Private macro 2 -----------------------------------------------------------*/
/* Private function code -----------------------------------------------------*/

/* ===== 守卫函数实现 ===== */
static bool Guard_PowerGood(void *param, struct event *e) {
    (void)param; (void)e;
    printf("[Guard] 电源良好检查通过\n");
    return true;
}

static bool Guard_InitSuccess(void *param, struct event *e) {
    (void)param; (void)e;
    printf("[Guard] 初始化成功，进入对齐流程\n");
    return true;
}

static bool Guard_AlignSuccess(void *param, struct event *e) {
    (void)param; (void)e;
    printf("[Guard] 对齐成功\n");
    return true;
}

static bool Guard_IsStopped(void *param, struct event *e) {
    (void)param; (void)e;
    printf("[Guard] 速度归零，停止完成\n");
    return true;
}

static bool Guard_FaultActive(void *param, struct event *e) {
    (void)param; (void)e;
    if (ctx.fault_active) {
        printf("[Guard] 检测到故障(Fault_Active/exFault)！\n");
        return true;
    }
    return false;
}

static bool Guard_SevereError(void *param, struct event *e) {
    (void)param; (void)e;
    printf("[Guard] 检测到严重错误，需终止\n");
    return false;
}

/* ===== Entry动作函数实现 ===== */
static void EnterAction_PowerUp(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入上电状态 (硬件自检与电源检查)\n");
}

static void EnterAction_Init(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入初始化状态 (引导初始化:外设/参数)\n");
}

static void EnterAction_Alignment(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入对齐状态 (电机相位对齐)\n");
}

static void EnterAction_Stopped(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入停止状态 (伺服停止/待机)\n");
}

static void EnterAction_Running(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入运行状态\n");
}

static void EnterAction_Stopping(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入停止中状态 (减速停车中/抱闸介入)\n");
}

static void EnterAction_Faulting(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入故障处理状态 (紧急动作:切断输出/抱闸锁死)\n");
}

static void EnterAction_Faulted(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf(">> [State] 进入故障锁定状态 (等待外部干预)\n");
}

/* ===== 运动模式层Entry/Exit动作函数实现 ===== */
static void EnterAction_MotionPVM(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 进入轮廓速度模式(PVM)\n");
}

static void EnterAction_MotionPPM(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 进入轮廓位置模式(PPM)\n");
}

static void EnterAction_MotionCSV(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 进入循环速度模式(CSV)\n");
}

static void EnterAction_MotionCSP(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 进入循环位置模式(CSP)\n");
}

static void EnterAction_MotionCST(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 进入循环力矩模式(CST)\n");
}

static void ExitAction_MotionPVM(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 退出轮廓速度模式(PVM)\n");
}

static void ExitAction_MotionPPM(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 退出轮廓位置模式(PPM)\n");
}

static void ExitAction_MotionCSV(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 退出循环速度模式(CSV)\n");
}

static void ExitAction_MotionCSP(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 退出循环位置模式(CSP)\n");
}

static void ExitAction_MotionCST(void *stateData, struct event *e) {
    (void)stateData; (void)e;
    printf("  [Motion] 退出循环力矩模式(CST)\n");
}

/* ===== 运动模式层状态数组初始化 ===== */
/* 注意：由于normalStateLayer在此时尚未初始化，这里使用NULL作为parentState占位 */
/* 在运行时通过init_context()函数设置正确的parentState指针 */
static struct state motionLayer[MAX_MOTOR_MOTION_NUM] = {
    /* 轮廓速度模式 (PVM) - 默认模式 */
    [MOTOR_MOTION_PVM] = {
        .parentState = NULL,  /* 运行时设置为&normalStateLayer[MOTOR_NORMAL_STATE_RUNNING] */
        .data = (void*)"PVM",
        .entryState = NULL,
        .entryAction = EnterAction_MotionPVM,
        .exitAction = ExitAction_MotionPVM,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, Guard_FaultActive, NULL, NULL },  /* nextState运行时设置 */
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_PVM] }
        },
        .numTransitions = 2,
    },
    /* 轮廓位置模式 (PPM) */
    [MOTOR_MOTION_PPM] = {
        .parentState = NULL,  /* 运行时设置 */
        .data = (void*)"PPM",
        .entryState = NULL,
        .entryAction = EnterAction_MotionPPM,
        .exitAction = ExitAction_MotionPPM,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, Guard_FaultActive, NULL, NULL },  /* 运行时设置 */
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_PPM] }
        },
        .numTransitions = 2,
    },
    /* 循环速度模式 (CSV) */
    [MOTOR_MOTION_CSV] = {
        .parentState = NULL,  /* 运行时设置 */
        .data = (void*)"CSV",
        .entryState = NULL,
        .entryAction = EnterAction_MotionCSV,
        .exitAction = ExitAction_MotionCSV,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, Guard_FaultActive, NULL, NULL },  /* 运行时设置 */
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_CSV] }
        },
        .numTransitions = 2,
    },
    /* 循环位置模式 (CSP) */
    [MOTOR_MOTION_CSP] = {
        .parentState = NULL,  /* 运行时设置 */
        .data = (void*)"CSP",
        .entryState = NULL,
        .entryAction = EnterAction_MotionCSP,
        .exitAction = ExitAction_MotionCSP,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, Guard_FaultActive, NULL, NULL },  /* 运行时设置 */
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_CSP] }
        },
        .numTransitions = 2,
    },
    /* 循环力矩模式 (CST) */
    [MOTOR_MOTION_CST] = {
        .parentState = NULL,  /* 运行时设置 */
        .data = (void*)"CST",
        .entryState = NULL,
        .entryAction = EnterAction_MotionCST,
        .exitAction = ExitAction_MotionCST,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, Guard_FaultActive, NULL, NULL },  /* 运行时设置 */
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &motionLayer[MOTOR_MOTION_CST] }
        },
        .numTransitions = 2,
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
