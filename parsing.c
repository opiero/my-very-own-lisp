#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpc.h"

//if we are compiling on windows compile these functions
#ifdef _WIN32

static char buffer[2048];

// fake readline function
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy
}

// fake add_history function
void add_history(char* unused) {}


// otherwise include the editline headers
#else
#include <editline/readline.h>
#endif

// Create Enumeration of Possible lval Types
enum { LVAL_NUM, LVAL_ERR };

// Create Enumeration of Possible Error Types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// Declare new lval struct
typedef struct {
    int type;
    long long num;
    int err;
} lval;

// Create new number type lval
lval lval_num(long long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

// Create a new error type lval
lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.num = x;
    return v;
}

// Print an "lval"
void lval_print (lval v) {
    switch (v.type) {
        // In the case the type is a number, print it
        // Then 'break' out of the switch
        case LVAL_NUM:
            printf("%lli", v.num);
            break;

        // In the case the type is an error
        case LVAL_ERR:
            // Check what type of error it is and print it
            if (v.err == LERR_DIV_ZERO) {
                printf("Error: Division By Zero!");
            }
            if (v.err == LERR_BAD_OP) {
                printf("Error: Invalid Operator!");
            }
            if (v.err == LERR_BAD_NUM) {
                printf("Error: Invalid Number!");
            }
            break;
    }
}

// Print an "lval" followed by a newline
void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

int number_of_nodes(mpc_ast_t* t) {
    if (t->children_num == 0) return 1;
    if (t->children_num >= 1) {
        int total = 1;
        for(int i = 0; i < t->children_num; i++){
            total += number_of_nodes(t->children[i]);

        }
        return total;
    }
    return 0;
}

long long eval_op (long long x, char* op, long long y) {
    if (strcmp(op, "+") == 0) return x+y;
    if (strcmp(op, "-") == 0) return x-y;
    if (strcmp(op, "*") == 0) return x*y;
    if (strcmp(op, "/") == 0) return x/y;
    return 0;
}

long long eval (mpc_ast_t* t) {
    if ( strstr(t->tag, "number") ) {
        return atoi(t->contents);
    }

    char* op = t->children[1]->contents;

    long long x = eval(t->children[2]);

    int i = 3;
    while( strstr(t->children[i]->tag, "expr")  ){
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main (int argc, char** argv) {

    // Create some parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // Define them with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
            " \
            number: /-?[0-9]+/; \
            operator: '+'|'-'|'*'|'/' ;\
            expr: <number> | '(' <operator> <expr>+ ')'; \
            lispy: /^/ <operator> <expr>+ /$/; \
            ",
            Number, Operator, Expr, Lispy
            );

    // Print Version and Exit Information
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while(1) {

        // Output the prompt and get input
        char* input = readline("lispy> ");

        // Add input to history
        add_history(input);

        // attempt to parse the user input
        mpc_result_t r;
        if ( mpc_parse("<stdin>", input, Lispy, &r) ) {

            long long result = eval(r.output);
            printf("%lld\n", result);
            mpc_ast_delete(r.output);
        }
        else {
            //otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        // free retrieved input
        free(input);
    }

    //undefine and Delete our Parsers
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return EXIT_SUCCESS;
}
