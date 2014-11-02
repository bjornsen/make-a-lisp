#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

#include "mpc.h"


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

float eval_op(char* op, float x, float y) {
	if (strcmp(op, "+") == 0) { return x + y; }
	if (strcmp(op, "-") == 0) { return x - y; }
	if (strcmp(op, "*") == 0) { return x * y; }
	if (strcmp(op, "/") == 0) { return x / y; }
	return 0;
}

float eval(mpc_ast_t* ast) {
	
	// Return numbers immediately
	if (strstr(ast->tag, "number")) {
		return atoi(ast->contents);
	}
	
	// Operators are always the second child
	char* op = ast->children[1]->contents;
	
	float x = eval(ast->children[2]);
			
	// Account for negative numbers
	if (strcmp(op, "-") == 0 && ast->children_num == 4) {
		return -x;
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
  	"                                                    \
			number	 : /-?[0-9]+/ ;																				\
			float    : <number> '.' <number>;	                           \
  		operator : '+' | '-' | '*' | '/' | '^';                  \
  		expr     : <float> | <number> | '(' <operator> <expr>+ ')' ; \
  		bilisp   : /^/ <operator> <expr>+ /$/ ;            \
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
			
			float result = eval(ast);
			printf("%f\n", result);
			mpc_ast_print(ast);
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