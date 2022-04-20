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

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long long x = strtoll(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_add(lval* v, lval*x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval* lval_read(mpc_ast_t* t) {

    //if symbol or number, return conversion to that type
    if (strstr(t->tag, "number")) {return lval_read_num(t);}
    if (strstr(t->tag, "symbol")) {return lval_sym(t->contents);}

    //if root (>) or sexpr then create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) {x = lval_sexpr();}
    if (strstr(t->tag, "sexpr")) {x = lval_sexpr();}

    //Fill this list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
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

void lval_print(lval* v);
void lval_expr_print(lval* v, char open, char close);

void lval_print (lval* v) {
    switch(v->type) {
        case LVAL_NUM: printf("%lli", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    }
}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {

        //Print Value contained within
        lval_print(v->cell[i]);

        //don't print trailing space for the last element
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_println(lval* x) {
    lval_print(x);
    putchar('\n');
}

lval* lval_eval_sexpr (lval* v);

lval* lval_eval(lval* v) {
    //evaluate Sexpressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    //all other lval types remain the same
    return v;
}
lval* lval_pop(lval* v, int i) {
    //find the item at "i"
    lval* x = v->cell[i];

    //shift memory after the item at "i" over the top
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    //decrease the count of items in the list
    v->count--;

    // reallocate the memory used
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}
lval* lval_take(lval* v, int i) {
    lval * x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_eval_sexpr (lval* v) {

    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    //error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    //empty expression
    if (v->count == 0) { return v; }

    //single expression
    if (v->count == 1) { return lval_take(v, 0); }

    //ensure first element is symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err ("S-expression Does not start with symbol!");
    }

    //call builtin operator
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
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

        if ( mpc_parse("<stdin>", input, Lispy, &r) ) {

            lval* x = lval_read(r.output);
            lval_println(x);
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
