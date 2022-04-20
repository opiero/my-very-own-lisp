all: compile run
debug: compile
	gdb ./parsing
compile:
	gcc -std=c99 -Wall parsing.c mpc.c -ledit -o parsing
run:
	./parsing
leaks:
	gcc -std=c99 -Wall -g parsing.c mpc.c -ledit -o parsing
	leaks --atExit -- ./parsing
clean:
	rm -rf parsing *.dSYM
