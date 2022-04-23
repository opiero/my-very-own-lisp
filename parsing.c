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

#define LASSERT(args, cond, err) \
    if(!(cond)) {lval_del(args); return lval_err(err);}

//Forward Declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// Lisp Value

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_FUN,
    LVAL_SEXPR, LVAL_QEXPR };

typedef lval* (*lbuiltin) (lenv*, lval*);

// Declare new lval struct
struct lval {
    int type;
    long long num;
    char* err;
    char* sym;
    lbuiltin fun;

    int count;
    lval** cell;
};

struct lenv{
    int count;
    char** syms;
    lval** vals;
};

// Create Enumeration of Possible Error Types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };


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

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
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

// Constructs a pointer to a new qexpr lval
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {

    switch (v->type) {
        // Do nothing special for number or func type
        case LVAL_NUM: break;
        case LVAL_FUN: break;

                       // For Err or Sym free the string data
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

                       // If Sexpr or Qexpr then delete all elements inside
        case LVAL_QEXPR:
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
    if (strstr(t->tag, "qexpr")) {x = lval_qexpr();}

    //Fill this list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval* lval_copy(lval* v) {

    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {

        //Copy functions and numbers directly
        case LVAL_NUM: x->num = v->num; break;
        case LVAL_FUN: x->fun = v->fun; break;

        //Copy strings using malloc and strcpy
        case LVAL_ERR:
            x->err = malloc((strlen(v->err) + 1) * sizeof(char));
            strcpy(x->err, v->err); break;

        case LVAL_SYM:
            x->sym = malloc((strlen(v->sym) + 1) * sizeof(char));
            strcpy(x->sym, v->sym);
            break;

        // Copy lists by copying each sub expression
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * v->count);
            for (int i = 0; i < v->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

void lval_print(lval* v);
void lval_expr_print(lval* v, char open, char close);

void lval_print (lval* v) {
    switch(v->type) {
        case LVAL_NUM: printf("%lli", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_FUN: printf("<function>"); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
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

lval* lval_eval_sexpr (lenv* e, lval* v);

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
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

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {

    //Iterate over all items in environment
    for (int i = 0; i < e->count; i++) {
        // Check if the stored string matches the symbol string
        // If it does, return a copy of the value
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    //if no symbol found return error
    return lval_err("unbound symbol!");
}

void lenv_put(lenv* e, lval* k, lval* v) {

    // Iterate over all items in environment
    // This is to see if variable already exists
    for (int i = 0; i < e->count; i++) {

        // if variable is found delete item at that position
        // and replace with variable supplied by user
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    // if no existing entry found allocate space for new entry
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    // copy contents of lval and symbol string into new location
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc((strlen(k->sym)+1) * sizeof(char));
    strcpy(e->syms[e->count-1], k->sym);
}

lval* builtin_head(lenv* e, lval* a) {

    LASSERT(a, a->count == 1,
        "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'head' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'head' passed {}!");

    //otherwise we take the first argument
    lval* v = lval_take(a, 0);

    //delete all elements that are not head and return
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {

    LASSERT(a, a->count == 1,
        "Function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'tail' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'tail' passed {}!");
    //Take first argument
    lval* v = lval_take(a, 0);

    //Delete first element and return
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT(a, a->count == 1,
            "Function 'eval' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect type");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval* lval_join (lval* x, lval* y) {

    // for each cell in 'y' add it to 'x'
    while (y->count) {
        lval_add(x, lval_pop(y, 0));
    }

    //delete the empty 'y' and return 'x'
    lval_del(y);
    return x;
}

lval* builtin_join(lenv* e, lval* a) {

    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type.");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(x);
    return x;
}

lval * builtin_op(lenv* e, lval* a, char* op) {

    //Ensure all arguments are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    //pop the first element
    lval* x = lval_pop(a, 0);

    //if no arguments and sub then perform unary negation
    if (strcmp(op, "-") == 0 && a->count == 0) {
        x->num = -x->num;
    }

    //while there are still elements remaining
    while (a->count > 0) {

        //pop the next element
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division by Zero!"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);

    }

    lval_del(a);
    return x;

}

lval * builtin_op(lenv* e, lval* a, char* op);

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin(lenv* e, lval* a, char* func) {
    if (strcmp("list", func) == 0) {return builtin_list(e, a);}
    if (strcmp("head", func) == 0) {return builtin_head(e, a);}
    if (strcmp("tail", func) == 0) {return builtin_tail(e, a);}
    if (strcmp("join", func) == 0) {return builtin_join(e, a);}
    if (strcmp("eval", func) == 0) {return builtin_eval(e, a);}
    if (strstr("+-/*", func)) {return builtin_op(e, a, func);}
    lval_del(a);
    return lval_err("Unknown Function!");
}

lval* lval_eval_sexpr (lenv* e, lval* v) {

    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    if (v->count == 0) { return v; }
    if (v->count == 1) { return lval_take(v, 0); }

    //Ensure first element is a function after evaluation
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(v); lval_del(f);
        return lval_err("first element is not a function");
    }

    //If so call function to get result
    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

int main (int argc, char** argv) {

    // Create some parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // Define them with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
            " \
            number: /-?[0-9]+/; \
            symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/; \
            sexpr: '(' <expr>* ')'; \
            qexpr: '{' <expr>* '}'; \
            expr: <number> | <symbol> | <sexpr> | <qexpr> ;\
            lispy : /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

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

            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
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
    mpc_cleanup(5, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return EXIT_SUCCESS;
}
