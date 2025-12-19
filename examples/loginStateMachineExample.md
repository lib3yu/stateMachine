# 两层分层状态机示例 - 内部结构与逻辑分析

## 概述

本文档深入分析一个两层的分层状态机示例的内部结构和逻辑实现。该状态机模拟 UI 登录/注销流程，重点展示 `stateMachine` C 库的核心特性，特别是父子状态关系、事件传递机制和层次状态管理。

## 核心数据结构分析

### 1. 状态结构体 (struct state)

```c
struct state
{
   struct state *parentState;       /* 父状态指针，支持层次结构 */
   struct state *entryState;        /* 入口状态指针，用于父状态 */
   struct transition *transitions;  /* 转换数组 */
   size_t numTransitions;           /* 转换数量 */
   void *data;                      /* 用户自定义状态数据 */
   void (*entryAction)(void *stateData, struct event *event);  /* 进入动作 */
   void (*exitAction)(void *stateData, struct event *event);   /* 退出动作 */
};
```

#### 父子状态关系的实现

父子状态关系通过 `parentState` 指针实现：

- **父状态**: `parentState = NULL`
- **子状态**: `parentState = &父状态地址`

这种设计支持多级嵌套：子状态本身也可以有子状态。

#### EntryState 机制

`entryState` 指针定义了父状态的默认入口子状态：
- 当状态机进入父状态时，自动进入其 `entryState`
- 如果 `entryState` 也有自己的 `entryState`，会继续深入
- 最终进入没有 `entryState` 的状态

### 2. 转换结构体 (struct transition)

```c
struct transition
{
   int eventType;                     /* 触发转换的事件类型 */
   void *condition;                   /* 传递给守卫函数的条件参数 */
   bool (*guard)(void *condition, struct event *event);  /* 守卫函数 */
   void (*action)(void *currentStateData, struct event *event,
                  void *newStateData); /* 转换动作函数 */
   struct state *nextState;           /* 下一个状态（不能为NULL） */
};
```

#### 守卫条件 (Guard) 设计

守卫函数实现条件转换逻辑：
- 接收 `condition` 参数和 `event` 数据
- 返回 `true` 允许转换，`false` 阻止转换
- 支持复杂的业务规则验证

## 状态机层次结构

### 两层结构设计

```
┌─────────────────────────────────────┐
│      状态机层次结构（两层）         │
├─────────────────────────────────────┤
│                                     │
│  ┌─────────────────────────────┐    │
│  │  父状态A: Unauthenticated   │    │
│  │                             │    │
│  │  ┌─────────────────────┐   │    │
│  │  │  子状态1: Idle      │   │    │
│  │  │  parent = &A       │   │    │
│  │  └─────────────────────┘   │    │
│  │                             │    │
│  │  ┌─────────────────────┐   │    │
│  │  │  子状态2: LoggingIn │   │    │
│  │  │  parent = &A       │   │    │
│  │  └─────────────────────┘   │    │
│  │                             │    │
│  │  ┌─────────────────────┐   │    │
│  │  │  子状态3: LoginFail│   │    │
│  │  │  parent = &A       │   │    │
│  │  └─────────────────────┘   │    │
│  └─────────────────────────────┘    │
│                                     │
│  ┌─────────────────────────────┐    │
│  │  父状态B: Authenticated    │    │
│  │                             │    │
│  │  ┌─────────────────────┐   │    │
│  │  │  子状态4: LoggedIn  │   │    │
│  │  │  parent = &B       │   │    │
│  │  └─────────────────────┘   │    │
│  │                             │    │
│  │  ┌─────────────────────┐   │    │
│  │  │  子状态5: LoggingOut│   │    │
│  │  │  parent = &B       │   │    │
│  │  └─────────────────────┘   │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
```

### 内存中的指针链接

```
Memory Address Space:
┌─────────────────────┐      ┌─────────────────────┐
│ Unauthenticated     │─────▶│ IdleState           │
│ parentState: NULL   │◀─────│ parentState: &Unauth│
│ entryState: &Idle   │      │ entryState: NULL    │
│ transitions: ...    │      │ ...                 │
└─────────────────────┘      └─────────────────────┘
       │
       ├─────────────────────▶┌─────────────────────┐
       │                      │ LoggingInState      │
       │                      │ parentState: &Unauth│
       │                      │ entryState: NULL    │
       │                      │ ...                 │
       │                      └─────────────────────┘
       │
       └─────────────────────▶┌─────────────────────┐
                              │ LoginFailedState    │
                              │ parentState: &Unauth│
                              │ entryState: NULL    │
                              │ ...                 │
                              └─────────────────────┘
                                        ...
┌─────────────────────┐      ┌─────────────────────┐
│ Authenticated       │─────▶│ LoggedInState       │
│ parentState: NULL   │◀─────│ parentState: &Auth  │
│ entryState: &LoggedIn      │ entryState: NULL    │
│ transitions: ...    │      │ ...                 │
└─────────────────────┘      └─────────────────────┘
       │
       └─────────────────────▶┌─────────────────────┐
                              │ LoggingOutState     │
                              │ parentState: &Auth  │
                              │ entryState: NULL    │
                              │ ...                 │
                              └─────────────────────┘
```

## 父子状态交互机制

### 1. 事件向上传递 (Event Bubbling)

```
事件处理流程：
┌─────────────────────────────────────┐
│   当前状态: LoggedInState           │
│   parentState: &AuthenticatedState  │
│                                     │
│   事件: EVENT_SESSION_TIMEOUT       │
│                                     │
│   处理过程：                        │
│   1. LoggedInState 检查转换         │
│     → 未找到 EVENT_SESSION_TIMEOUT  │
│                                     │
│   2. 事件向上传递给父状态           │
│     currentState = currentState->parentState
│                                     │
│   3. AuthenticatedState 检查转换    │
│     → 找到 EVENT_SESSION_TIMEOUT    │
│     → 执行转换到 IdleState          │
└─────────────────────────────────────┘
```

### 2. EntryState 链式进入

```
进入父状态的流程：
┌─────────────────────────────────────┐
│   父状态: AuthenticatedState        │
│   entryState: &LoggedInState        │
│                                     │
│   处理过程：                        │
│   1. 进入 AuthenticatedState        │
│                                     │
│   2. 检查 entryState 是否为 NULL    │
│     → entryState = &LoggedInState  │
│                                     │
│   3. 进入 LoggedInState             │
│                                     │
│   4. 检查 LoggedInState 的 entryState│
│     → entryState = NULL            │
│                                     │
│   5. 最终进入 LoggedInState         │
└─────────────────────────────────────┘
```

### 3. 状态转换规则

#### 同级状态转换
```
IdleState ──[EVENT_LOGIN_REQUEST]──▶ LoggingInState
         (需满足守卫条件)
```

#### 跨父状态转换
```
LoggedInState ──[EVENT_SESSION_TIMEOUT]──▶ IdleState
(LoggedInState → AuthenticatedState → IdleState)
```

#### 自循环转换
```
状态 ──[EVENT_UI_INPUT]──▶ 同一状态
(触发动作，但不改变状态)
```

## 内部逻辑流程

### 状态机执行循环

```
Main Loop:
┌─────────────────────────────────────┐
│ while (running) {                   │
│                                     │
│   // 读取用户输入                    │
│   command = readUserInput();        │
│                                     │
│   // 解析命令创建事件                 │
│   event = parseCommand(command);    │
│                                     │
│   // 处理事件                        │
│   result = stateM_handleEvent(&fsm,│
│             &event);                │
│                                     │
│   // 输出结果                        │
│   printResult(result);              │
│ }                                   │
└─────────────────────────────────────┘
```

### 事件处理内部逻辑

```
stateM_handleEvent 内部流程：
┌─────────────────────────────────────┐
│ 1. 参数验证                          │
│    if (!fsm || !event)              │
│      return stateM_errArg;          │
│                                     │
│ 2. 检查当前状态                      │
│    if (!fsm->currentState)          │
│      goToErrorState();              │
│                                     │
│ 3. 查找匹配的转换                    │
│    while (currentState) {           │
│      transition = getTransition();   │
│      if (transition) break;         │
│      currentState = currentState->parentState;
│    }                                │
│                                     │
│ 4. 执行转换                         │
│    if (transition) {                │
│      // 进入目标状态                 │
│      while (nextState->entryState)  │
│        nextState = nextState->entryState;
│                                     │
│      // 执行退出动作                 │
│      if (fsm->currentState->exitAction)
│        fsm->currentState->exitAction();
│                                     │
│      // 执行转换动作                 │
│      if (transition->action)        │
│        transition->action();        │
│                                     │
│      // 执行进入动作                 │
│      if (nextState->entryAction)    │
│        nextState->entryAction();    │
│    }                                │
│                                     │
│ 5. 更新状态机状态                    │
│    fsm->previousState = fsm->currentState;
│    fsm->currentState = nextState;    │
└─────────────────────────────────────┘
```

### 状态转换的完整过程

```
一次完整的状态转换：
┌─────────────────────────────────────┐
│ 转换: IdleState → LoggingInState     │
│ 触发事件: EVENT_LOGIN_REQUEST        │
│                                     │
│ 处理步骤：                          │
│ 1. 验证守卫条件（validateLoginData）│
│   a. 检查事件数据不为 NULL          │
│   b. 验证用户名和密码有效性         │
│                                     │
│ 2. 执行退出动作（exitIdle）         │
│   a. 隐藏登录表单                   │
│                                     │
│ 3. 执行转换动作（initiateLogin）    │
│   a. 发送登录请求到服务器           │
│   b. 启动超时计时器                 │
│                                     │
│ 4. 执行进入动作（enterLoggingIn）   │
│   a. 显示加载动画                   │
│   b. 禁用登录按钮                   │
│   c. 发送登录请求                   │
│                                     │
│ 5. 更新状态机状态                   │
│   a. previousState = IdleState      │
│   b. currentState = LoggingInState  │
└─────────────────────────────────────┘
```

## 设计模式与架构特点

### 1. 组合模式 (Composite Pattern)

**实现方式**:
- 父状态与子状态形成树状结构
- 统一的状态接口（struct state）
- 递归的事件处理机制

**优势**:
- 支持无限层次嵌套
- 统一的处理接口
- 灵活的层次结构

### 2. 责任链模式 (Chain of Responsibility)

**实现方式**:
- 事件沿状态树向上传递
- 每个状态都有机会处理事件
- 直到事件被处理或到达根状态

**优势**:
- 解耦事件发送者与处理者
- 灵活的异常处理机制
- 支持事件委托

### 3. 策略模式 (Strategy Pattern)

**实现方式**:
- 守卫函数作为条件策略
- 动作函数作为行为策略
- 运行时动态绑定

**优势**:
- 算法可独立变化
- 避免多重条件判断
- 支持热插拔策略

### 4. 访问者模式 (Visitor Pattern)

**实现方式**:
- 事件作为访问者访问状态
- 状态接受事件并做出响应
- 双重分派机制

**优势**:
- 状态与事件解耦
- 易于添加新的事件类型
- 支持状态的多态行为

## 关键实现细节

### 1. 状态数据传递

```c
/* 在动作函数中可以访问三个关键数据 */
static void actionFunction(void *currentStateData, struct event *event,
                          void *newStateData) {
   /* currentStateData: 当前状态的数据 */
   /* event->data: 事件携带的数据 */
   /* newStateData: 将要进入的状态的数据 */
}
```

### 2. 守卫条件设计

```c
/* 守卫函数需要满足的条件 */
static bool guardFunction(void *condition, struct event *event) {
   /* condition: 转换时定义的静态条件 */
   /* event->data: 运行时的事件数据 */
   /* 返回 true 允许转换，false 阻止转换 */
}
```

### 3. 状态机生命周期

```
生命周期阶段：
1. 初始化 (stateM_init)
2. 事件处理循环
3. 状态转换执行
4. 动作函数调用
5. 状态更新
6. 终止 (stateM_stopped)
```

### 4. 错误处理机制

```
错误处理流程：
1. 参数验证失败 → 返回错误代码
2. 状态机错误 → 进入错误状态
3. 转换错误 → 调用错误处理函数
4. 动作错误 → 继续执行，记录日志
```

## 总结

这个两层分层状态机示例展示了 `stateMachine` 库的多个核心设计原则：

### 架构特点
1. **层次化状态管理**: 通过指针链接形成状态树，支持复杂的状态关系
2. **事件驱动模型**: 外部事件驱动状态转换，实现响应式设计
3. **策略模式应用**: 守卫函数和动作函数作为可替换策略
4. **组合模式实现**: 状态树支持无限层次嵌套

### 关键技术实现
1. **父子状态关系**: 通过 `parentState` 指针建立层次结构
2. **事件传递机制**: 事件沿状态树向上传递，支持状态继承

3. **EntryState 链**: 自动进入默认子状态，简化状态进入逻辑

4. **状态数据隔离**: 每个状态拥有独立的数据存储

### 设计优势
1. **高内聚低耦合**: 状态逻辑集中管理，状态之间松耦合
2. **可扩展性强**: 易于添加新状态、新事件类型
3. **可维护性好**: 状态转换逻辑清晰，易于理解和调试
4. **重用性高**: 通用状态和转换逻辑可被多个子状态共享

## 状态机层次关系详解

### 父子状态关系图

```
┌──────────────────────────────────────────────┐
│            状态层次关系图                      │
├──────────────────────────────────────────────┤
│                                              │
│  ┌─────────────────────────────────────┐    │
│  │       父状态A: Unauthenticated      │    │
│  │ parentState: NULL                   │    │
│  │ entryState: &IdleState              │    │
│  │                                     │    │
│  │   ┌─────────────────────────┐      │    │
│  │   │  子状态1: IdleState     │◀─────┤    │
│  │   │ parentState: &Unauth    │      │    │
│  │   │ entryState: NULL        │      │    │
│  │   └─────────────────────────┘      │    │
│  │                                     │    │
│  │   ┌─────────────────────────┐      │    │
│  │   │  子状态2: LoggingInState│◀─────┤    │
│  │   │ parentState: &Unauth    │      │    │
│  │   │ entryState: NULL        │      │    │
│  │   └─────────────────────────┘      │    │
│  │                                     │    │
│  │   ┌─────────────────────────┐      │    │
│  │   │  子状态3: LoginFailedState│◀────┤    │
│  │   │ parentState: &Unauth    │      │    │
│  │   │ entryState: NULL        │      │    │
│  │   └─────────────────────────┘      │    │
│  └─────────────────────────────────────┘    │
│                                              │
│  ┌─────────────────────────────────────┐    │
│  │       父状态B: Authenticated       │    │
│  │ parentState: NULL                   │    │
│  │ entryState: &LoggedInState          │    │
│  │                                     │    │
│  │   ┌─────────────────────────┐      │    │
│  │   │  子状态4: LoggedInState │◀─────┤    │
│  │   │ parentState: &Auth      │      │    │
│  │   │ entryState: NULL        │      │    │
│  │   └─────────────────────────┘      │    │
│  │                                     │    │
│  │   ┌─────────────────────────┐      │
│  │   │  子状态5: LoggingOutState│◀─────┤
│  │   │ parentState: &Auth      │      │
│  │   │ entryState: NULL        │      │
│  │   └─────────────────────────┘      │
│  └─────────────────────────────────────┘
└──────────────────────────────────────────────┘
```

### 父子状态交互的核心逻辑

#### 1. 事件传递机制

```c
/* 在 stateM_handleEvent 中的关键逻辑 */
struct state *nextState = fsm->currentState;
do {
   struct transition *transition = getTransition(fsm, nextState, event);

   /* 如果当前状态没有匹配的转换，向上查找父状态 */
   if (!transition) {
      nextState = nextState->parentState;  // 关键：parentState 指针
      continue;
   }

   /* 处理找到的转换 */
   // ...
} while (nextState);
```

#### 2. EntryState 链式进入

```c
/* 处理状态进入的逻辑 */
while (nextState->entryState)
   nextState = nextState->entryState;
```

### 父子状态设计的优势

1. **代码复用**: 父状态定义通用转换，子状态继承使用
2. **逻辑清晰**: 状态层次反映业务逻辑层次
3. **灵活扩展**: 易于添加新状态，不影响现有逻辑
4. **错误隔离**: 状态错误局限于状态层次，不影响其他状态

## 总结

这个两层分层状态机示例的核心价值在于展示了如何通过简单的 `parentState` 指针机制实现复杂的状态层次管理。父子状态关系不仅简化了代码结构，还提供了强大的事件传递和状态继承能力，使得状态机能够优雅地处理复杂的业务逻辑场景。

该设计的关键创新点：
1. **轻量级层次结构**: 通过指针链接，避免复杂的继承体系
2. **事件冒泡机制**: 事件自动向上传递，支持状态委托
3. **链式进入**: 支持状态层次的深度控制
4. **数据隔离**: 状态数据独立管理，提高可维护性

这个示例为构建复杂的状态机系统提供了一个可扩展、可维护的设计模式参考。
enum EventType {
   EVENT_LOGIN_REQUEST,      /* 登录请求 */
   EVENT_LOGIN_SUCCESS,      /* 登录成功 */
   EVENT_LOGIN_FAILURE,      /* 登录失败 */
   EVENT_LOGOUT_REQUEST,     /* 注销请求 */
   EVENT_LOGOUT_COMPLETE,    /* 注销完成 */
   EVENT_SESSION_TIMEOUT,    /* 会话超时 */
   EVENT_UI_INPUT            /* 用户界面输入 */
};
```

### 事件载荷数据结构

```c
/* 登录数据 */
struct LoginData {
   char username[50];
   char password[50];
};

/* UI 状态数据 */
struct UIStateData {
   int currentScreen;
   char message[100];
};
```

## 状态转换逻辑

### 主要转换路径

1. **登录流程**
   ```
   IdleState → LoggingInState → LoggedInState 或 LoginFailedState
   ```

2. **注销流程**
   ```
   LoggedInState → LoggingOutState → IdleState
   ```

3. **会话超时**
   ```
   任何已认证子状态 → IdleState
   ```

4. **登录重试**
   ```
   LoginFailedState → IdleState
   ```

### 状态转换图

```
┌─────────────────┐   login    ┌─────────────────┐   success   ┌─────────────────┐
│    IdleState    │ ──────────►│  LoggingInState │ ──────────►│  LoggedInState  │
│                 │            │                 │            │                 │
└─────┬───────────┘            └────────┬────────┘            └────────┬────────┘
      │                                  │                             │
      │ input                            │ failure                     │ logout
      │                                  │                             │
      ▼                                  ▼                             ▼
┌─────────────────┐            ┌─────────────────┐            ┌─────────────────┐
│ LoginFailedState│            │ LoginFailedState│            │  LoggingOutState│
│                 │            │                 │            │                 │
└─────────────────┘            └────────┬────────┘            └────────┬────────┘
                                        │                             │
                                        └───────── timeout ───────────┘
```

## 守卫函数

### `validateLoginData`

**功能**: 验证登录数据的有效性

**验证逻辑**:
1. 检查登录数据指针是否为 NULL
2. 检查用户名是否为空
3. 检查密码是否为空
4. 检查用户名是否只包含字母和数字

**返回**:
- `true`: 登录数据有效
- `false`: 登录数据无效

### `checkSessionValid`

**功能**: 检查会话有效性

**验证逻辑**:
1. 检查 UI 数据指针是否为 NULL
2. 检查当前屏幕是否为主屏幕（假设 1 表示主屏幕）

**返回**:
- `true`: 会话有效
- `false`: 会话无效

## 动作函数

### 进入动作（Entry Actions）

| 状态 | 函数 | 描述 |
|------|------|------|
| `UnauthenticatedState` | `enterUnauthenticated` | 显示登录界面，清除用户会话数据 |
| `IdleState` | `enterIdle` | 等待用户输入，显示登录表单 |
| `LoggingInState` | `enterLoggingIn` | 显示加载动画，禁用登录按钮，发送登录请求 |
| `LoginFailedState` | `enterLoginFailed` | 显示错误消息，高亮错误字段，提供重试选项 |
| `AuthenticatedState` | `enterAuthenticated` | 设置认证标志，加载用户数据 |
| `LoggedInState` | `enterLoggedIn` | 显示主界面，显示欢迎消息，显示用户头像 |
| `LoggingOutState` | `enterLoggingOut` | 显示注销确认对话框，发送注销请求 |

### 退出动作（Exit Actions）

| 状态 | 函数 | 描述 |
|------|------|------|
| `UnauthenticatedState` | `exitUnauthenticated` | 隐藏登录界面 |
| `IdleState` | `exitIdle` | 隐藏登录表单 |
| `LoggingInState` | `exitLoggingIn` | 隐藏加载动画，启用登录按钮 |
| `LoginFailedState` | `exitLoginFailed` | 清除错误消息，重置输入字段 |
| `AuthenticatedState` | `exitAuthenticated` | 清除认证标志，保存用户数据 |
| `LoggedInState` | `exitLoggedIn` | 隐藏主界面，清除欢迎消息 |
| `LoggingOutState` | `exitLoggingOut` | 隐藏注销确认对话框 |

### 转换动作（Transition Actions）

| 函数 | 触发事件 | 描述 |
|------|----------|------|
| `initiateLogin` | `EVENT_LOGIN_REQUEST` | 发送登录请求到服务器，启动登录超时计时器 |
| `handleLoginSuccess` | `EVENT_LOGIN_SUCCESS` | 保存用户会话，更新 UI 显示已登录状态 |
| `handleLoginFailure` | `EVENT_LOGIN_FAILURE` | 显示错误消息，重置登录表单，记录失败尝试 |
| `transitionToAuthenticated` | `EVENT_LOGIN_SUCCESS` | 设置认证标志，加载用户数据，初始化首选项 |
| `initiateLogout` | `EVENT_LOGOUT_REQUEST` | 显示注销确认对话框，发送注销请求，清理用户数据 |
| `handleLogoutComplete` | `EVENT_LOGOUT_COMPLETE` | 清除用户会话，显示登录界面，重置应用状态 |
| `handleSessionTimeout` | `EVENT_SESSION_TIMEOUT` | 显示会话超时警告，强制用户重新登录，清除过期数据 |

## 命令行交互界面

### 可用命令

| 命令 | 语法 | 描述 |
|------|------|------|
| `login` | `login <username> <password>` | 发送登录请求 |
| `success` | `success` | 模拟登录成功 |
| `failure` | `failure` | 模拟登录失败 |
| `logout` | `logout` | 发送注销请求 |
| `complete` | `complete` | 模拟注销完成 |
| `timeout` | `timeout` | 模拟会话超时 |
| `input` | `input` | 发送 UI 输入事件 |
| `help` | `help` | 显示帮助信息 |
| `exit` | `exit` | 退出程序 |

### 使用示例

```
========================================
 两层分层状态机示例：UI登录/注销流程
========================================

> login user123 password123
守卫条件通过：登录数据有效
<<< 退出空闲状态
    隐藏登录表单
*** 转换动作：初始化登录 ***
    用户名：user123
    发送登录请求到服务器
    启动登录超时计时器
    [模拟] 正在验证用户凭据...
>>> 进入登录中状态
    显示加载动画
    禁用登录按钮
    发送登录请求到服务器

> success
<<< 退出登录中状态
    隐藏加载动画
    启用登录按钮
*** 转换动作：处理登录成功 ***
    保存用户会话
    更新用户界面显示已登录状态
    跳转到主界面
>>> 进入已登录状态
    显示主界面
    显示欢迎消息
    显示用户头像

> logout
<<< 退出已登录状态
    隐藏主界面
    清除欢迎消息
*** 转换动作：初始化注销 ***
    显示注销确认对话框
    发送注销请求到服务器
    清理本地用户数据
>>> 进入注销中状态
    显示注销确认对话框
    发送注销请求到服务器

> complete
<<< 退出注销中状态
    隐藏注销确认对话框
*** 转换动作：处理注销完成 ***
    清除用户会话
    显示登录界面
    重置应用程序状态
>>> 进入空闲状态
    等待用户输入
    显示登录表单
```

## 编译与运行

### 编译命令

```bash
# 编译所有示例（包括新示例）
make dist

# 只编译新示例
make login_example

# 清理编译文件
make clean
```

### 运行命令

```bash
# 运行新示例
./bin/login_example

# 或使用 Makefile 目标
make run_login
```

### 输出文件

```
bin/
├── example           # 原始键盘输入示例
└── login_example     # 新登录状态机示例
```

## 设计模式与特性展示

### 1. 分层状态模式

**实现方式**: 通过 `parentState` 指针形成状态层次结构

**优势**:
- 子状态共享父状态的转换逻辑
- 减少代码重复，提高复用性
- 支持事件向上传递

### 2. 策略模式

**实现方式**: 使用函数指针实现不同的守卫函数和动作函数

**优势**:
- 灵活替换不同的验证逻辑
- 支持多种业务逻辑实现
- 易于扩展新的动作类型

### 3. 状态模式

**实现方式**: 每个状态封装自己的转换逻辑和动作

**优势**:
- 状态转换逻辑集中管理
- 状态行为与状态数据分离
- 支持复杂的状态转换规则

### 4. 事件驱动

**实现方式**: 外部事件驱动状态转换

**优势**:
- 响应式设计，及时处理用户输入
- 状态机与外部环境解耦
- 支持异步事件处理

## 代码结构分析

### 文件结构

```c
/*
 * 文件：loginStateMachineExample.c
 * 结构：
 * 1. 版权声明和头文件包含
 * 2. 文档注释（中文）
 * 3. 事件类型定义
 * 4. 事件载荷结构定义
 * 5. 函数声明（守卫、进入/退出、转换动作）
 * 6. 状态声明
 * 7. 状态定义（初始化）
 * 8. 守卫函数实现
 * 9. 动作函数实现
 * 10. 主程序（命令行交互）
 */
```

### 关键函数

#### 状态定义示例

```c
static struct state IdleState = {
   .parentState = &UnauthenticatedState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      { EVENT_LOGIN_REQUEST, NULL, &validateLoginData, &initiateLogin,
         &LoggingInState },
   },
   .numTransitions = 1,
   .data = "Idle",
   .entryAction = &enterIdle,
   .exitAction = &exitIdle
};
```

#### 守卫函数示例

```c
static bool validateLoginData(void *condition, struct event *event) {
   struct LoginData *loginData = (struct LoginData *)event->data;

   if (loginData == NULL) {
      printf("守卫条件失败：登录数据为空\n");
      return false;
   }

   if (strlen(loginData->username) == 0) {
      printf("守卫条件失败：用户名为空\n");
      return false;
   }

   if (strlen(loginData->password) == 0) {
      printf("守卫条件失败：密码为空\n");
      return false;
   }

   printf("守卫条件通过：登录数据有效\n");
   return true;
}
```

#### 主程序结构

```c
int main(void) {
   struct stateMachine fsm;

   /* 初始化状态机 */
   stateM_init(&fsm, &IdleState, &ErrorState);

   /* 打印欢迎信息和帮助 */
   printHelp();
   printCurrentState(&fsm);

   /* 事件循环 */
   while (running) {
      /* 读取用户输入 */
      /* 解析命令，创建事件 */
      /* 处理事件 */
      /* 更新状态显示 */
   }

   return 0;
}
```

## 扩展与定制

### 添加新事件类型

```c
/* 扩展事件类型枚举 */
enum EventType {
   /* 原有事件... */
   EVENT_NEW_EVENT_TYPE,  /* 新事件类型 */
};
```

### 添加新状态

```c
/* 声明新状态 */
static struct state NewState;

/* 定义新状态 */
static struct state NewState = {
   .parentState = &ParentState,  /* 设置父状态 */
   /* 其他属性... */
};
```

### 自定义守卫函数

```c
/* 实现自定义守卫函数 */
static bool customGuard(void *condition, struct event *event) {
   /* 自定义验证逻辑 */
   return true;  /* 或 false */
}
```

### 添加新的动作函数

```c
/* 实现自定义动作函数 */
static void customAction(void *currentStateData, struct event *event,
                         void *newStateData) {
   /* 自定义业务逻辑 */
}
```

## 测试场景

### 场景 1：成功登录流程

```
1. 初始状态：IdleState
2. 命令：login user123 password123
   - 守卫条件验证通过
   - 转换到 LoggingInState
3. 命令：success
   - 转换到 LoggedInState
4. 验证：当前状态为 LoggedInState，父状态为 AuthenticatedState
```

### 场景 2：登录失败流程

```
1. 初始状态：IdleState
2. 命令：login user123 password123
   - 守卫条件验证通过
   - 转换到 LoggingInState
3. 命令：failure
   - 转换到 LoginFailedState
4. 命令：input
   - 转换到 IdleState
5. 验证：回到空闲状态，可以重新尝试登录
```

### 场景 3：会话超时处理

```
1. 当前状态：LoggedInState
2. 命令：timeout
   - 强制转换到 IdleState
3. 验证：回到空闲状态，需要重新登录
```

### 场景 4：注销流程

```
1. 当前状态：LoggedInState
2. 命令：logout
   - 转换到 LoggingOutState
3. 命令：complete
   - 转换到 IdleState
4. 验证：完成注销，回到空闲状态
```

## 总结

这个两层分层状态机示例展示了 `stateMachine` 库的核心特性：

1. **分层状态结构**: 通过父状态共享通用转换逻辑
2. **事件驱动**: 响应外部事件，驱动状态转换
3. **守卫条件**: 在转换前验证数据有效性
4. **动作函数**: 状态进入/退出和转换时的业务逻辑
5. **命令行交互**: 提供直观的用户界面，便于测试和学习

这个示例不仅可以直接运行，还可以作为模板用于开发更复杂的层次状态机应用，如用户界面状态管理、网络连接状态跟踪、游戏状态机等。通过扩展事件类型、状态定义和动作函数，可以轻松适配不同的业务需求。

## 参考

- [stateMachine 库文档](http://misje.github.io/stateMachine)
- [源代码：examples/loginStateMachineExample.c](../examples/loginStateMachineExample.c)
- [原始示例：examples/stateMachineExample.c](../examples/stateMachineExample.c)
- [测试文件：tests/nestedTest.c](../tests/nestedTest.c)