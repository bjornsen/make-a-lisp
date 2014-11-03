#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

#include "mpc.h"

// Lisp value (lval) types
enum { LVAL_NUM, LVAL_ERR };

// Error types
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// Defines possible return values for a lisp value
typedef struct {
	int type;
	float num;
	int err;
} lval;


lval lval_num(float x) {
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
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
	
	if (v.type == LVAL_NUM) {
		printf("%f", v.num);
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
	
	if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
	if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
	if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
	if (strcmp(op, "/") == 0) {
		return y.num == 0
			? lval_err(LERR_DIV_ZERO)
			: lval_num(x.num / y.num); 
	}
	if (strcmp(op, "max") == 0) {
		if (x.num > y.num) {
			return x;
		}
		else {
			return y;
		}
	}
	if (strcmp(op, "min") == 0) {
		if (x.num < y.num) {
			return x;
		}
		else {
			return y;
		}
	}
	
	return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* ast) {
	
	// Return numbers immediately
	if (strstr(ast->tag, "number")) {
		// Error checking
		errno = 0;
		float x = strtof(ast->contents, NULL);
		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
	}
	
	// Operators are always the second child
	char* op = ast->children[1]->contents;
	
	lval x = eval(ast->children[2]);
			
	/* Account for negative numbers.  Children_num is 4 because that indicates
	 * there is only one number in addition to the operator */
	if (strcmp(op, "-") == 0 && ast->children_num == 4) {
		return lval_num(-x.num);
	}
	
	// Evalute remaining child expressions
	int i = 3;
	while (strstr(ast->children[i]->tag, "expr")) {
		x = eval_op(op, x, eval(ast->children[i]));
		i++;
	}
	
	return x;
}

int main(int argc, char** argv) {
	
	/* Create parsers */
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Float = mpc_new("float");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Bilisp = mpc_new("bilisp");

	mpca_lang(MPCA_LANG_DEFAULT,
  	" \
			number	 : /-?[0-9]+/ ;	\
			float    : <number> '.' <number> ;	\
  		operator : '+' \
  						 | '-' \
  						 | '*' \
  						 | '/' \
  						 | '^' \
  						 | \"max\" \
  						 | \"min\" ; \
  		expr     : <float> \
  						 | <number> \
  						 | '(' <operator> <expr>+ ')' ; \
  		bilisp   : /^/ <operator> <expr>+ /$/ ; \
  	",
  	Number, Float, Operator, Expr, Bilisp);
	
	puts("Bilisp 0.0.0.0.1");
	puts("Press Ctrl+c to Exit\n");
	
	while (1) {
		
		char* input = readline("lispy> ");
		
		add_history(input);
		
		/* Attempt to parse the user input */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Bilisp, &r)) {
			mpc_ast_t* ast = r.output;
			
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
	mpc_cleanup(5, Number, Float, Operator, Expr, Bilisp);
	
	return 0;
}