#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <editline/readline.h>

#include "mpc.h"

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

// Lisp value (lval) types
enum { LVAL_INT, LVAL_FLOAT, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_ERR };

// Error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// Defines possible return values for a lisp value
typedef struct lval lval;
struct lval {
	int type;
	long i;
	double f;
	// Error and Symbol types have some string data
	char* err;
	char* sym;
	// Count and pointer to a list of "lval*"
	int count;
	struct lval** cell;
};

lval* lval_int(int x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_INT;
	v->i = x;
	return v;
}

lval* lval_float(float x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FLOAT;
	v->f = x;
	return v;
}

lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

lval* lval_sexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_err(char* m) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);
  return v;
}

void lval_del(lval* v) {
	
	switch (v->type) {
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		
		// Q-expressions and S-expressions are deallocated in the same way
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			for (int i = 0; i < v-> count; i++) {
				lval_del(v->cell[i]);
			}
			free(v->cell);
		break;
		}
		
		free(v);
}

lval* lval_read_int(mpc_ast_t* ast) {
	errno = 0;
	long x = strtol(ast->contents, NULL, 10);
	return errno != ERANGE ?
		lval_int(x) : lval_err("Invalid integer");
}

lval* lval_read_float(mpc_ast_t* ast) {
	errno = 0;
	char* float_string;
	float_string = (char *) malloc(sizeof(char));
  *float_string = '\0';
	for (int i = 0; i < ast->children_num; i++) {
		char* child = ast->children[i]->contents;
		float_string = realloc(float_string, strlen(float_string) + strlen(child));
		strcat(float_string, child);
	}
	float x = strtod(float_string, NULL);
	free(float_string);
	return errno != ERANGE ? lval_float(x) : lval_err("Invalid float");
}

lval* lval_add(lval* list, lval* element) {
	list->count++;
	list->cell = realloc(list->cell, sizeof(lval*) * list->count);
	list->cell[list->count-1] = element;
	return list;
}

lval* lval_read(mpc_ast_t* ast) {
	
	if (strstr(ast->tag, "int")) { return lval_read_int(ast); }
	if (strstr(ast->tag, "float")) { return lval_read_float(ast); }
	if (strstr(ast->tag, "symbol")) { return lval_sym(ast->contents); }
	
	// If root ">", sexpr, or qexpr then create empty list
	lval* x = NULL;
	if (strcmp(ast->tag, ">") == 0) { x = lval_sexpr(); }
	if (strcmp(ast->tag, "sexpr")) { x = lval_sexpr(); }
	if (strstr(ast->tag, "qexpr")) { x = lval_qexpr(); }
	
	for (int i = 0; i < ast->children_num; i++) {
		if (strcmp(ast->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(ast->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(ast->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(ast->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(ast->children[i]->tag, "regex") == 0) { continue; }
		x = lval_add(x, lval_read(ast->children[i]));
	}
	
	return x;
}

void lval_print(lval* v);

void lval_expr_print(lval* list, char open, char close) {
	putchar(open);
	for (int i = 0; i < list->count; i++) {
		lval_print(list->cell[i]);
		
		if (i != (list->count-1)) {
			putchar(' ');
		}
	}
	putchar(close);
}

// Print an lval
void lval_print(lval* v) {
	switch (v->type) {
		case LVAL_INT: printf("%li", v->i); break;
		case LVAL_FLOAT: printf("%f", v->f); break;
		case LVAL_ERR: printf("%s", v->err); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
	}
}

void lval_println(lval* v) {lval_print(v); putchar('\n'); }

void print_children_details(mpc_ast_t* ast) {
	mpc_ast_t* child;
	for (int i = 0; i < ast->children_num; i++) {
		child = ast->children[i];
		printf("\n------ Child Number %i ------\n", i);
		printf("Child %i tag: %s\n", i, child->tag);
		printf("Child %i contents: %s\n", i, child->contents);
		printf("Child %i number of children: %i\n", i, child->children_num);
		printf("\n");
	}
}

lval* lval_pop(lval* v, int i) {
	// Get the item at "i"
	lval* item = v->cell[i];
	
	// Shift memory after the item at "i" over the top
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
	
	// Decrease the count
	v->count--;
	
	// Reallocate the memory used
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return item;
}

lval* lval_take(lval* v, int i) {
	lval* item = lval_pop(v, i);
	lval_del(v);
	return item;
}

lval* builtin_head(lval* a) {
	LASSERT(a, a->count == 1,
	  "Function 'head' passed too many arguments");
	
	LASSERT(a, a->type != LVAL_QEXPR,
	  "Function 'head' requires a Q-expression");
	
	LASSERT(a, a->cell[0]->count == 0, 
		"Function 'head' passed an emptry Q-expression");
	
	// Take the first element and delete the remaining
	lval* v = lval_take(a, 0);
	while (v->count > 1) { lval_del(lval_pop(v,1)); }
	return v;
}

lval* builtin_tail(lval* a) {
	LASSERT(a, a->count != 1,
	  "Function 'head' passed too many arguments");
	
	LASSERT(a, a->cell[0]->type != LVAL_QEXPR,
	  "Function 'head' requires a Q-expression");
	
	LASSERT(a, a->cell[0]->count == 0, 
	  "Function 'tail' passed empty Q-epxression {}");
	
	// Take the first argument, delete the first element, and return
	lval* v = lval_take(a,0);
	lval_del(lval_pop(v,0));
	return v;
}

lval* builtin_op(lval* a, char* op) {
	
	// Check to make sure we're operating on numbers
	for (int i = 0; i < a->count; i++) {
		int type = a->cell[i]->type;
		if ((type != LVAL_INT) && (type != LVAL_FLOAT))  {
			lval_del(a);
			return lval_err("Cannot operate on non-number!");
		}
	}
	
	lval* x = lval_pop(a, 0);
	bool is_float = false;
	
	if (x->type == LVAL_FLOAT) {
		is_float = true;
	}
	
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		if (is_float) {
			x->f = -x->f;
		}
		else {
			x->i = -x->i;
		}
	}
	
	while (a->count > 0) {
		
		lval* y = lval_pop(a, 0);
		
		if (y->type == LVAL_FLOAT && !is_float) {
			is_float = true;
			x = lval_float((float) x->i);
		}
		
		if (is_float) {
			
			if (y->type == LVAL_INT) {
				y = lval_float((float) y->i);
			}
			
			if (strcmp(op, "+") == 0) { x->f += y->f; }
			if (strcmp(op, "-") == 0) { x->f -= y->f; }
			if (strcmp(op, "*") == 0) { x->f *= y->f; }
			if (strcmp(op, "/") == 0) {
				if (y->f == 0) {
					return lval_err("Division by zero!");
				} 
				else {
					x->f /= y->f; 
				}
			}
		
		if (strcmp(op, "max") == 0) {
			if (x->f < y->f) {
				x->f = y->f;
			}
		}
		if (strcmp(op, "min") == 0) {
			if (x->f > y->f) {
				x->f = y->f;
			}
		}
			
		} 
		// Integer operations
		else {
			if (strcmp(op, "+") == 0) { x->i += y->i; }
			if (strcmp(op, "-") == 0) { x->i -= y->i; }
			if (strcmp(op, "*") == 0) { x->i *= y->i; }
			if (strcmp(op, "%") == 0) { x->i %= y->i; }
			if (strcmp(op, "/") == 0) {
				if (y->i == 0) {
					return lval_err("Division by zero!");
				}
				else if (x->i % y->i == 0) {
					x->i /= y->i;
				}
				else {
					x = lval_float((float)x->i / (float)y->i);
				}
			}
			if (strcmp(op, "max") == 0) {
				if (x->i < y->i) {
					x->i = y->i;
				}
			}
			if (strcmp(op, "min") == 0) {
				if (x->i > y->i) {
					x->i = y->i;
				}
			}
		}
		lval_del(y);
	}
	
	lval_del(a);
	return x;
}

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {
	
	// Evaluate children
	for (int i = 0; i < v-> count; i++) {
		v->cell[i] = lval_eval(v->cell[i]);
	}
	
	// Error checking
	for (int i = 0; i < v->count; i++) {
		lval* x = v->cell[i];
		int type = x->type;
		if (type == LVAL_ERR) {
			return lval_take(v, i);
		}
	}
	
	// Empty expression
	if (v->count == 0 ) { return v; }
	
	// Single expression
	if (v-> count == 1) { return lval_take(v, 0); }
	
	// Ensure first element is a symbol
	lval* first = lval_pop(v, 0);
	if (first->type != LVAL_SYM) {
		lval_del(first); 
		lval_del(v);
		return lval_err("S-expression does not start with a symbol!");
	}
	
	// Call builtin with operator
	lval* result = builtin_op(v, first->sym);
	lval_del(first);
	return result;
}

lval* lval_eval(lval* v) {
	// Evaluate S-expressions
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
	// Return anything that isn't a S-expression
	return v;
}

int main(int argc, char** argv) {
	
	/* Create parsers */
	mpc_parser_t* Integer = mpc_new("integer");
	mpc_parser_t* Float = mpc_new("float");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Qexpr = mpc_new("qexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Bilisp = mpc_new("bilisp");

	mpca_lang(MPCA_LANG_DEFAULT,
  	" \
			integer	 : /-?[0-9]+/ ;	\
			float    : <integer> '.'<integer>;	\
  		symbol   : '+' \
  						 | '-' \
  						 | '*' \
  						 | '/' \
  						 | '^' \
  						 | '%' \
  						 | \"max\" \
  						 | \"min\" \
  						 | \"list\" \
  						 | \"head\" \
  						 | \"tail\" \
  						 | \"join\" \
  						 | \"eval\" ; \
  		sexpr    : '(' <expr>* ')' ; \
  		qexpr    : '{' <expr>* '}' ; \
  		expr     : <float> \
  						 | <integer> \
  						 | <symbol> \
  						 | <qexpr> \
  						 | <sexpr> ; \
  		bilisp   : /^/ <expr>* /$/ ; \
  	",
  	Integer, Float, Symbol, Sexpr, Qexpr, Expr, Bilisp);
	
	puts("Bilisp 0.0.0.0.1");
	puts("Press Ctrl+c to Exit\n");
	
	while (1) {
		
		char* input = readline("bilisp> ");
		
		add_history(input);
		
		/* Attempt to parse the user input */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Bilisp, &r)) {
			mpc_ast_t* ast = r.output;
			
//			mpc_ast_print(ast);
			
			lval* l = lval_read(ast);
			
//			lval_println(l);
			
			lval* x = lval_eval(l);
			lval_println(x);
			
			lval_del(x);

			mpc_ast_delete(ast);
			
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
		
	}
	
	/* Undefine and delete parsers */
	mpc_cleanup(7, Integer, Float, Symbol, Sexpr, Qexpr, Expr, Bilisp);
	
	return 0;
}