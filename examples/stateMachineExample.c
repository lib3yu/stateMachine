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
#include "stateMachine.h"

/* This simple example checks keyboad input against the two allowed strings
 * "ha\n" and "hi\n". If an unrecognised character is read, a group state will
 * handle this by printing a message and returning to the idle state. If the
 * character '!' is encountered, a "reset" message is printed, and the group
 * state's entry state will be entered (the idle state). 
 *
 *                   print 'reset'
 *       o      +---------------------+
 *       |      |                     | '!'
 *       |      v     group state     |
 * +-----v-----------------------------------+----+
 * |  +------+  'h'  +---+  'a'  +---+  '\n'      |
 * +->| idle | ----> | h | ----> | a | ---------+ |
 * |  +------+       +---+\      +---+          | |
 * |   ^ ^ ^               \'i'  +---+  '\n'    | |
 * |   | | |                \--> | i | ------+  | |
 * |   | | |                     +---+       |  | |
 * +---|-|-|----------------+----------------|--|-+
 *     | | |                |                |  |
 *     | | |                | '[^hai!\n]'    |  |
 *     | | | print unrecog. |                |  |
 *     | | +----------------+   print 'hi'   |  |
 *     | +-----------------------------------+  |
 *     |               print 'ha'               |
 *     +----------------------------------------+
 */

/* Types of events */
enum eventType {
   Event_keyboard,
};

/* Compare keyboard character from transition's condition variable against
 * data in event. */
static bool compareKeyboardChar( struct stateMachine *fsm, void *ch, struct event *event );

static void printRecognisedChar( struct stateMachine *fsm, void *stateData, struct event *event );
static void printUnrecognisedChar( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData );
static void printReset( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData );
static void printHiMsg( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData );
static void printHaMsg( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData );
static void printErrMsg( struct stateMachine *fsm, void *stateData, struct event *event );
static void printEnterMsg( struct stateMachine *fsm, void *stateData, struct event *event );
static void printExitMsg( struct stateMachine *fsm, void *stateData, struct event *event );

/* Forward declaration of states so that they can be defined in an logical
 * order: */
static struct state checkCharsGroupState, idleState, hState, iState, aState;

/* All the following states (apart from the error state) are children of this
 * group state. This way, any unrecognised character will be handled by this
 * state's transition, eliminating the need for adding the same transition to
 * all the children states. */
static struct state checkCharsGroupState = {
   .parentState = NULL,
   /* The entry state is defined in order to demontrate that the 'reset'
    * transtition, going to this group state, will be 'redirected' to the
    * 'idle' state (the transition could of course go directly to the 'idle'
    * state): */
   .entryState = &idleState,
   .transitions = (struct transition[]){
      { Event_keyboard, (void *)(intptr_t)'!', &compareKeyboardChar,
         &printReset, &idleState, },
      { Event_keyboard, NULL, NULL, &printUnrecognisedChar, &idleState, },
   },
   .numTransitions = 2,
   .data = "group",
   .entryAction = &printEnterMsg,
   .exitAction = &printExitMsg,
};

static struct state idleState = {
   .parentState = &checkCharsGroupState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      { Event_keyboard, (void *)(intptr_t)'h', &compareKeyboardChar, NULL,
         &hState },
   },
   .numTransitions = 1,
   .data = "idle",
   .entryAction = &printEnterMsg,
   .exitAction = &printExitMsg,
};

static struct state hState = {
   .parentState = &checkCharsGroupState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      { Event_keyboard, (void *)(intptr_t)'a', &compareKeyboardChar, NULL,
         &aState },
      { Event_keyboard, (void *)(intptr_t)'i', &compareKeyboardChar, NULL,
         &iState },
   },
   .numTransitions = 2,
   .data = "H",
   .entryAction = &printRecognisedChar,
   .exitAction = &printExitMsg,
};

static struct state iState = {
   .parentState = &checkCharsGroupState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      { Event_keyboard, (void *)(intptr_t)'\n', &compareKeyboardChar,
         &printHiMsg, &idleState }
   },
   .numTransitions = 1,
   .data = "I",
   .entryAction = &printRecognisedChar,
   .exitAction = &printExitMsg,
};

static struct state aState = {
   .parentState = &checkCharsGroupState,
   .entryState = NULL,
   .transitions = (struct transition[]){
      { Event_keyboard, (void *)(intptr_t)'\n', &compareKeyboardChar,
         &printHaMsg, &idleState }
   },
   .numTransitions = 1,
   .data = "A",
   .entryAction = &printRecognisedChar,
   .exitAction = &printExitMsg
};

static struct state errorState = {
   .entryAction = &printErrMsg
};


int main()
{
   struct stateMachine m;
   stateM_init( &m, &idleState, &errorState, NULL );

   int ch;
   while ( ( ch = getc( stdin ) ) != EOF )
      stateM_handleEvent( &m, &(struct event){ Event_keyboard,
            (void *)(intptr_t)ch } );

   return 0;
}

static bool compareKeyboardChar( struct stateMachine *fsm, void *ch, struct event *event )
{
   (void)fsm;
   if ( event->type != Event_keyboard )
      return false;

   return (intptr_t)ch == (intptr_t)event->data;
}

static void printRecognisedChar( struct stateMachine *fsm, void *stateData, struct event *event )
{
   (void)fsm;
   printEnterMsg( fsm, stateData, event );
   printf( "parsed: %c\n", (char)(intptr_t)event->data );
}

static void printUnrecognisedChar( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData )
{
   (void)fsm;
   (void)oldStateData;
   (void)newStateData;
   printf( "unrecognised character: %c\n",
         (char)(intptr_t)event->data );
}

static void printReset( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData )
{
   (void)fsm;
   (void)oldStateData;
   (void)event;
   (void)newStateData;
   puts( "Resetting" );
}

static void printHiMsg( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData )
{
   (void)fsm;
   (void)oldStateData;
   (void)event;
   (void)newStateData;
   puts( "Hi!" );
}

static void printHaMsg( struct stateMachine *fsm, void *oldStateData, struct event *event,
      void *newStateData )
{
   (void)fsm;
   (void)oldStateData;
   (void)event;
   (void)newStateData;
   puts( "Ha-ha" );
}

static void printErrMsg( struct stateMachine *fsm, void *stateData, struct event *event )
{
   (void)fsm;
   (void)stateData;
   (void)event;
   puts( "ENTERED ERROR STATE!" );
}

static void printEnterMsg( struct stateMachine *fsm, void *stateData, struct event *event )
{
   (void)fsm;
   (void)event;
   printf( "Entering %s state\n", (char *)stateData );
}

static void printExitMsg( struct stateMachine *fsm, void *stateData, struct event *event )
{
   (void)fsm;
   (void)event;
   printf( "Exiting %s state\n", (char *)stateData );
}
