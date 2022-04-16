compile:
	gcc -std=c99 -Wall prompt.c -ledit -o prompt
	gcc -std=c99 -Wall parsing.c mpc.c -ledit -lm -o parsing
prompt:
	./prompt
parsing:
	./parsing

