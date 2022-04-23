#ifndef _LVAL_H
#define _LVAL_H

#include <stdlib.h>

struct lval;
typedef struct lval lval;

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

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_FUN,
    LVAL_SEXPR, LVAL_QEXPR };

// Create Enumeration of Possible Error Types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_add(lval* v, lval*x);

#endif
