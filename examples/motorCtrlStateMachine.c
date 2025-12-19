/* 

```mermaid
 stateDiagram-v2
    [*] --> POWER_UP
    
    POWER_UP --> INIT : 电源稳定(Power_Good)
    
    INIT --> ALIGNMENT : 外设初始化成功
    INIT --> FAULT : 初始化失败/电机不存在
    INIT --> STOPPED : (BDC)无需对齐
    
    ALIGNMENT --> STOPPED : (BLDC)对齐成功
    ALIGNMENT --> FAULT : 对齐失败/超时
    
    STOPPED --> RUN : 启动命令(Start_Cmd)
    STOPPED --> FAULT : 监测到故障(含exFault)
    
    RUN --> STOPPING : 停止命令(Stop_Cmd)
    RUN --> FAULT : 运行故障(含exFault)
    
    STOPPING --> STOPPED : 停机完成
    
    %% 全局外部故障入口（除FAULT自身）
    note left of POWER_UP
        所有状态（除FAULT外）
        检测到exFault立即转FAULT
    end note
    
    %% FAULT状态处理
    FAULT --> STOPPED : 故障清除(可恢复故障)
    FAULT --> [*] : 故障状态(需重新上电)
```

 ┌────────────────────────────────────────────────────────────┐
 │                        STATE  LAYER                        │
 └────────────────────────────────────────────────────────────┘

  +-----------+    +-----------+      +-----------------------+
  | POWER_UP  | -> |   INIT    | -> ( | ALIGNMENT (BLDC only) | ) 
  +-----------+    +-----------+      +-----------------------+
                        ^                        |
                        |                        v
                        |                  +-----------+
                        |                  |  STOPPED  |   <---+
                        |                  +-----------+       |
                  reset |                     |      ^         |
                        |                     |      | quick   |
                        |              start  |      | stop    |
                        |                     v      |         |
                  +-----------+            +-----------+       | 
  [*Any] ----->   |   FAULT   |            | RUNNING   |       |
                  +-----------+            +-----------+       |
                                                |              |
                                                | stopping     |
                                                v              |
                                           +-----------+       |
                                           | STOPPING  | ------+
                                           +-----------+



 ┌────────────────────────────────────────────────────────────┐
 │                        MOTION  LAYER                       │
 │              (Active only when STATE = RUNNING)            │
 └────────────────────────────────────────────────────────────┘

   +---------+    +---------+    +---------+    +---------+    +---------+
   |  PPM    |    |  PVM    |    |   CSV   |    |   CSP   |    |   CST   |
   |Profile  |    | Profile |    | Cyclic  |    | Cyclic  |    | Cyclic  |
   |Position |    |Velocity |    |Velocity |    |Position |    | Torque  |
   +---------+    +---------+    +---------+    +---------+    +---------+

*/


#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include "stateMachine.h"
#include "queue.h"

#define _2str(X_) #X_

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
} Motor_ParamData_t;

/* motor command message types (User -> Motor) */
typedef enum {
    MOTOR_CMD_NONE = 0,                /* No command pending */
    MOTOR_CMD_INIT,                    /* Initialize motor */
    MOTOR_CMD_START,                   /* Start motor */
    MOTOR_CMD_STOP,                    /* Stop motor */
    MOTOR_CMD_QUICK_STOP,              /* Quick stop */
    MOTOR_CMD_RST_FAULT,               /* Reset fault future */

    MOTOR_CMD_SET_MODE,                /* Set control mode */
    MOTOR_CMD_SET_TARGET_TORQUE,       /* Set target torque */
    MOTOR_CMD_SET_TARGET_VELOCITY,     /* Set target velocity */
    MOTOR_CMD_SET_TARGET_POSITION,     /* Set target position */
    MOTOR_CMD_SET_ACCELERATION,        /* Set profile acceleration */
    MOTOR_CMD_SET_MAX_VELOCITY,        /* Set profile max velocity */
    MOTOR_CMD_SET_MAX_ACCELERATION,    /* Set max acceleration */
    MOTOR_CMD_SET_PID_PARAM,           /* Set PI parameters */
} Motor_CmdType_t;

/* motor command message */
typedef struct {
    Motor_CmdType_t type;
    Motor_ParamData_t data;
} Motor_Cmd_t;

/* motor event type */
typedef enum {
    MOTOR_EV_NONE,
    MOTOR_EV_CYCLE,
    /* from user command */
    MOTOR_EV_START_REQUESTED,          /* start */
    MOTOR_EV_STOP_REQUESTED,           /* normal stop */
    MOTOR_EV_FAULT_RESET_REQUESTED,    /* reset fault */
    MOTOR_EV_PARAM_UPDATE_REQUESTED,   /* all parameter update */
    // add more ...
} Motor_Event_t;

/* motor parameter type */
typedef enum {
    MOTOR_PARAM_MODE,
    MOTOR_PARAM_TARGET_TORQUE,
    MOTOR_PARAM_TARGET_VELOCITY,
    MOTOR_PARAM_TARGET_POSITION,
    MOTOR_PARAM_ACCELERATION,
    MOTOR_PARAM_MAX_VELOCITY,
    MOTOR_PARAM_MAX_ACCELERATION,
    MOTOR_PARAM_PIDs,
} Motor_ParamType_t;

/* motor parameter value */
typedef struct {
    Motor_ParamType_t type;
    Motor_ParamData_t val;
} Motor_Param_t;


/** state layer (first layer) */
typedef enum {
    MOTOR_STATE_POWER_UP,
    MOTOR_STATE_INIT,
    MOTOR_STATE_ALIGN,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_STOPPING,
    MOTOR_STATE_STOPPED,
    MOTOR_STATE_FAULTING,
    MOTOR_STATE_FAULTED,
    MOTOR_STATE_RESETTING,
    MAX_MOTOR_STATE_NUM,
} Motor_State_t;

/** motion layer (second layer) */
typedef enum {
    MOTOR_MOTION_TORQUE_CYCLIC,
    MOTOR_MOTION_VELOCITY_CYCLIC,
    MOTOR_MOTION_POSITION_CYCLIC,
    MOTOR_MOTION_VELOCITY_PROFILE,
    MOTOR_MOTION_POSITION_PROFILE,
    MAX_MOTOR_MOTION_NUM,
} Motor_MotionMode_t;

typedef struct {
    int exit_app;
    int fault_active;
    int aligned;

    Motor_MotionMode_t lastMotion;
    struct state *lastMotionState;
    Motor_MotionMode_t pendingMotion;
    struct state *pendingMotionState;
} Context_t;

/* ========================================================================= */
/*                              IMPLEMENTATION                               */
/* ========================================================================= */
// Queue instance
static queue_t cmdQueue;

typedef struct {
    Context_t *ctx;
    const char *selfn;
}Context_Self_t;

// Motor context
static Context_t ctx = {
    .exit_app = 0,
    .fault_active = 0,
    .aligned = 0,
    .lastMotion = MOTOR_MOTION_VELOCITY_PROFILE,
    .lastMotionState = NULL,
    .pendingMotion = MOTOR_MOTION_VELOCITY_PROFILE,
    .pendingMotionState = NULL
};

// Guard Functions
static bool G_PowerGood(void *param, struct event *e);
static bool G_InitSuccess(void *param, struct event *e);
static bool G_AlignSuccess(void *param, struct event *e);
static bool G_IsStopped(void *param, struct event *e);
static bool G_FaultActive(void *param, struct event *e);
static bool G_FaultReseted(void *param, struct event *e);

// Action Functions
static void A_EnterPowerUp(void *stateData, struct event *e);
static void A_EnterInit(void *stateData, struct event *e);
static void A_EnterAlign(void *stateData, struct event *e);
static void A_EnterStopped(void *stateData, struct event *e);
static void A_EnterRunning(void *stateData, struct event *e);
static void A_EnterStopping(void *stateData, struct event *e);
static void A_EnterFault(void *stateData, struct event *e);
static void A_ProcessAlign(void *currentStateData, struct event *event, void *newStateData );
static void A_ProcessStopping(void *currentStateData, struct event *event, void *newStateData );
static void A_UpdateParams(void *currentStateData, struct event *event, void *newStateData );

// State-specific Cycle Actions (Layer 2)
static void A_EnterCyclicTorque( void *stateData, struct event *event );
static void A_EnterCyclicVelocity( void *stateData, struct event *event );
static void A_EnterCyclicPosition( void *stateData, struct event *event );
static void A_EnterProfileVelocity( void *stateData, struct event *event );
static void A_EnterProfilePosition( void *stateData, struct event *event );
static void A_CycleCyclicTorque(void *currentStateData, struct event *event, void *newStateData );
static void A_CycleCyclicVelocity(void *currentStateData, struct event *event, void *newStateData );
static void A_CycleCyclicPosition(void *currentStateData, struct event *event, void *newStateData );
static void A_CycleProfileVelocity(void *currentStateData, struct event *event, void *newStateData );
static void A_CycleProfilePosition(void *currentStateData, struct event *event, void *newStateData );

// forward declaration
static struct state stateLayer[MAX_MOTOR_STATE_NUM];
static struct state motionLayer[MAX_MOTOR_MOTION_NUM];

#define N_TRANSITIONS(layer_, item_) \
    sizeof(layer_[item_].transitions) / sizeof(layer_[item_].transitions[0])

// state layer (first layer) 
static struct state stateLayer[MAX_MOTOR_STATE_NUM] = \
{
    /**
     * MOTOR_STATE_POWER_UP 上电状态
     * 该状态不接收任何命令
     * 检查硬件组件是否连接，检查电源状态(G_PowerGood)
     * - 电源正常&硬件连接 -> 跳转至 MOTOR_STATE_INIT
     * - 未就绪 -> 保持当前状态
     */
    [MOTOR_STATE_POWER_UP] = {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_POWER_UP)
        },
        .entryState = NULL,
        .transitions = (struct transition[]){
            {MOTOR_EV_CYCLE, NULL, &G_PowerGood, NULL, &stateLayer[MOTOR_STATE_INIT]},
            {MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_POWER_UP]}
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_POWER_UP),
        .exitAction = NULL,
    },
    /**
     * MOTOR_STATE_INIT 初始化状态
     * 执行硬件初始化，并监控初始化结果
     * - 初始化成功(G_InitSuccess) -> 跳转至 MOTOR_STATE_ALIGN
     * - 发生故障(G_FaultActive) -> 跳转至 MOTOR_STATE_FAULTING
     * - 正在初始化 -> 保持当前状态
     */
    [MOTOR_STATE_INIT] =  {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_INIT)
        },
        .entryState = NULL,
        .entryAction = A_EnterInit,
        .exitAction = NULL,
        .transitions = (struct transition[]){
             { MOTOR_EV_CYCLE, NULL, &G_FaultActive, NULL, &stateLayer[MOTOR_STATE_FAULTING] },
             { MOTOR_EV_CYCLE, NULL, &G_InitSuccess, NULL, &stateLayer[MOTOR_STATE_ALIGN] },
             { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_INIT] }
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_INIT),
        
    },
    /**
     * MOTOR_STATE_ALIGN 对齐状态
     * 执行电机相位对齐
     * - 对齐成功(G_AlignSuccess) -> 跳转至 MOTOR_STATE_STOPPED
     * - 发生故障(G_FaultActive) -> 跳转至 MOTOR_STATE_FAULTING
     * - 正在对齐 -> 保持当前状态
     */
    [MOTOR_STATE_ALIGN] =  {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_ALIGN)
        },
        .entryState = NULL,
        .entryAction = A_EnterAlign,
        .exitAction = NULL,
        .transitions = (struct transition[]){
             { MOTOR_EV_CYCLE, NULL, G_FaultActive, NULL, &stateLayer[MOTOR_STATE_FAULTING] },
             { MOTOR_EV_CYCLE, NULL, G_AlignSuccess, NULL, &stateLayer[MOTOR_STATE_STOPPED] },
             { MOTOR_EV_CYCLE, NULL, NULL, A_ProcessAlign, &stateLayer[MOTOR_STATE_ALIGN] }
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_ALIGN),
    },
    /**
     * MOTOR_STATE_RUNNING 运行状态（父状态）
     * 作为所有运动模式的父状态，处理通用的停止和故障事件
     * - 发生故障 -> 跳转至 MOTOR_STATE_FAULTING
     * - 收到停止命令 -> 跳转至 MOTOR_STATE_STOPPING
     * - 周期性循环 -> 执行通用的周期任务(A_RunCycle)
     */
    [MOTOR_STATE_RUNNING] =  {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_RUNNING)
        },
        .entryState = &motionLayer[MOTOR_MOTION_VELOCITY_PROFILE],
        .entryAction = A_EnterRunning,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, &G_FaultActive, NULL, &stateLayer[MOTOR_STATE_FAULTING] },
            { MOTOR_EV_STOP_REQUESTED, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_STOPPING] },
            { MOTOR_EV_PARAM_UPDATE_REQUESTED, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_RUNNING] }, 
            { MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_RUNNING] }, 
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_RUNNING),

    },
    /**
     * MOTOR_STATE_STOPPING 状态下不接收任何命令，
     * 将调用 A_EnterStopping 执行减速动作，
     * 使用 G_IsStopped 判断减速是否完成，
     * 速度降到0后，过渡到 MOTOR_STATE_STOPPED 状态
     */
    [MOTOR_STATE_STOPPING] =  {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_STOPPING)
        },
        .entryState = NULL,
        .entryAction = A_EnterStopping,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            {MOTOR_EV_CYCLE, NULL, G_IsStopped, NULL, &stateLayer[MOTOR_STATE_STOPPED]},
            {MOTOR_EV_CYCLE, NULL, NULL, A_ProcessStopping, &stateLayer[MOTOR_STATE_STOPPING]},
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_STOPPING),
        
    },
    /**
     * MOTOR_STATE_STOPPED 状态下
     * 进入时调用 A_EnterStopped 停止运动，
     * 接收所有设置参数的命令，
     * 接收 MOTOR_EV_START 命令，切换到 MOTOR_STATE_RUNNING 状态；
     * 接收 MOTOR_EV_STOP 命令，不会产生实际效果；
     */
    [MOTOR_STATE_STOPPED] =  {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_STOPPED)
        },
        .entryState = NULL,
        .entryAction = A_EnterStopped,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            {MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_STOPPED]},
            {MOTOR_EV_START_REQUESTED, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_RUNNING]},
            {MOTOR_EV_PARAM_UPDATE_REQUESTED, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_STOPPED]},
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_STOPPED),
    },
    /**
     * MOTOR_STATE_FAULTING 状态下不执行任何动作，不接收任何命令，
     * 直接会过渡到 MOTOR_STATE_FAULTED 状态
     */
    [MOTOR_STATE_FAULTING] =  {
        .parentState = NULL,
        .entryState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_FAULTING)
        },
        .exitAction = NULL,
        .entryAction = NULL,
        .transitions = (struct transition[]){
            {MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED]} 
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_FAULTING),
    },
    /**
     * MOTOR_STATE_FAULTED 状态下仅接收 MOTOR_EV_FAULT_RESET_REQUESTED 命令，
     * 检查 G_Faulted_ResetFault 通过后进入 MOTOR_STATE_RESETTING 状态
     */
    [MOTOR_STATE_FAULTED] =  {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_FAULTED)
        },
        .entryState = NULL,
        .entryAction = NULL,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            {MOTOR_EV_FAULT_RESET_REQUESTED, &ctx, G_FaultReseted, NULL, &stateLayer[MOTOR_STATE_RESETTING]},
            {MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_FAULTED]} 
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_FAULTED),
    },
    /**
     * MOTOR_STATE_RESETTING 状态下不执行任何动作，不接收任何命令，
     * 直接会过渡到 MOTOR_STATE_INIT 状态
     */
    [MOTOR_STATE_RESETTING] =  {
        .parentState = NULL,
        .data = &(Context_Self_t){ 
            &ctx, _2str(MOTOR_STATE_RESETTING)
        },
        .entryState = NULL,
        .entryAction = NULL,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            {MOTOR_EV_CYCLE, NULL, NULL, NULL, &stateLayer[MOTOR_STATE_INIT]}
        },
        .numTransitions = N_TRANSITIONS(stateLayer, MOTOR_STATE_RESETTING),
    },
};

/** motion layer (second layer) */
static struct state motionLayer[MAX_MOTOR_MOTION_NUM] = \
{
    /**
     * MOTOR_MOTION_TORQUE_CYCLIC 循环力矩模式
     */
    [MOTOR_MOTION_TORQUE_CYCLIC] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .entryState = NULL,
        .data = _2str(MOTOR_MOTION_TORQUE_CYCLIC),
        .entryAction = NULL,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, &A_CycleCyclicTorque, &motionLayer[MOTOR_MOTION_TORQUE_CYCLIC] }
        },
        .numTransitions = N_TRANSITIONS(motionLayer, MOTOR_MOTION_TORQUE_CYCLIC),
    },
    /**
     * MOTOR_MOTION_VELOCITY_CYCLIC 循环速度模式
     * 在 RUNNING 状态下的子状态，执行速度控制回路
     */
    [MOTOR_MOTION_VELOCITY_CYCLIC] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .entryState = NULL,
        .data = _2str(MOTOR_MOTION_VELOCITY_CYCLIC),
        .entryAction = NULL,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, &A_CycleCyclicVelocity, &motionLayer[MOTOR_MOTION_VELOCITY_CYCLIC] }
        },
        .numTransitions = N_TRANSITIONS(motionLayer, MOTOR_MOTION_VELOCITY_CYCLIC),
    },
    /**
     * MOTOR_MOTION_POSITION_CYCLIC 循环位置模式
     */
    [MOTOR_MOTION_POSITION_CYCLIC] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .entryState = NULL,
        .data = _2str(MOTOR_MOTION_POSITION_CYCLIC),
        .entryAction = NULL,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, &A_CycleCyclicPosition, &motionLayer[MOTOR_MOTION_POSITION_CYCLIC] }
        },
        .numTransitions = N_TRANSITIONS(motionLayer, MOTOR_MOTION_POSITION_CYCLIC),
    },
    /**
     * MOTOR_MOTION_VELOCITY_PROFILE 轮廓速度模式
     */
    [MOTOR_MOTION_VELOCITY_PROFILE] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .entryState = NULL,
        .data = _2str(MOTOR_MOTION_VELOCITY_PROFILE),
        .entryAction = NULL,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, &A_CycleProfileVelocity, &motionLayer[MOTOR_MOTION_VELOCITY_PROFILE] }
        },
        .numTransitions = N_TRANSITIONS(motionLayer, MOTOR_MOTION_VELOCITY_PROFILE),
    },
    /**
     * MOTOR_MOTION_POSITION_PROFILE 轮廓位置模式
     */
    [MOTOR_MOTION_POSITION_PROFILE] = {
        .parentState = &stateLayer[MOTOR_STATE_RUNNING],
        .entryState = NULL,
        .data = _2str(MOTOR_MOTION_POSITION_PROFILE),
        .entryAction = NULL,
        .exitAction = NULL,
        .transitions = (struct transition[]){
            { MOTOR_EV_CYCLE, NULL, NULL, &A_CycleProfilePosition, &motionLayer[MOTOR_MOTION_POSITION_PROFILE] }
        },
        .numTransitions = N_TRANSITIONS(motionLayer, MOTOR_MOTION_POSITION_PROFILE),
    },
};



/* ---------------- Threads ---------------- */

// Motor Control Thread
void *motor_thread(void *arg) 
{
    struct stateMachine fsm;
    stateM_init(&fsm, 
                &stateLayer[MOTOR_STATE_POWER_UP], 
                &stateLayer[MOTOR_STATE_FAULTING]);
    
    printf("Motor Thread Started.\n");

#define EMIT_PARAM(_type, _val)                         \
        do {                                            \
            param.type = _type; param.val = _val;       \
            ev.type = MOTOR_EV_PARAM_UPDATE_REQUESTED;  \
            ev.data = &param;                           \
        } while(0)

    while (!ctx.exit_app) 
    {
        Motor_Cmd_t cmd;
        Motor_Param_t param;
        struct event ev;

        if (dequeue(&cmdQueue, &cmd, 10) == 0) {
            // Got Command
            switch (cmd.type) {
                case MOTOR_CMD_START: 
                    ev.type = MOTOR_EV_START_REQUESTED; 
                    break;
                case MOTOR_CMD_STOP: 
                    ev.type = MOTOR_EV_STOP_REQUESTED; 
                    break;
                case MOTOR_CMD_RST_FAULT: 
                    ev.type = MOTOR_EV_FAULT_RESET_REQUESTED; 
                    break;
                case MOTOR_CMD_SET_MODE: 
                    EMIT_PARAM(MOTOR_PARAM_MODE, cmd.data);
                    break;
                case MOTOR_CMD_SET_TARGET_TORQUE:
                    EMIT_PARAM(MOTOR_PARAM_TARGET_TORQUE, cmd.data);
                    break;
                case MOTOR_CMD_SET_TARGET_VELOCITY:
                    EMIT_PARAM(MOTOR_PARAM_TARGET_VELOCITY, cmd.data);
                    break;
                case MOTOR_CMD_SET_TARGET_POSITION:
                    EMIT_PARAM(MOTOR_PARAM_TARGET_POSITION, cmd.data);
                    break;
                case MOTOR_CMD_SET_ACCELERATION:
                    EMIT_PARAM(MOTOR_PARAM_ACCELERATION, cmd.data);
                    break;
                case MOTOR_CMD_SET_MAX_VELOCITY:
                    EMIT_PARAM(MOTOR_PARAM_MAX_VELOCITY, cmd.data);
                    break;
                case MOTOR_CMD_SET_MAX_ACCELERATION:
                    EMIT_PARAM(MOTOR_PARAM_MAX_ACCELERATION, cmd.data);
                    break;
                case MOTOR_CMD_SET_PID_PARAM:
                    EMIT_PARAM(MOTOR_PARAM_PIDs, cmd.data);
                    break;
                default: 
                    ev.type = MOTOR_EV_NONE; 
                    break;
            }
        } 

        // Timeout -> Cycle
        if (ev.type == MOTOR_EV_NONE) ev.type = MOTOR_EV_CYCLE;
        stateM_handleEvent(&fsm, &ev);
         
         // Visual feedback for running
        //  struct state *st = stateM_currentState(&fsm);
        //  if (st == &sRunTorque) { printf("T"); fflush(stdout); }
        //  if (st == &sRunVelCyclic) { printf("V"); fflush(stdout); }
    }
    return NULL;
}


// User Input Thread
void *input_thread(void *arg) 
{
    char line[64];
    printf("CLI Ready. Commands: start, stop, mode <0-4>, fault, reset, exit\n");
    

#define _strnref(X_) X_, sizeof(X_)

    while (!ctx.exit_app) 
    {
        if (fgets(line, sizeof(line), stdin)) {

            Motor_Cmd_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            
            if (strncmp(line, _strnref("init")) == 0) 
                cmd.type = MOTOR_CMD_INIT;
            else if (strncmp(line, _strnref("start")) == 0) 
                cmd.type = MOTOR_CMD_START;
            else if (strncmp(line, _strnref("stop") ) == 0) 
                cmd.type = MOTOR_CMD_STOP;
            else if (strncmp(line, _strnref("reset")) == 0) 
                cmd.type = MOTOR_CMD_RST_FAULT;
            else if (strncmp(line, _strnref("fault")) == 0) { 
                const void *fault_base = line + sizeof("fault ");
                const int fault_stat = (atoi(fault_base) != 0);
                ctx.fault_active = fault_stat; 
                printf("Fault Injection, %d!\n", fault_stat); 
                continue; 
            }
            else if (strncmp(line, _strnref("exit") ) == 0) { 
                ctx.exit_app = 1; 
                break; 
            }
            else if (strncmp(line, _strnref("mode") ) == 0) {
                const int motion_ofst = sizeof("mode ");
                cmd.type = MOTOR_CMD_SET_MODE;
                if (strncmp(line + motion_ofst , _strnref("cst")) == 0)
                    cmd.data.mode = MOTOR_MOTION_TORQUE_CYCLIC;
                else if (strncmp(line + motion_ofst , _strnref("csv")) == 0) 
                    cmd.data.mode = MOTOR_MOTION_VELOCITY_CYCLIC;
                else if (strncmp(line + motion_ofst , _strnref("csp")) == 0) 
                    cmd.data.mode = MOTOR_MOTION_POSITION_CYCLIC;
                else if (strncmp(line + motion_ofst , _strnref("pvm")) == 0) 
                    cmd.data.mode = MOTOR_MOTION_VELOCITY_PROFILE;
                else if (strncmp(line + motion_ofst , _strnref("ppm")) == 0) 
                    cmd.data.mode = MOTOR_MOTION_POSITION_PROFILE;
                else {
                    printf("Bad mode!\n"); 
                    continue; 
                }
            }
            else if (strncmp(line, _strnref("set") ) == 0) {
                
                const void * set_base = line + sizeof("set ");
                if (strncmp(set_base , _strnref("trq")) == 0) {
                    cmd.type = MOTOR_CMD_SET_TARGET_TORQUE;
                    cmd.data.targetTorque = atoi(set_base + sizeof("trq "));
                }
                else if (strncmp(set_base , _strnref("vel")) == 0) {
                    cmd.type = MOTOR_CMD_SET_TARGET_VELOCITY;
                    cmd.data.targetVelocity = atoi(set_base + sizeof("vel "));
                }
                else if (strncmp(set_base , _strnref("pos")) == 0) {
                    cmd.type = MOTOR_CMD_SET_TARGET_POSITION;
                    cmd.data.targetPosition = atoi(set_base + sizeof("pos "));
                }
                else if (strncmp(set_base , _strnref("acc")) == 0) {
                    cmd.type = MOTOR_CMD_SET_ACCELERATION;
                    cmd.data.acceleration = atoi(set_base + sizeof("acc "));
                }
                else if (strncmp(set_base , _strnref("max_acc")) == 0) {
                    cmd.type = MOTOR_CMD_SET_MAX_ACCELERATION;
                    cmd.data.maxAcceleration = atoi(set_base + sizeof("max_acc "));
                }
                else if (strncmp(set_base , _strnref("max_vel")) == 0) {
                    cmd.type = MOTOR_CMD_SET_MAX_VELOCITY;
                    cmd.data.maxVelocity = atoi(set_base + sizeof("max_vel "));
                }
                else {
                    printf("Bad set!\n"); 
                    continue; 
                }
            }
            
            if (cmd.type != MOTOR_CMD_NONE) {
                enqueue(&cmdQueue, &cmd, 100);
            }
        }
    }

    return NULL;
}

int main() 
{
    newqueue(&cmdQueue, sizeof(Motor_Cmd_t), 10);
    
    pthread_t th_motor, th_input;
    pthread_create(&th_motor, NULL, motor_thread, NULL);
    pthread_create(&th_input, NULL, input_thread, NULL);
    
    pthread_join(th_input, NULL);
    // User typed exit
    ctx.exit_app = 1;
    pthread_join(th_motor, NULL);
    
    delequeue(&cmdQueue);
    return 0;
}



