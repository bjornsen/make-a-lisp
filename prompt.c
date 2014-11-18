#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

#include "mpc.h"

// Lisp value (lval) types
enum { LVAL_INT, LVAL_FLOAT, LVAL_SYM, LVAL_SEXPR, LVAL_ERR };

// Error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// Defines possible return values for a lisp value
typedef struct {
	int type;
	long i;
	double f;
	// Error and Symbol types have some string data
	char* err;
	char* sym;
	// Count and pointer to a list of "lval*"
	int count;
	struct lval** cell;
} lval;

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

lval* lval_err(char* m) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);
  return v;
}

void lval_del(lval* v) {
	
	switch (v-> type) {
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		
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
	
	// If root ">" or sexpr then create empty list
	lval* x = NULL;
	if (strcmp(ast->tag, ">") == 0) { x = lval_sexpr(); }
	if (strcmp(ast->tag, "sexpr")) { x = lval_sexpr(); }
	
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

lval* eval_op(char* op, lval* x, lval* y) {
	
	if (x->type == LVAL_ERR) { return x; }
	if (y->type == LVAL_ERR) { return y; } 
	
	// If both values are integers, operate on the integers
	if ((x->type == LVAL_INT) && (y->type == LVAL_INT)) { 
			
		if (strcmp(op, "+") == 0) { return lval_int(x->i + y->i); }
		if (strcmp(op, "-") == 0) { return lval_int(x->i - y->i); }
		if (strcmp(op, "*") == 0) { return lval_int(x->i * y->i); }
		if (strcmp(op, "%") == 0) { return lval_int(x->i % y->i); }
		if (strcmp(op, "/") == 0) {
			if (y->i == 0) {
				return lval_err(LERR_DIV_ZERO);
			}
			else if (x->i % y->i == 0) {
				return lval_int(x->i / y->i);
			}
			else {
				
				return lval_float((float)x->i / (float)y->i);
			}
		}
		if (strcmp(op, "max") == 0) {
			if (x->i > y->i) {
				return x;
			}
			else {
				return y;
			}
		}
		if (strcmp(op, "min") == 0) {
			if (x->i < y->i) {
				return x;
			}
			else {
				return y;
			}
		}
	} 
	// If any values are float, the result will need to be a float
	else {
		// Explicitly cast any integers to floats
		if (x->type == LVAL_INT) {
			x = lval_float(x->i);
		}
		if (y->type == LVAL_INT) {
			y = lval_float(y->i);
		}
		if (strcmp(op, "+") == 0) { return lval_float(x->f + y->f); }
		if (strcmp(op, "-") == 0) { return lval_float(x->f - y->f); }
		if (strcmp(op, "*") == 0) { return lval_float(x->f * y->f); }
		if (strcmp(op, "/") == 0) {
			return y->f == 0
				? lval_err(LERR_DIV_ZERO)
				: lval_float(x->f / y->f); 
		}
		
		if (strcmp(op, "max") == 0) {
			if (x->f > y->f) {
				return x;
			}
			else {
				return y;
			}
		}
		if (strcmp(op, "min") == 0) {
			if (x->f < y->f) {
				return x;
			}
			else {
				return y;
			}
		}
	}
	
	return lval_err("Bad operator!");
}

lval* eval(mpc_ast_t* ast) {
	
	// Return numbers immediately
	if (strstr(ast->tag, "integer")) {
		// Error checking
		errno = 0;
		long x = strtol(ast->contents, NULL, 10);
		return errno != ERANGE ? lval_int(x) : lval_err(LERR_BAD_NUM);
	}
	
	if (strstr(ast->tag, "float")) {
		// Error checking
		return lval_read_float(ast);
	}
	
	// Operators are always the second child
	char* op = ast->children[1]->contents;
	
	lval* x = eval(ast->children[2]);
			
	/* Account for negative numbers.  Children_num is 4 because that indicates
	 * there is only one number in addition to the operator */
	if (strcmp(op, "-") == 0 && ast->children_num == 4) {
		return lval_float(-x->f);
	}
	
	// Evalute remaining child expressions
	int i = 3;
	while (strstr(ast->children[i]->tag, "expr") || strstr(ast->children[i]->tag, "float")) {
		x = eval_op(op, x, eval(ast->children[i]));
		i++;
	}
	
	return x;
}

int main(int argc, char** argv) {
	
	/* Create parsers */
	mpc_parser_t* Integer = mpc_new("integer");
	mpc_parser_t* Float = mpc_new("float");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
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
  						 | \"min\" ; \
  		sexpr    : '(' <expr>* ')' ; \
  		expr     : <float> \
  						 | <integer> \
  						 | <symbol> \
  						 | <sexpr> ; \
  		bilisp   : /^/ <expr>* /$/ ; \
  	",
  	Integer, Float, Symbol, Sexpr, Expr, Bilisp);
	
	puts("Bilisp 0.0.0.0.1");
	puts("Press Ctrl+c to Exit\n");
	
	while (1) {
		
		char* input = readline("bilisp> ");
		
		add_history(input);
		
		/* Attempt to parse the user input */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Bilisp, &r)) {
			mpc_ast_t* ast = r.output;
			
			mpc_ast_print(ast);
						
			lval* x = lval_read(ast);
			lval_println(x);

			mpc_ast_delete(ast);
			
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
		
	}
	
	/* Undefine and delete parsers */
	mpc_cleanup(5, Integer, Float, Symbol, Sexpr, Expr, Bilisp);
	
	return 0;
}