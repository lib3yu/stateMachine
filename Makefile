default: clean dist

dist:
	mkdir -p bin/
	gcc -O0 -g -std=c99 -I src src/stateMachine.c examples/stateMachineExample.c  -o bin/example
	gcc -O0 -g -std=c99 -I src src/stateMachine.c examples/loginStateMachineExample.c  -o bin/login_example
	gcc -O0 -g -std=c99 -Isrc -Iexamples src/stateMachine.c examples/queue.c examples/motorCtrlStateMachine.c  -o bin/motor_example

test:
	mkdir -p bin/
	gcc -std=c99 -I src src/stateMachine.c tests/nestedTest.c  -o bin/test
	
clean:
	rm -rf bin