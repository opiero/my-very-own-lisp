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
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

// Create Enumeration of Possible Error Types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// Declare new lval struct
typedef struct lval {
    int type;
    long long num;
    // Error and symbol types have some string data
    char* err;
    char* sym;
    // Count and Pointer to a list of "lval"
    int count;
    struct lval** cell;
} lval;

// Construct a pointer to a new number lval
lval* lval_num(long long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// Construct a pointer to a new error lval
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) * sizeof(char) + 1);
    strcpy(v->err, m);
    return v;
}

// Construct a pointer to a new symbol lval
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) * sizeof(char) + 1);
    strcpy(v->sym, s);
    return v;
}

// Construct a poiter to a new sexpr lval
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {

    switch (v->type) {
        // Do nothing special for number type
        case LVAL_NUM: break;

        // For Err or Sym free the string data
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        // If Sexpr then delete all elements inside
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }

            // also free the memory allocated to contain the pointers
            free(v->cell);
            break;
    }

    // also free the memory allocated from the "lval" struct itself
    free(v);
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
            if (v.num == LERR_DIV_ZERO) {
                printf("Error: Division By Zero!");
            }
            if (v.num == LERR_BAD_OP) {
                printf("Error: Invalid Operator!");
            }
            if (v.num == LERR_BAD_NUM) {
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

lval eval_op(lval x, char *op, lval y){
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    if (strcmp(op, "+") == 0) {return lval_num (x.num + y.num);}
    if (strcmp(op, "-") == 0) {return lval_num (x.num - y.num);}
    if (strcmp(op, "*") == 0) {return lval_num (x.num * y.num);}
    if (strcmp(op, "/") == 0) {
        printf("%lld\n", y.num);
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    }
    puts("bad op");
    return lval_err(LERR_BAD_OP);
}


lval eval (mpc_ast_t* t) {
    if ( strstr(t->tag, "number") ) {
        // check if there is some error in conversion
        errno = 0;
        long long x = strtoll(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    char* op = t->children[1]->contents;

    lval x = eval(t->children[2]);

    int i = 3;
    while(strstr(t->children[i]->tag, "expr")){
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main (int argc, char** argv) {

    // Create some parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // Define them with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
            " \
            number: /-?[0-9]+/; \
            symbol: '+' | '-' | '*' | '/'; \
            sexpr: '(' <expr>* ')'; \
            expr: <number> | <symbol> | <sexpr> ;\
            lispy : /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Expr, Lispy);

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

        /*
         * FIXME: with the following parsing/evaluation, invalid op and
         * invalid number errors won't be printed to the user
         * */

        if ( mpc_parse("<stdin>", input, Lispy, &r) ) {

            lval result = eval(r.output);
            lval_println(result);
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
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
    return EXIT_SUCCESS;
}
