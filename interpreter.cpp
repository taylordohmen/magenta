#include <unordered_map>
#include <iterator>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include "interpreter.h"
#include "parser.tab.h"
#include "tree.h"
#include "mg_obj.h"
#include "mg_int.h"
#include "mg_flt.h"
#include "mg_str.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

//mg_objs need to be cast to their appropriate subclasses to access value
//we need to use pointers for this.
// e.g. map<string, mg_obj *> 
// 		mg_obj * o;
// 		if (mg_obj->type == TYPE_INTEGER) {
// 			int value = ((mg_int *)o)->value;
// 		}
// you can't access value from mg_obj because it doesn't have that field
// you can't directly cast mg_obj to any of its subclasses, but you can cast pointers

std::unordered_map<string, mg_obj *> vars;

// returns whether id is an initialized variable name
bool declared(string id) {
	std::unordered_map<string, mg_obj *>
		::const_iterator iter = vars.find(id);
	return iter != vars.end();
}


// given mg_obj pointer o this function will
// cast the pointer to a pointer of the appropriate subclass and
// return the boolean value of that pointer's actual value
bool eval_bool(mg_obj * o) {
	switch(o->type) {
		case TYPE_STRING:
			return ((mg_str *) o)->value != "";
		case TYPE_INTEGER:
			return  ((mg_int *) o)->value != 0;
		case TYPE_FLOAT:
			return  ((mg_flt *) o)->value != 0.0;
		default: // for non-primitive types XXX this may change later
			return false;
	}
}

// returns the boolean evaluation of a comparison between the values of
// two mg_objs
// Throws error if str is compared to non-str
bool eval_comp(mg_obj * left, int op, mg_obj * right) {
	if ( (left->type == TYPE_STRING && left->type != right->type)
		|| (right->type == TYPE_STRING && left->type != right->type)) {
		
		cerr << "Bad comparison: str val against non-str val" << endl;
		exit(EXIT_FAILURE);
	}
	
	bool numeric = left->type != TYPE_STRING && right->type != TYPE_STRING;
	double lfloat, rfloat;
	string lstr, rstr;
	
	if (numeric) {
		lfloat = (left->type == TYPE_INTEGER) ? 
			(double) ((mg_int *)left)->value : ((mg_flt *)left)->value;
		rfloat = (right->type == TYPE_INTEGER) ? 
			(double) ((mg_int *)right)->value : ((mg_flt *)right)->value;
		switch (op) {
			case LESS_THAN: return lfloat < rfloat;
			case LESS_EQUAL: return lfloat <= rfloat;
			case EQUAL: return lfloat == rfloat;
			case NOT_EQUAL: return lfloat != rfloat;
			case GREATER_EQUAL: return lfloat >= rfloat;
			case GREATER_THAN: return lfloat > rfloat;
		}
	} else {
		lstr = ((mg_str *)left)->value;
		rstr = ((mg_str *)right)->value;
		switch (op) {
			case LESS_THAN: return lstr < rstr;
			case LESS_EQUAL: return lstr <= rstr;
			case EQUAL: return lstr == rstr;
			case NOT_EQUAL: return lstr != rstr;
			case GREATER_EQUAL: return lstr >= rstr;
			case GREATER_THAN: return lstr > rstr;
		}
	}
	
}

// if reps > 0 return value == s repeated reps times
// 	"abc" * 3 == "abcabcabc"
// if reps < 0 return value == reversed s repeated reps times
// 	"abc" * -3 == "cbacbacba""
// if reps == 0 return value == ""
string str_multiply(string s, int reps) {
	int len = s.length();
	int new_len = len * reps;
	if (reps > 0) {
		char temp[new_len];
		for (int i = 0; i < new_len; i++) {
			temp[i] = s[i % len];
		}
		return string(temp, new_len);
	} else if (reps < 0) {
		new_len = new_len * (-1);
		char temp[new_len];
		for (int i = new_len - 1; i > -1; i--) {
			temp[new_len - i - 1] = s[i % len];
		}
		return string(temp, new_len);
	}
	return "";
}


mg_obj * multiply(mg_obj * left, mg_obj * right) {
	int i_product;
	double d_product;
	int repeats;
	string text;
	
	if (left->type == TYPE_INTEGER && right->type == TYPE_INTEGER) {
		i_product = ((mg_int *)left)->value * ((mg_int *)right)->value;
		return new mg_int(&i_product);
	} else if (left->type == TYPE_INTEGER && right->type == TYPE_FLOAT) {
		d_product = ((mg_int *)left)->value * ((mg_flt *)right)->value;
		return new mg_flt(&d_product);
	} else if (left->type == TYPE_FLOAT && right->type == TYPE_INTEGER) {
		d_product = ((mg_int *)left)->value * ((mg_int *)right)->value;
		return new mg_flt(&d_product);
	} else if (left->type == TYPE_FLOAT && right->type == TYPE_FLOAT) {
		d_product = ((mg_int *)left)->value * ((mg_flt *)right)->value;
		return new mg_flt(&d_product);
	} else if (left->type == TYPE_INTEGER && right->type == TYPE_STRING) {
		repeats = ((mg_int *)left)->value;
		text = ((mg_str *)right)->value;
		string str_product = str_multiply(text, repeats);
		return new mg_str(&str_product);
	} else if (left->type == TYPE_STRING && right->type == TYPE_INTEGER) {
		repeats = ((mg_int *)right)->value;
		text = ((mg_str *)left)->value;
		string str_product = str_multiply(text, repeats);
		return new mg_str(&str_product);
	}
}

mg_obj * eval_expr(struct node * node) {
	bool t_val;
	mg_obj * left;
	mg_obj * right;
	switch(node->token) {
		case IDENTIFIER:
			return vars[std::string((char *) node->value)];
		case INTEGER_LITERAL:
			return new mg_int(node->value);
		case FLOAT_LITERAL:
			return new mg_flt(node->value);
		case STRING_LITERAL:
			return new mg_str(node->value);
		
		case PAREN_OPEN:
			return eval_expr(node->children[0]);
		
		case LOG_NOT:
			right = eval_expr(node->children[0]);
			t_val = !eval_bool(right);
			return new mg_int(&t_val); 
		case LOG_OR:
			left = eval_expr(node->children[0]);
			right = eval_expr(node->children[1]);
			t_val = eval_bool(left) || eval_bool(right);
			return new mg_int(&t_val);
		case LOG_XOR:
			left = eval_expr(node->children[0]);
			right = eval_expr(node->children[1]);
			t_val = (eval_bool(left) || eval_bool(right))
				&& !(eval_bool(left) && eval_bool(right));
			return new mg_int(&t_val);
		case LOG_AND:
			left = eval_expr(node->children[0]);
			right = eval_expr(node->children[1]);
			t_val = eval_bool(left) && eval_bool(right);
			return new mg_int(&t_val);
		
		case LESS_THAN:
		case LESS_EQUAL:
		case EQUAL:
		case NOT_EQUAL:
		case GREATER_THAN:
		case GREATER_EQUAL:
			// use token passed in to determine the operation in eval_comp()
			t_val = eval_comp(
				eval_expr(node->children[0]),
				node->token,
				eval_expr(node->children[1])
			);
			return new mg_int(&t_val);
		
		case TIMES:
			return multiply(
				eval_expr(node->children[0]),
				eval_expr(node->children[1])
			);
		
		case DIVIDE:
			// return divide(
			// 	eval_expr(node->children[0]),
			// 	eval_expr(node->children[1])
			// );
			break;
		
	}
	
}

// given a node n this function will do one of the following:
// 1 initialize a new variable and assign it a value
// 2 reassign a initialized variable
// 3 throw a type mismatch error
// 4 throw a multiple initialization error
void assign(struct node * n) {
	//assignment 
	if (n->num_children == 3) {
		string id = string((char *)n->children[1]->value);
		int type = n->children[0]->token;
		mg_obj * value = eval_expr(n->children[2]);
		if (!declared(id)) {
			if (type != value->type) {
				cerr << "ERROR: TYPE MISMATCH" << endl;
				exit(EXIT_FAILURE);
			}
			vars[id] = value;	
		} else {
			cerr << "ERROR: IDENTIFIER CANNOT BE INITIALIZED ";
			cerr << "MORE THAN ONCE." << endl;
			exit(EXIT_FAILURE);
		}
	} else {
		// reassignment
		string id = string((char *)n->children[0]->value);
		mg_obj * value = eval_expr(n->children[1]);
		int type = vars[id]->type;
		if (type != value->type) {
			cerr << "ERROR: TYPE MISMATCH" << endl;
			exit(EXIT_FAILURE);
		}
		vars[id] = value;
	}
}

void eval_stmt(struct node * node) {

	switch (node->token) {
		case ASSIGN:
			assign(node);
			break;
		case WHILE_LOOP:
			break;
		case IF:
			break;
		case STATEMENT:
			for (int i = 0; i < node->num_children; i++) {
				eval_stmt(node->children[i]);
			}
			break;
		case PRINT:
			break;

	}
}
