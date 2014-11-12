#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

#include "mpc.h"

// Lisp value (lval) types
enum { LVAL_INT, LVAL_FLOAT, LVAL_ERR };

// Error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// Defines possible return values for a lisp value
typedef struct {
	int type;
	int i;
	float f;
	int err;
} lval;

lval lval_int(int x) {
	lval v;
	v.type = LVAL_INT;
	v.i = x;
	return v;
}

lval lval_float(float x) {
	lval v;
	v.type = LVAL_FLOAT;
	v.f = x;
	return v;
}

lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
  return v;
}

// Print an lval
void lval_print(lval v) {
	
	if (v.type == LVAL_FLOAT) {		
		printf("%f", v.f);
	}
	
	if (v.type == LVAL_INT) {
		printf("%i", v.i);
	}
	
	if (v.type == LVAL_ERR) {
		if (v.err == LERR_DIV_ZERO) {
			printf("Error: Division by zero");
		}
		if (v.err == LERR_BAD_OP) {
			printf("Error:  Invalid operator");
		}
		if (v.err == LERR_BAD_NUM) {
			printf("Error:  Invalid number");
		}
	}
	
	printf("\n");
}

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

lval eval_op(char* op, lval x, lval y) {
	
	if (x.type == LVAL_ERR) { return x; }
	if (y.type == LVAL_ERR) { return y; } 
	
	// If both values are integers, operate on the integers
	if ((x.type == LVAL_INT) && (y.type == LVAL_INT)) { 
			
		if (strcmp(op, "+") == 0) { return lval_int(x.i + y.i); }
		if (strcmp(op, "-") == 0) { return lval_int(x.i - y.i); }
		if (strcmp(op, "*") == 0) { return lval_int(x.i * y.i); }
		if (strcmp(op, "%") == 0) { return lval_int(x.i % y.i); }
		if (strcmp(op, "/") == 0) {
			if (y.i == 0) {
				return lval_err(LERR_DIV_ZERO);
			}
			else if (x.i % y.i == 0) {
				return lval_int(x.i / y.i);
			}
			else {
				
				return lval_float((float)x.i / (float)y.i);
			}
		}
		if (strcmp(op, "max") == 0) {
			if (x.i > y.i) {
				return x;
			}
			else {
				return y;
			}
		}
		if (strcmp(op, "min") == 0) {
			if (x.i < y.i) {
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
		if (x.type == LVAL_INT) {
			x = lval_float(x.i);
		}
		if (y.type == LVAL_INT) {
			y = lval_float(y.i);
		}
		if (strcmp(op, "+") == 0) { return lval_float(x.f + y.f); }
		if (strcmp(op, "-") == 0) { return lval_float(x.f - y.f); }
		if (strcmp(op, "*") == 0) { return lval_float(x.f * y.f); }
		if (strcmp(op, "/") == 0) {
			return y.f == 0
				? lval_err(LERR_DIV_ZERO)
				: lval_float(x.f / y.f); 
		}
		
		if (strcmp(op, "max") == 0) {
			if (x.f > y.f) {
				return x;
			}
			else {
				return y;
			}
		}
		if (strcmp(op, "min") == 0) {
			if (x.f < y.f) {
				return x;
			}
			else {
				return y;
			}
		}
	}
	
	return lval_err(LERR_BAD_OP);
}

lval parse_float(mpc_ast_t* ast) {
	errno = 0;
	int size = ast->children_num * sizeof(char);
	char f[size];
	for (int i = 0; i < ast->children_num; i++) {
		f[i] = *ast->children[i]->contents;
	}
	float x = strtof(f, NULL);
	return errno != ERANGE ? lval_float(x) : lval_err(LERR_BAD_NUM);
}

lval eval(mpc_ast_t* ast) {
	
	// Return numbers immediately
	if (strstr(ast->tag, "integer")) {
		// Error checking
		errno = 0;
		long x = strtol(ast->contents, NULL, 10);
		return errno != ERANGE ? lval_int(x) : lval_err(LERR_BAD_NUM);
	}
	
	if (strstr(ast->tag, "float")) {
		// Error checking
		return parse_float(ast);
	}
	
	// Operators are always the second child
	char* op = ast->children[1]->contents;
	
	lval x = eval(ast->children[2]);
			
	/* Account for negative numbers.  Children_num is 4 because that indicates
	 * there is only one number in addition to the operator */
	if (strcmp(op, "-") == 0 && ast->children_num == 4) {
		return lval_float(-x.f);
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
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Bilisp = mpc_new("bilisp");

	mpca_lang(MPCA_LANG_DEFAULT,
  	" \
			integer	 : /-?[0-9]+/ ;	\
			float    : <integer> '.' <integer> ;	\
  		operator : '+' \
  						 | '-' \
  						 | '*' \
  						 | '/' \
  						 | '^' \
  						 | '%' \
  						 | \"max\" \
  						 | \"min\" ; \
  		expr     : <float> \
  						 | <integer> \
  						 | '(' <operator> <expr>+ ')' ; \
  		bilisp   : /^/ <operator> <expr>+ /$/ ; \
  	",
  	Integer, Float, Operator, Expr, Bilisp);
	
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
			
			lval result = eval(ast);
			lval_print(result);

			mpc_ast_delete(ast);
			
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
		
	}
	
	/* Undefine and delete parsers */
	mpc_cleanup(5, Integer, Float, Operator, Expr, Bilisp);
	
	return 0;
}