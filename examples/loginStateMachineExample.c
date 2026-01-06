/*
 * Copyright (c) 2013 Andreas Misje
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "stateMachine.h"

/*
 * 两层分层状态机示例：UI登录/注销流程
 *
 * 状态机层次结构：
 *
 * 顶层状态（父状态）：
 * ├── 未认证状态（UnauthenticatedState）
 * │   ├── 空闲状态（IdleState） - entryState
 * │   ├── 登录中状态（LoggingInState）
 * │   └── 登录失败状态（LoginFailedState）
 * └── 已认证状态（AuthenticatedState）
 *     ├── 已登录状态（LoggedInState） - entryState
 *     └── 注销中状态（LoggingOutState）
 *
 * 这个示例展示了：
 * 1. 两层嵌套状态结构
 * 2. 事件向上传递（子状态未处理的事件传递给父状态）
 * 3. 守卫条件（guard functions）验证数据
 * 4. 进入/退出动作（entry/exit actions）更新UI
 * 5. 转换动作（transition actions）处理业务逻辑
 */

/* 事件类型定义 */
enum EventType {
   EVENT_LOGIN_REQUEST,      /* 登录请求 */
   EVENT_LOGIN_SUCCESS,      /* 登录成功 */
   EVENT_LOGIN_FAILURE,      /* 登录失败 */
   EVENT_LOGOUT_REQUEST,     /* 注销请求 */
   EVENT_LOGOUT_COMPLETE,    /* 注销完成 */
   EVENT_SESSION_TIMEOUT,    /* 会话超时 */
   EVENT_UI_INPUT            /* 用户界面输入 */
};

/* 事件载荷数据结构 */
struct LoginData {
   char username[50];
   char password[50];
};

struct UIStateData {
   int currentScreen;
   char message[100];
};

/* 守卫函数声明 */
static bool validateLoginData(struct stateMachine *fsm, void *condition, struct event *event);
static bool checkSessionValid(struct stateMachine *fsm, void *condition, struct event *event);

/* 进入/退出动作函数声明 */
static void enterUnauthenticated(struct stateMachine *fsm, void *stateData, struct event *event);
static void exitUnauthenticated(struct stateMachine *fsm, void *stateData, struct event *event);
static void enterIdle(struct stateMachine *fsm, void *stateData, struct event *event);
static void exitIdle(struct stateMachine *fsm, void *stateData, struct event *event);
static void enterLoggingIn(struct stateMachine *fsm, void *stateData, struct event *event);
static void exitLoggingIn(struct stateMachine *fsm, void *stateData, struct event *event);
static void enterLoginFailed(struct stateMachine *fsm, void *stateData, struct event *event);
static void exitLoginFailed(struct stateMachine *fsm, void *stateData, struct event *event);
static void enterAuthenticated(struct stateMachine *fsm, void *stateData, struct event *event);
static void exitAuthenticated(struct stateMachine *fsm, void *stateData, struct event *event);
static void enterLoggedIn(struct stateMachine *fsm, void *stateData, struct event *event);
static void exitLoggedIn(struct stateMachine *fsm, void *stateData, struct event *event);
static void enterLoggingOut(struct stateMachine *fsm, void *stateData, struct event *event);
static void exitLoggingOut(struct stateMachine *fsm, void *stateData, struct event *event);
static void handleError(struct stateMachine *fsm, void *stateData, struct event *event);

/* 转换动作函数声明 */
static void initiateLogin(struct stateMachine *fsm, void *currentStateData, struct event *event,
                          void *newStateData);
static void handleLoginSuccess(struct stateMachine *fsm, void *currentStateData, struct event *event,
                               void *newStateData);
static void handleLoginFailure(struct stateMachine *fsm, void *currentStateData, struct event *event,
                               void *newStateData);
static void transitionToAuthenticated(struct stateMachine *fsm, void *currentStateData, struct event *event,
                                      void *newStateData);
static void initiateLogout(struct stateMachine *fsm, void *currentStateData, struct event *event,
                           void *newStateData);
static void handleLogoutComplete(struct stateMachine *fsm, void *currentStateData, struct event *event,
                                 void *newStateData);
static void handleSessionTimeout(struct stateMachine *fsm, void *currentStateData, struct event *event,
                                 void *newStateData);

/* 状态声明 - 按照逻辑顺序定义 */
static struct state UnauthenticatedState, IdleState, LoggingInState,
                     LoginFailedState, AuthenticatedState, LoggedInState,
                     LoggingOutState, ErrorState;

/* ================== 状态定义 ================== */

/* 未认证状态 - 顶层父状态 */
static struct state UnauthenticatedState = {
   .parentState = NULL,
   .entryState = &IdleState,       /* 进入未认证状态时默认进入空闲状态 */
   .transitions = (struct transition[]){
      /* 会话超时事件 - 父状态处理，所有子状态共享 */
      { EVENT_SESSION_TIMEOUT, NULL, NULL, &handleSessionTimeout, &IdleState },
   },
   .numTransitions = 1,
   .data = "Unauthenticated",
   .entryAction = &enterUnauthenticated,
   .exitAction = &exitUnauthenticated
};

/* 空闲状态 - 未认证状态的子状态，也是入口状态 */
static struct state IdleState = {
   .parentState = &UnauthenticatedState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      /* 登录请求 - 需要验证登录数据 */
      { EVENT_LOGIN_REQUEST, NULL, &validateLoginData, &initiateLogin,
         &LoggingInState },
   },
   .numTransitions = 1,
   .data = "Idle",
   .entryAction = &enterIdle,
   .exitAction = &exitIdle
};

/* 登录中状态 - 未认证状态的子状态 */
static struct state LoggingInState = {
   .parentState = &UnauthenticatedState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      /* 登录成功 - 转换到已认证状态 */
      { EVENT_LOGIN_SUCCESS, NULL, NULL, &handleLoginSuccess, &AuthenticatedState },
      /* 登录失败 - 回到登录失败状态 */
      { EVENT_LOGIN_FAILURE, NULL, NULL, &handleLoginFailure, &LoginFailedState },
   },
   .numTransitions = 2,
   .data = "LoggingIn",
   .entryAction = &enterLoggingIn,
   .exitAction = &exitLoggingIn
};

/* 登录失败状态 - 未认证状态的子状态 */
static struct state LoginFailedState = {
   .parentState = &UnauthenticatedState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      /* 用户输入 - 返回空闲状态重新尝试 */
      { EVENT_UI_INPUT, NULL, NULL, NULL, &IdleState },
   },
   .numTransitions = 1,
   .data = "LoginFailed",
   .entryAction = &enterLoginFailed,
   .exitAction = &exitLoginFailed
};

/* 已认证状态 - 另一个顶层父状态 */
static struct state AuthenticatedState = {
   .parentState = NULL,
   .entryState = &LoggedInState,   /* 进入已认证状态时默认进入已登录状态 */
   .transitions = (struct transition[]){
      /* 会话超时 - 父状态处理，强制回到未认证状态 */
      { EVENT_SESSION_TIMEOUT, NULL, NULL, &handleSessionTimeout, &IdleState },
      /* 登录成功 - 转换到已认证状态（用于状态机初始化） */
      { EVENT_LOGIN_SUCCESS, NULL, NULL, &transitionToAuthenticated, &AuthenticatedState },
   },
   .numTransitions = 2,
   .data = "Authenticated",
   .entryAction = &enterAuthenticated,
   .exitAction = &exitAuthenticated
};

/* 已登录状态 - 已认证状态的子状态，也是入口状态 */
static struct state LoggedInState = {
   .parentState = &AuthenticatedState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      /* 注销请求 - 开始注销流程 */
      { EVENT_LOGOUT_REQUEST, NULL, NULL, &initiateLogout, &LoggingOutState },
   },
   .numTransitions = 1,
   .data = "LoggedIn",
   .entryAction = &enterLoggedIn,
   .exitAction = &exitLoggedIn
};

/* 注销中状态 - 已认证状态的子状态 */
static struct state LoggingOutState = {
   .parentState = &AuthenticatedState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      /* 注销完成 - 回到未认证状态 */
      { EVENT_LOGOUT_COMPLETE, NULL, NULL, &handleLogoutComplete, &IdleState },
   },
   .numTransitions = 1,
   .data = "LoggingOut",
   .entryAction = &enterLoggingOut,
   .exitAction = &exitLoggingOut
};

/* 错误状态 */
static struct state ErrorState = {
   .parentState = NULL,
   .entryState = NULL,
   .transitions = NULL,
   .numTransitions = 0,
   .data = "Error",
   .entryAction = &handleError,
   .exitAction = NULL
};

/* ================== 守卫函数实现 ================== */

/* 验证登录数据 */
static bool validateLoginData(struct stateMachine *fsm, void *condition, struct event *event) {
   struct LoginData *loginData = (struct LoginData *)event->data;

   if (loginData == NULL) {
      printf("守卫条件失败：登录数据为空\n");
      return false;
   }

   /* 简单的验证逻辑：用户名和密码不能为空 */
   if (strlen(loginData->username) == 0) {
      printf("守卫条件失败：用户名为空\n");
      return false;
   }

   if (strlen(loginData->password) == 0) {
      printf("守卫条件失败：密码为空\n");
      return false;
   }

   /* 用户名只能包含字母和数字 */
   for (size_t i = 0; i < strlen(loginData->username); i++) {
      if (!isalnum(loginData->username[i])) {
         printf("守卫条件失败：用户名包含非法字符\n");
         return false;
      }
   }

   printf("守卫条件通过：登录数据有效\n");
   return true;
}

/* 检查会话有效性 */
static bool checkSessionValid(struct stateMachine *fsm, void *condition, struct event *event) {
   struct UIStateData *uiData = (struct UIStateData *)condition;

   if (uiData == NULL) {
      printf("守卫条件失败：UI数据为空\n");
      return false;
   }

   /* 简单的验证逻辑：当前屏幕必须是主屏幕 */
   if (uiData->currentScreen == 1) {  /* 假设1表示主屏幕 */
      printf("守卫条件通过：会话有效\n");
      return true;
   }

   printf("守卫条件失败：当前屏幕不是主屏幕\n");
   return false;
}

/* ================== 进入/退出动作函数实现 ================== */

static void enterUnauthenticated(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf(">>> 进入未认证状态\n");
   printf("    显示登录界面\n");
   printf("    清除用户会话数据\n");
}

static void exitUnauthenticated(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("<<< 退出未认证状态\n");
   printf("    隐藏登录界面\n");
}

static void enterIdle(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf(">>> 进入空闲状态\n");
   printf("    等待用户输入\n");
   printf("    显示登录表单\n");
}

static void exitIdle(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("<<< 退出空闲状态\n");
   printf("    隐藏登录表单\n");
}

static void enterLoggingIn(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf(">>> 进入登录中状态\n");
   printf("    显示加载动画\n");
   printf("    禁用登录按钮\n");
   printf("    发送登录请求到服务器\n");
}

static void exitLoggingIn(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("<<< 退出登录中状态\n");
   printf("    隐藏加载动画\n");
   printf("    启用登录按钮\n");
}

static void enterLoginFailed(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf(">>> 进入登录失败状态\n");
   printf("    显示错误消息\n");
   printf("    高亮错误的输入字段\n");
   printf("    提供重试选项\n");
}

static void exitLoginFailed(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("<<< 退出登录失败状态\n");
   printf("    清除错误消息\n");
   printf("    重置输入字段\n");
}

static void enterAuthenticated(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf(">>> 进入已认证状态\n");
   printf("    设置认证标志\n");
   printf("    加载用户数据\n");
}

static void exitAuthenticated(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("<<< 退出已认证状态\n");
   printf("    清除认证标志\n");
   printf("    保存用户数据\n");
}

static void enterLoggedIn(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf(">>> 进入已登录状态\n");
   printf("    显示主界面\n");
   printf("    显示欢迎消息\n");
   printf("    显示用户头像\n");
}

static void exitLoggedIn(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("<<< 退出已登录状态\n");
   printf("    隐藏主界面\n");
   printf("    清除欢迎消息\n");
}

static void enterLoggingOut(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf(">>> 进入注销中状态\n");
   printf("    显示注销确认对话框\n");
   printf("    发送注销请求到服务器\n");
}

static void exitLoggingOut(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("<<< 退出注销中状态\n");
   printf("    隐藏注销确认对话框\n");
}

static void handleError(struct stateMachine *fsm, void *stateData, struct event *event) {
   printf("### 错误状态 ###\n");
   printf("    发生未预期的错误\n");
   printf("    显示错误界面\n");
   printf("    提供重新加载选项\n");
}

/* ================== 转换动作函数实现 ================== */

static void initiateLogin(struct stateMachine *fsm, void *currentStateData, struct event *event,
                          void *newStateData) {
   struct LoginData *loginData = (struct LoginData *)event->data;

   printf("*** 转换动作：初始化登录 ***\n");
   printf("    用户名：%s\n", loginData->username);
   printf("    发送登录请求到服务器\n");
   printf("    启动登录超时计时器\n");

   /* 模拟登录验证 */
   printf("    [模拟] 正在验证用户凭据...\n");
}

static void handleLoginSuccess(struct stateMachine *fsm, void *currentStateData, struct event *event,
                               void *newStateData) {
   printf("*** 转换动作：处理登录成功 ***\n");
   printf("    保存用户会话\n");
   printf("    更新用户界面显示已登录状态\n");
   printf("    跳转到主界面\n");
}

static void handleLoginFailure(struct stateMachine *fsm, void *currentStateData, struct event *event,
                               void *newStateData) {
   printf("*** 转换动作：处理登录失败 ***\n");
   printf("    显示错误消息\n");
   printf("    重置登录表单\n");
   printf("    记录登录失败尝试\n");
}

static void transitionToAuthenticated(struct stateMachine *fsm, void *currentStateData, struct event *event,
                                      void *newStateData) {
   printf("*** 转换动作：转换到已认证状态 ***\n");
   printf("    设置认证标志\n");
   printf("    加载用户数据\n");
   printf("    初始化用户首选项\n");
}

static void initiateLogout(struct stateMachine *fsm, void *currentStateData, struct event *event,
                           void *newStateData) {
   printf("*** 转换动作：初始化注销 ***\n");
   printf("    显示注销确认对话框\n");
   printf("    发送注销请求到服务器\n");
   printf("    清理本地用户数据\n");
}

static void handleLogoutComplete(struct stateMachine *fsm, void *currentStateData, struct event *event,
                                 void *newStateData) {
   printf("*** 转换动作：处理注销完成 ***\n");
   printf("    清除用户会话\n");
   printf("    显示登录界面\n");
   printf("    重置应用程序状态\n");
}

static void handleSessionTimeout(struct stateMachine *fsm, void *currentStateData, struct event *event,
                                 void *newStateData) {
   printf("*** 转换动作：处理会话超时 ***\n");
   printf("    显示会话超时警告\n");
   printf("    强制用户重新登录\n");
   printf("    清除过期会话数据\n");
}

/* ================== 主程序 ================== */

static void printHelp(void) {
   printf("\n可用命令：\n");
   printf("  login <username> <password>   - 发送登录请求\n");
   printf("  success                       - 模拟登录成功\n");
   printf("  failure                       - 模拟登录失败\n");
   printf("  logout                        - 发送注销请求\n");
   printf("  complete                      - 模拟注销完成\n");
   printf("  timeout                       - 模拟会话超时\n");
   printf("  input                         - 发送UI输入事件\n");
   printf("  help                          - 显示帮助\n");
   printf("  exit                          - 退出程序\n");
   printf("\n当前状态层次结构：\n");
   printf("  未认证状态 -> [空闲, 登录中, 登录失败]\n");
   printf("  已认证状态 -> [已登录, 注销中]\n");
}

static void printCurrentState(struct stateMachine *fsm) {
   struct state *current = stateM_currentState(fsm);
   struct state *previous = stateM_previousState(fsm);

   printf("\n=== 当前状态 ===\n");
   printf("当前状态：%s\n", (char *)current->data);
   printf("前一状态：%s\n", previous ? (char *)previous->data : "无");

   if (current->parentState) {
      printf("父状态：%s\n", (char *)current->parentState->data);
   }

   printf("状态机已停止：%s\n", stateM_stopped(fsm) ? "是" : "否");
}

int main(void) {
   struct stateMachine fsm;

   /* 初始化状态机，从空闲状态开始 */
   stateM_init(&fsm, &IdleState, &ErrorState, NULL);

   printf("========================================\n");
   printf(" 两层分层状态机示例：UI登录/注销流程\n");
   printf("========================================\n");

   printHelp();
   printCurrentState(&fsm);

   char command[100];
   int running = 1;

   while (running) {
      printf("\n> ");

      if (fgets(command, sizeof(command), stdin) == NULL) {
         break;
      }

      /* 去除换行符 */
      command[strcspn(command, "\n")] = 0;

      if (strcmp(command, "exit") == 0) {
         printf("退出程序...\n");
         running = 0;
         continue;
      }

      if (strcmp(command, "help") == 0) {
         printHelp();
         continue;
      }

      struct event event;

      /* 解析命令并创建相应的事件 */
      if (strncmp(command, "login ", 6) == 0) {
         char username[50];
         char password[50];

         if (sscanf(command + 6, "%49s %49s", username, password) == 2) {
            struct LoginData loginData;
            strcpy(loginData.username, username);
            strcpy(loginData.password, password);

            event.type = EVENT_LOGIN_REQUEST;
            event.data = &loginData;

            int result = stateM_handleEvent(&fsm, &event);
            printf("事件处理结果：%d\n", result);
         } else {
            printf("用法：login <用户名> <密码>\n");
         }
      }
      else if (strcmp(command, "success") == 0) {
         event.type = EVENT_LOGIN_SUCCESS;
         event.data = NULL;
         stateM_handleEvent(&fsm, &event);
      }
      else if (strcmp(command, "failure") == 0) {
         event.type = EVENT_LOGIN_FAILURE;
         event.data = NULL;
         stateM_handleEvent(&fsm, &event);
      }
      else if (strcmp(command, "logout") == 0) {
         event.type = EVENT_LOGOUT_REQUEST;
         event.data = NULL;
         stateM_handleEvent(&fsm, &event);
      }
      else if (strcmp(command, "complete") == 0) {
         event.type = EVENT_LOGOUT_COMPLETE;
         event.data = NULL;
         stateM_handleEvent(&fsm, &event);
      }
      else if (strcmp(command, "timeout") == 0) {
         event.type = EVENT_SESSION_TIMEOUT;
         event.data = NULL;
         stateM_handleEvent(&fsm, &event);
      }
      else if (strcmp(command, "input") == 0) {
         event.type = EVENT_UI_INPUT;
         event.data = NULL;
         stateM_handleEvent(&fsm, &event);
      }
      else {
         printf("未知命令：%s\n", command);
         printf("输入 'help' 查看可用命令\n");
      }

      printCurrentState(&fsm);
   }

   return 0;
}