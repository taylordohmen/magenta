%{
	#include <stdio.h>
	#define YYDEBUG 1
	int yywrap();
	int yylex();
	void yyerror(const char* str);
%}

%union {
	struct Node* node;
	int var;
}

%token BRACE_OPEN      
%token BRACE_CLOSE     
%token ASSIGN          
%token COLON 
%token SEMICOLON          

%token TYPE_FUNCTION
%token TYPE_METHOD   
%token TYPE_INTEGER    
%token TYPE_STRING     
%token TYPE_FLOAT      
%token TYPE_COMPOUND   
%token OPTION_STMT     
%token CASE_STMT       
%token IF_STMT         
%token ELIF_STMT       
%token ELSE_STMT       
%token FOR_LOOP        
%token WHILE_LOOP      

%token ACCESS          
%token BRACKET_OPEN    
%token BRACKET_CLOSE   
%token PAREN_OPEN      
%token PAREN_CLOSE     
%token POWER           
%token LOG             
%token TIMES           
%token DIVIDE          
%token MODULO          
%token PLUS            
%token CONCAT          
%token MINUS           

%token LESS_THAN       
%token GREATER_THAN    
%token EQUAL           
%token NOT_EQUAL       
%token GREATER_EQUAL   
%token LESS_EQUAL      

%token LOG_AND         
%token LOG_OR          
%token LOG_NOT         
%token LOG_XOR         
%token LOG_IMPLIES     

%token BIT_AND         
%token BIT_OR          
%token BIT_NOT         
%token BIT_XOR         
%token BIT_IMPLIES     

%token IDENTIFIER      
%token NUMERIC_LITERAL 
%token STRING_LITERAL 

%token <var> STMT 

%type <node> prog

%error-verbose

%%

prog: STMT { int x = 0; }

%%

int yywrap() {
	return 1;
}

void yyerror(const char* str) {
	fprintf(stderr, "COMPILER ERROR: '%s'.\n", str);
}

int main(int argc, char **argv) {

	FILE* orig_stdin = stdin;
	stdin = fopen(argv[1], "r");

	int token;
	do {
		token = yylex( );
		
		printf("%d\n", token);

  	} while(token != 0);
	

	fclose(stdin);
	stdin = orig_stdin;
	return 0;
}