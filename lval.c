#include "lval.h"


lval* lval_add(lval* v, lval*x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
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


lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long long x = strtoll(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
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

// Construct a pointer to a new number lval
lval* lval_num(long long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
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

lval* lval_join (lval* x, lval* y) {

    // for each cell in 'y' add it to 'x'
    while (y->count) {
        lval_add(x, lval_pop(y, 0));
    }

    //delete the empty 'y' and return 'x'
    lval_del(y);
    return x;
}
