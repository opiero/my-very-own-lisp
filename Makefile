all: compile run
compile:
	gcc -std=c99 -Wall interactive.c -ledit -o output
run:
	./output
clean:
	rm ./output

