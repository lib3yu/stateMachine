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

#include "stateMachine.h"

static void goToErrorState( struct stateMachine *stateMachine,
      struct event *const event );
static struct transition *getTransition( struct stateMachine *stateMachine,
      struct state *state, struct event *const event );

void stateM_init( struct stateMachine *fsm,
      struct state *initialState, struct state *errorState )
{
   if ( !fsm )
      return;

   fsm->currentState = initialState;
   fsm->previousState = NULL;
   fsm->errorState = errorState;
}

int stateM_handleEvent( struct stateMachine *fsm,
      struct event *event )
{
   if ( !fsm || !event )
      return stateM_errArg;

   if ( !fsm->currentState )
   {
      goToErrorState( fsm, event );
      return stateM_errorStateReached;
   }

    // 1. original code
    // if ( !fsm->currentState->numTransitions )
    // 2. edition 2
    // if ( !fsm->currentState->numTransitions && 
    //    (!fsm->currentState->parentState || !fsm->currentState->parentState->numTransitions) )
    // 3. edition 3
    // /* Optimization
    //  * If no state in the entire hierarchy has any transitions,
    //  * we can return immediately without traversing the parent chain.
    //  *
    //  * We need to check all ancestors because in a hierarchical state machine,
    //  * events can be handled by any ancestor state.
    //  */
    // bool anyTransitions = false;
    // struct state *checkState = fsm->currentState;
    // while (checkState && !anyTransitions) {
    //    anyTransitions = (checkState->numTransitions > 0);
    //    checkState = checkState->parentState;
    // }
    // if (!anyTransitions)
    //    return stateM_noStateChange;

   struct state *nextState = fsm->currentState;
   do {
      struct transition *transition = getTransition( fsm, nextState, event );

      /* If there were no transitions for the given event for the current
       * state, check if there are any transitions for any of the parent
       * states (if any): */
      if ( !transition )
      {
         nextState = nextState->parentState;
         continue;
      }

      /* A transition must have a next state defined. If the user has not
       * defined the next state, go to error state: */
      if ( !transition->nextState )
      {
         goToErrorState( fsm, event );
         return stateM_errorStateReached;
      }

      nextState = transition->nextState;

      /* If the new state is a parent state, enter its entry state (if it has
       * one). Step down through the whole family tree until a state without
       * an entry state is found: */
      struct state *targetLeafState = nextState;
      while ( targetLeafState->entryState )
         targetLeafState = targetLeafState->entryState;

      /* Find common ancestor between current state and target leaf state */
      struct state *currentPath[STATE_MAX_DEPTH], *targetPath[STATE_MAX_DEPTH];
      int currentDepth = 0, targetDepth = 0;

      /* Build current state path (leaf to root) */
      struct state *s = fsm->currentState;
      while (s && currentDepth < STATE_MAX_DEPTH) {
         currentPath[currentDepth++] = s;
         s = s->parentState;
      }

      /* Build target leaf state path (leaf to root) */
      s = targetLeafState;
      while (s && targetDepth < STATE_MAX_DEPTH) {
         targetPath[targetDepth++] = s;
         s = s->parentState;
      }

      /* Find common ancestor by comparing paths from root down */
      struct state *commonAncestor = NULL;
      int i = currentDepth - 1, j = targetDepth - 1;
      while (i >= 0 && j >= 0 && currentPath[i] == targetPath[j]) {
         commonAncestor = currentPath[i];
         i--; j--;
      }

      /* Call exit actions from current state up to (but not including) common ancestor */
      for (int k = 0; k <= i; k++) {
         struct state *exitState = currentPath[k];
         if (exitState->exitAction)
            exitState->exitAction(exitState->data, event);
      }

      /* Run transition action (if any): */
      if ( transition->action )
         transition->action( fsm->currentState->data, event, targetLeafState->data );

      /* Call entry actions from common ancestor's child down to target leaf state */
      for (int k = j; k >= 0; k--) {
         struct state *entryState = targetPath[k];
         if (entryState->entryAction)
            entryState->entryAction(entryState->data, event);
      }

      /* Update current state to target leaf state */
      nextState = targetLeafState;

      fsm->previousState = fsm->currentState;
      fsm->currentState = nextState;
      
      /* If the state returned to itself: */
      if ( fsm->currentState == fsm->previousState )
         return stateM_stateLoopSelf;

      if ( fsm->currentState == fsm->errorState )
         return stateM_errorStateReached;

      /* If the new state is a final state, notify user that the state
       * machine has stopped: */
      if ( !fsm->currentState->numTransitions )
         return stateM_finalStateReached;

      return stateM_stateChanged;
   } while ( nextState );

   return stateM_noStateChange;
}

struct state *stateM_currentState( struct stateMachine *fsm )
{
   if ( !fsm )
      return NULL;

   return fsm->currentState;
}

struct state *stateM_previousState( struct stateMachine *fsm )
{
   if ( !fsm )
      return NULL;

   return fsm->previousState;
}


static void goToErrorState( struct stateMachine *fsm,
      struct event *const event )
{
   fsm->previousState = fsm->currentState;
   fsm->currentState = fsm->errorState;

   if ( fsm->currentState && fsm->currentState->entryAction )
      fsm->currentState->entryAction( fsm->currentState->data, event );
}

static struct transition *getTransition( struct stateMachine *fsm,
      struct state *state, struct event *const event )
{
   size_t i;

   for ( i = 0; i < state->numTransitions; ++i )
   {
      struct transition *t = &state->transitions[ i ];

      /* A transition for the given event has been found: */
      if ( t->eventType == event->type )
      {
         if ( !t->guard )
            return t;
         /* If transition is guarded, ensure that the condition is held: */
         else if ( t->guard( t->condition, event ) )
            return t;
      }
   }

   /* No transitions found for given event for given state: */
   return NULL;
}

bool stateM_stopped( struct stateMachine *stateMachine )
{
   if ( !stateMachine )
      return true;

   return stateMachine->currentState->numTransitions == 0;
}
