/**
  ******************************************************************************
  * File Name          : cia402StateMachine.c
  * Description        : source for template of cia402 state machine
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
#include "queue.h"
#include "stateMachine.h"
/* Private includes ----------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* Private define 0 ----------------------------------------------------------*/

/* -------------------------------------------------------------------------- */
/* CiA 402 Controlword (0x6040) Bit Definitions                               */
/* -------------------------------------------------------------------------- */
#define CW_BIT_SWITCH_ON        (1 << 0)  /* 位0：开启开关 */
#define CW_BIT_ENABLE_VOLTAGE   (1 << 1)  /* 位1：使能电压 */
#define CW_BIT_QUICK_STOP       (1 << 2)  /* 位2：快速停止 */
#define CW_BIT_ENABLE_OPERATION (1 << 3)  /* 位3：使能操作 */
#define CW_BIT_SPEC_1           (1 << 4)  /* 位4：操作模式特定位1 */
#define CW_BIT_SPEC_2           (1 << 5)  /* 位5：操作模式特定位2 */
#define CW_BIT_SPEC_3           (1 << 6)  /* 位6：操作模式特定位3 */
#define CW_BIT_FAULT_RESET      (1 << 7)  /* 位7：故障复位（上升沿有效） */
#define CW_BIT_HALT             (1 << 8)  /* 位8：暂停 */

/* -------------------------------------------------------------------------- */
/* CiA 402 Statusword (0x6041) Masks & Bits                                   */
/* -------------------------------------------------------------------------- */
/* State Mask: Bits 0, 1, 2, 3, 5, 6 */
#define SW_STATE_MASK               0x006F

/* 状态值（掩码后） */
#define SW_STATE_NOT_READY_TO_SWITCH_ON  0x0000  /* 未准备好开启 */
#define SW_STATE_SWITCH_ON_DISABLED      0x0040  /* 开启禁用 */
#define SW_STATE_READY_TO_SWITCH_ON      0x0021  /* 准备好开启 */
#define SW_STATE_SWITCHED_ON             0x0023  /* 已开启 */
#define SW_STATE_OPERATION_ENABLED       0x0027  /* 操作使能 */
#define SW_STATE_QUICK_STOP_ACTIVE       0x0007  /* 快速停止活动 */
#define SW_STATE_FAULT_REACTION_ACTIVE   0x000F  /* 故障反应活动 */
#define SW_STATE_FAULT                   0x0008  /* 故障状态 */

/* 独立状态位 */
#define SW_BIT_FAULT            (1 << 3)   /* 位3：故障 */
#define SW_BIT_VOLTAGE_ENABLED  (1 << 4)   /* 位4：电压使能 */
#define SW_BIT_WARNING          (1 << 7)   /* 位7：警告 */
#define SW_BIT_REMOTE           (1 << 9)   /* 位9：远程控制 */
#define SW_BIT_TARGET_REACHED   (1 << 10)  /* 位10：目标到达 */
#define SW_BIT_LIMIT_ACTIVE     (1 << 11)  /* 位11：限位激活 */

/* Private macro 0 -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/

typedef enum {
    CiA402_CMD_CTRLWORD,
    CiA402_CMD_MOTION,
    CiA402_CMD_TARGET_TRQ,
    CiA402_CMD_TARGET_VEL,
    CiA402_CMD_TARGET_POS,
    CiA402_CMD_ACC,
    CiA402_CMD_MAX_ACC,
    CiA402_CMD_MAX_VEL,
    CiA402_CMD_PID_PARAM,
} CiA402_Cmd_t;

typedef union {
    int8_t   mode;                 /* Control mode (for SET_MODE) */
    int16_t  targetTorque;         /* Target value (for SET_TARGET_TORQUE) */
    int32_t  targetVelocity;       /* Target value (for SET_TARGET_VELOCITY) */
    int32_t  targetPosition;       /* Target value (for SET_TARGET_POSITION) */
    uint32_t acceleration;         /* Acceleration value (for SET_ACCELERATION) */
    uint32_t maxVelocity;          /* Max velocity value (for SET_MAX_VELOCITY) */
    uint32_t deceleration;         /* Quick stop deceleration (for SET_QUICK_STOP_DEC) */
    uint32_t maxAcceleration;      /* Max acceleration (for SET_MAX_ACCELERATION) */
    uint16_t typi[3];              /* [0]:type(1:TRQ;2:VEL;3:POS;); [1]:param_p;[2]:param_i; */
} CiA402_ParamPayload_t;

/* 内部有限状态自动机状态枚举 */
typedef enum {
    STATE_NOT_READY              = 0,  /* 状态0：未准备好开启 */
    STATE_SWITCH_ON_DISABLED     = 1,  /* 状态1：开启禁用 */
    STATE_READY_TO_SWITCH_ON     = 2,  /* 状态2：准备好开启 */
    STATE_SWITCHED_ON            = 3,  /* 状态3：已开启 */
    STATE_OPERATION_ENABLED      = 4,  /* 状态4：操作使能 */
    STATE_QUICK_STOP_ACTIVE      = 5,  /* 状态5：快速停止活动 */
    STATE_FAULT_REACTION_ACTIVE  = 6,  /* 状态6：故障反应活动 */
    STATE_FAULT                  = 7,  /* 状态7：故障状态 */
    MAX_STATE_NUM                = 8
} CiA402_State_t;

/* 支持的操作模式枚举（对应对象字典0x6060） */
typedef enum {
    OP_MODE_NONE = 0,   /* 模式0：无模式 */
    OP_MODE_PPM  = 1,   /* 模式1：轮廓位置模式（梯形加减速位置控制） */
    OP_MODE_PVM  = 3,   /* 模式3：轮廓速度模式（梯形加减速速度控制） */
    OP_MODE_CSP  = 8,   /* 模式8：循环同步位置模式（周期性位置同步） */
    OP_MODE_CSV  = 9,   /* 模式9：循环同步速度模式（周期性速度同步） */
    OP_MODE_CST  = 10   /* 模式10：循环同步扭矩模式（周期性扭矩同步） */
} CiA402_OpMode_t;

typedef enum {
    CiA402_EV_SHUTDOWN,
    CiA402_EV_SWITCH_ON,
    CiA402_EV_ENABLE_OPERATION,
    CiA402_EV_DISABLE_OPERATION,
    CiA402_EV_DISABLE_VOLTAGE,
    CiA402_EV_QUICK_STOP,
    CiA402_EV_FAULT_RESET,
    CiA402_EV_INTERNAL_FAULT,
} CiA402_Event_t;

/* Private variables ---------------------------------------------------------*/
// Queue instance
static queue_t userCmdQueue;
static queue_t stackUpdateQueue;
static struct state cia_states[MAX_STATE_NUM];

static struct state cia_states[MAX_STATE_NUM] = {
    [STATE_NOT_READY] = { },
    [STATE_SWITCH_ON_DISABLED] = { },
    [STATE_READY_TO_SWITCH_ON] = { },
    [STATE_SWITCHED_ON] = { },
    [STATE_OPERATION_ENABLED] = { },
    [STATE_QUICK_STOP_ACTIVE] = { },
    [STATE_FAULT_REACTION_ACTIVE] = { },
    [STATE_FAULT] = { }
};

/* Private define 1 ----------------------------------------------------------*/
/* Private macro 1 -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private define 2 ----------------------------------------------------------*/
/* Private macro 2 -----------------------------------------------------------*/
/* Private function code -----------------------------------------------------*/
/* Public application code ---------------------------------------------------*/

/* Entry point ---------------------------------------------------------------*/
int main(int argc, char const *argv[])
{
    (void)argc;
    (void)argv;

    printf("Hello World!\n");    
    return 0;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */

