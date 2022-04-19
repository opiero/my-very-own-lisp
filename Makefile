all: compile run
debug: compile
	gdb ./parsing
compile:
	gcc -std=c99 -Wall parsing.c mpc.c -ledit -o parsing
run:
	./parsing
clean:
	rm parsing
