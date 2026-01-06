# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.


---

## ⚠️ CRITICAL DESIGN PHILOSOPHY ⚠️

**When reviewing code, prioritize checking adherence to CRITICAL DESIGN PHILOSOPHY above all else.**
**READ THIS BEFORE WRITING ANY CODE**

### 1. Don't Overthink Interfaces

Interfaces must be **simple, direct, intuitive**. Do not add unnecessary abstraction layers for "flexibility" or "extensibility." Callers need APIs that work directly, not design patterns to decode.

### 2. Don't Over-Defend

Over-defensive programming (excessive parameter validation, error handling branches) makes code harder to read and use. Trust callers to provide valid input, validate only when necessary.

**Principle**: Matching caller expectations is more important than "protecting" them.

### 3. Intuitive Expectations First

Function behavior must match the literal meaning of its name. No hesitation, no confirmations, no surprises.

### 4. Fact-Based Judgments

Judge based on actual use cases, not "might need later." Follow language idioms, don't import patterns from other languages. Performance and readability claims need actual evidence.

### 5. No People-Pleasing

Don't add unnecessary comments to "look professional." Don't over-document "for completeness." State facts, point out issues. No "sandwich method" (compliment-criticize-compliment).

### 6. Code as Doc

Code should be self-documenting. Names and structure should convey intent without requiring external explanation. If you need comments to explain what code does, the code is poorly written.

### 7. Explain Why, Not What

Necessary comments should explain **why**, not repeat **what** the code does.

---

## User Development Plan Implementation Standards

**WORKFLOW FOR HANDLING USER DEVELOPMENT REQUESTS**

### 1. Plan Assessment

When the user proposes a development plan:
- Fully assess the workload and complexity
- Provide feedback to the user about the plan's scope
- The plan remains in a "pending review" state awaiting user approval

### 2. Complexity Evaluation

If the development plan is too complex to complete in one session or would benefit from a multi-step approach:
- Inform the user of the recommendation to split into stages
- Explain the rationale for staged implementation
- Wait for user review and decision before proceeding

### 3. TODO Creation (After Approval)

Once the development plan is approved by the user:
- Create a complete development plan TODO list
- Each TODO item must be a **minimal completable unit**:
  - Can be independently explained
  - Can be independently tested
  - Not bloated (single responsibility)
- This structure facilitates user review and progress tracking

---

## Project Overview

This is a C99 finite state machine (FSM) library with support for nested states, guarded transitions, event payloads, and entry/exit actions. The library is a standalone C implementation with no external dependencies.

## Key Files

**Core Library:**
- [src/stateMachine.h](src/stateMachine.h): Main header file with complete API documentation and Doxygen comments
- [src/stateMachine.c](src/stateMachine.c): Core implementation (179 lines)

**Example Applications and Utilities:**
- [examples/stateMachineExample.c](examples/stateMachineExample.c): Example usage with keyboard input parsing
- [examples/queue.h](examples/queue.h): Thread-safe message queue header (utility for examples, not part of core library)
- [examples/queue.c](examples/queue.c): Thread-safe message queue implementation (utility for examples, not part of core library)

**Tests:**
- [tests/nestedTest.c](tests/nestedTest.c): Test for multiply nested states behavior

**Build System and Documentation:**
- [Makefile](Makefile): Simple GNU Make build system
- [doc/doxyconfig](doc/doxyconfig): Doxygen configuration for generating API documentation

## Build Commands

```bash
make          # Cleans, builds, and runs example (default target)
make dist     # Compiles to bin/example
make run      # Runs compiled example
make clean    # Cleans bin directory
```

The build uses `gcc -std=c99 -I src` and compiles `src/stateMachine.c` with `examples/stateMachineExample.c` to produce `bin/example`.

## Architecture

### Core Data Structures
- `event`: Contains type and optional payload (void pointer data)
- `transition`: Links states with guard conditions and actions
- `state`: Contains transitions array, parent/child relationships, entry/exit actions
- `stateMachine`: Holds current/previous state, error state, and user context

### Key Features
1. **Nested States**: Parent-child relationships with inheritance of transitions
2. **Multi-Instance Support**: Each FSM instance has a `userData` pointer for instance-specific context
3. **Guarded Transitions**: Conditional transitions with guard functions
4. **Event Payloads**: Arbitrary data passed with events
5. **Entry/Exit Actions**: Functions called when entering/exiting states
6. **Transition Actions**: Functions executed during state changes
7. **Error State Handling**: Configurable error state for invalid transitions
8. **Final States**: States that stop the state machine

### Callback Signatures
All callbacks receive `struct stateMachine *fsm` as the first parameter, providing access to `fsm->userData`:
- `guard(fsm, condition, event)`: Returns bool to allow/deny transition
- `action(fsm, currentStateData, event, newStateData)`: Executes during transition
- `entryAction(fsm, stateData, event)`: Called when entering a state
- `exitAction(fsm, stateData, event)`: Called when exiting a state

### API Functions
- `stateM_init(fsm, initialState, errorState, userData)`: Initialize state machine with initial state, error state, and user context
- `stateM_handleEvent(fsm, event)`: Process events and trigger state transitions
- `stateM_currentState(fsm)`: Get current state
- `stateM_previousState(fsm)`: Get previous state
- `stateM_stopped(fsm)`: Check if state machine has stopped (reached final state)

## Development Workflow

### Creating a New State Machine
1. Define your context structure (if using instance-specific data)
2. Define state structures with transitions arrays
3. Link states using pointers (parent/child relationships)
4. Implement guard functions, actions, and entry/exit routines (all receive `fsm` as first parameter)
5. Initialize with `stateM_init(fsm, initialState, errorState, userData)`
6. Process events with `stateM_handleEvent(fsm, event)`

### Multi-Instance Example
```c
// Define context for each instance
typedef struct {
    int id;
    int value;
} MyContext_t;

// Create two independent FSM instances
struct stateMachine fsm1, fsm2;
MyContext_t ctx1 = { .id = 1, .value = 10 };
MyContext_t ctx2 = { .id = 2, .value = 20 };

stateM_init(&fsm1, &initialState, &errorState, &ctx1);
stateM_init(&fsm2, &initialState, &errorState, &ctx2);

// Callbacks can access instance-specific data via fsm->userData
static void myAction(struct stateMachine *fsm, void *stateData, struct event *e) {
    MyContext_t *ctx = (MyContext_t *)fsm->userData;
    printf("Instance %d, value %d\n", ctx->id, ctx->value);
}
```

### Running Tests
The test file `nestedTest.c` is a standalone C program. To run it:
```bash
gcc -std=c99 -I src src/stateMachine.c tests/nestedTest.c -o bin/test && ./bin/test
```

### Generating Documentation
```bash
doxygen doc/doxyconfig
```

## Code Style
- C99 standard with `stdbool.h`, `stdint.h`
- Clear Doxygen documentation in header file
- Pointer-based state linking (no dynamic allocation required)
- Error handling through return codes and error state transitions

## Integration
To use this library in another project:
1. Copy `src/stateMachine.h` and `src/stateMachine.c` into your project
2. Include the header: `#include "stateMachine.h"`
3. Compile with C99 support: `gcc -std=c99 -c stateMachine.c`
4. Link with your application

## Example Utilities

### Thread-Safe Message Queue ([examples/queue.h](examples/queue.h), [examples/queue.c](examples/queue.c))
This queue utility is provided for example applications that need thread-safe message passing. It's not part of the core state machine library but is available for use in examples.

**Basic Usage:**
```c
#include "queue.h"

queue_t msg_queue;

// Initialize queue for 100 int messages
newqueue(&msg_queue, sizeof(int), 100);

// Producer thread
int data = 42;
enqueue(&msg_queue, &data, -1); // Block indefinitely

// Consumer thread
int received;
dequeue(&msg_queue, &received, 1000); // Wait up to 1 second

// Cleanup
delequeue(&msg_queue);
```

**Timeout Options:**
- `-1`: Block indefinitely
- `0`: Non-blocking (fail immediately if queue is full/empty)
- `>0`: Block for specified milliseconds

**Return Codes:**
- `0`: Success
- `-1`: Queue full/empty (non-blocking mode)
- `-2`: Timeout exceeded

**Dependencies:** Requires pthread library

## VS Code Configuration
The `.vscode/settings.json` disables Claude Code terminal integration: `"claudeCode.useTerminal": false`