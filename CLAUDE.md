# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
- `stateMachine`: Holds current/previous state, error state

### Key Features
1. **Nested States**: Parent-child relationships with inheritance of transitions
2. **Guarded Transitions**: Conditional transitions with guard functions
3. **Event Payloads**: Arbitrary data passed with events
4. **Entry/Exit Actions**: Functions called when entering/exiting states
5. **Transition Actions**: Functions executed during state changes
6. **Error State Handling**: Configurable error state for invalid transitions
7. **Final States**: States that stop the state machine

### API Functions
- `stateM_init()`: Initialize state machine with initial and error states
- `stateM_handleEvent()`: Process events and trigger state transitions
- `stateM_currentState()`: Get current state
- `stateM_previousState()`: Get previous state
- `stateM_stopped()`: Check if state machine has stopped (reached final state)

## Development Workflow

### Creating a New State Machine
1. Define state structures with transitions arrays
2. Link states using pointers (parent/child relationships)
3. Implement guard functions, actions, and entry/exit routines
4. Initialize with `stateM_init()`
5. Process events with `stateM_handleEvent()`

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