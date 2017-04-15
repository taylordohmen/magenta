#include <unordered_map>
#include <iterator>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <vector>
#include "interpreter.h"
#include "parser.tab.h"
#include "tree.h"
#include "mg_types.h"
#include "mg_ops.h"
#include "mg_error.h"
#include "except.h"

/* roughly tracks what line the interpreter is on */
extern unsigned linecount;

using std::string;
using std::to_string;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::unordered_map;

void eval_stmt(struct node * node);
mg_obj * eval_expr(struct node * node);
mg_obj * lookup(string id);

/* the stack of different scopes, with scope[0] being global */
vector<unordered_map<string, mg_obj *> > scope(1);

/* a "pointer" to the current stack frame in the `scope` vector */
unsigned current_scope = 0;

/* a constant "pointer" to the global scope */
const unsigned GLOBAL = 0;

/* This contains the output of the current mg_func call. */
mg_obj * return_address;

// mapping of custom type names to interger flags
unordered_map<string, int> custom_types;

// the next available integer to use as a custom type flag
int next_type = 1024;

bool is_type(string type_name) {
	auto iter = custom_types.find(type_name);
	return iter != custom_types.end();
}

/* clear out the global scope before we exit to prevent leaks (called in
	parser.y before program exit)
*/
void cleanup() {
	for (auto it = scope[GLOBAL].begin(); it != scope[GLOBAL].end(); ++it) {
		delete scope[GLOBAL][it->first];
	}
	scope[GLOBAL].clear();
}

// detect identifier `id` within the current stack frame
bool declared(string id) {
	auto iter = scope[current_scope].find(id);
	return iter != scope[current_scope].end();
}

// view all variables in the provided scope
void view_map(unsigned stack_frame) {
	for (
		auto it = scope[stack_frame].begin();
		it != scope[stack_frame].end();
		++it
	) {
		cout << " " << it->first << " : " << *it->second;
	}
	cout << endl;
}

// given the first argument node in an argument chain,
// returns vector<mg_obj *> which are the evaluated arguments
vector<mg_obj *> * eval_args(struct node * n) {

	vector<mg_obj *> * args = new vector<mg_obj *>();
	do {
		mg_obj * arg = eval_expr(n->children[0]);
		args->push_back(arg);
		n = n->num_children == 2 ? n->children[1] : NULL;
	} while (n != NULL);
	return args;
}

// returns whether an the structure of a given type_def and anonymouse object match
bool structure_matches(mg_type * type, vector<mg_obj *> * object) {
	if (type->field_names.size() != object->size()) {
		return false;
	}
	for (int i = 0; i < object->size(); i++) {
		if (type->field_types[i] != object->at(i)->type) {
			return false;
		}
	}
	return true;
}

void assign_type(struct node * n) {
	string id;
	int type;
	vector<mg_obj *> * args;
	mg_type * type_def;
	if (n->num_children == 3) {
		string type_name = string((char*)n->children[0]->value);
		type = custom_types[type_name];
		id = string((char*)n->children[1]->value);
		if (!is_type(type_name)) {
			error("Unknown type declartion", linecount);
		}
		type_def = (mg_type*)scope[current_scope][type_name];
		args = eval_args(n->children[2]->children[0]);
	} else {
		id = string((char*)n->children[0]);
		if (declared(id)) {
			mg_instance * value = (mg_instance* )scope[current_scope][id];

			if (value->magenta_type != type) {
				error("type mismatch on assignment", linecount);
			}
		} else {
			error("uninitialized indentifier '" + id + "'", linecount);
		}
	}

	if (structure_matches(type_def, args)) {
		scope[current_scope][id] = new mg_instance(type_def, args);
	} else {
		error("value does not match type structure", linecount);
	}
}

// given a node n this function will do one of the following:
// 1 initialize a new variable and assign it a value
// 2 reassign a initialized variable
// 3 throw a type mismatch error
// 4 throw a multiple initialization error
void assign(struct node * n) {

	if ( n->children[0]->token == IDENTIFIER) {
		assign_type(n);
		return;
	}
	//assignment 
	if (n->num_children == 3) {
		string id = string((char *)n->children[1]->value);
		int type = n->children[0]->token;
		mg_obj * value = eval_expr(n->children[2]);
		if (type == TYPE_FLOAT && value->type == TYPE_INTEGER) {
			mg_flt * temp = new mg_flt(
				(double)((mg_int *)value)->value
			);
			delete value;
			value = (mg_obj *) temp;
		} else if (type != value->type) {
			error("type mismatch", linecount);
		}
		scope[current_scope][id] = value;
	} else {
		// reassignment
		string id = string((char *)n->children[0]->value);
		if (!declared(id)) {
			error("uninitialized identifier`" + id + "'", linecount);
		}
		
		mg_obj * value = eval_expr(n->children[1]);
		int type = scope[current_scope][id]->type;
		if (type == TYPE_FLOAT && value->type == TYPE_INTEGER) {
			mg_flt * temp = new mg_flt(
				(double)((mg_int *)value)->value
			);
			delete value;
			
			value = (mg_obj *) temp;
		} else if (type == TYPE_INTEGER && value->type == TYPE_FLOAT) {
			mg_int * temp = new mg_int(
				(int)((mg_flt *)value)->value
			);
			delete value;
			value = (mg_obj *) temp;
	
		} else if (type != value->type) {
			error("type mismatch", linecount);
		}
		delete scope[current_scope][id];
		scope[current_scope][id] = value;
	}
}


mg_obj * eval_index(mg_obj * left, mg_obj * right) {
	// TODO: amend this later when we support vector types
	if (!(left->type == TYPE_STRING && right->type == TYPE_INTEGER)) {
		error("unsupported index operation", linecount);
	}
	
	string text = ((mg_str *)left)->value;
	int length = text.length();
	string out;
	int position = ((mg_int *)right)->value;
	
	if (abs(position) >= length) {
		error("index out of bounds", linecount);
	}
	
	if (position < 0) {
		out = text[length + position];
	} else {
		out = text[position];
	}
	return new mg_str(out);
}

mg_obj * eval_math(mg_obj * left, int token, mg_obj * right) {
	mg_obj * out;
	switch (token) {
		case TIMES:
			out = multiply(left, right);
			break;
		case DIVIDE:
			out = divide(left, right);
			break;
		case INT_DIVIDE:
			out = int_divide(left, right);
			break;
		case MODULO:
			out = mod(left, right);
			break;
		case PLUS:
			out = add(left, right);
			break;
		case POWER:
			out = power(left, right);
			break;
		case LOG:
			out = logarithm(left, right);
			break;
	}
	return out;
}

mg_obj * eval_func(struct node * node) {
	string id = string((char *)node->children[0]->value);
	mg_func * variable = (mg_func *)lookup(id);
	if (!variable) {
		error(
			"no such function `" + id + "' in scope "
			+ to_string(current_scope),
			linecount
		);
	}
	
	// evaluate arguments
	vector<mg_obj *> * args;
	if (node->num_children > 1) {
		args = eval_args(node->children[1]);
	}
	
	// add arguments to local map
	mg_func * f = variable;
	current_scope++;
	if (current_scope >= scope.size()) {
		unordered_map<string, mg_obj *> u;
		scope.push_back(u);
	}
	if (variable->param_types.size() == args->size()) {
		for (int i = 0; i < args->size(); i++) {
			if (variable->param_types[i] == args->at(i)->type) {
				scope[current_scope][(variable->param_names)[i]] = args->at(i);
			} else {
				error("wrong arg type for func `" + id + "'", linecount);
			}
		}
		try {
			eval_stmt(variable->value);
		} catch (return_except &e) {
			if (return_address->type != f->return_type) {
				error("wrong return type in func `" + id + "'", linecount);
			}
			return return_address;
		}
		error("func " + id + " must return a value", linecount);
	} else {
		error("invalid arg count for func `" + id + "'", linecount);
	}
}

mg_obj * lookup(string id) {
	auto local_iter = scope[current_scope].find(id);
	auto global_iter = scope[GLOBAL].find(id);
	
	if (local_iter != scope[current_scope].end()) {
		return scope[current_scope][id];
	}
	if (global_iter != scope[GLOBAL].end()) {
		return scope[GLOBAL][id];
	}
}

mg_obj * get_value(string id) {
	mg_obj * variable = lookup(id);
			
	if (!variable) {
		error("var `" + id + "' not in local or global scope", linecount);
	}
	
	int t = variable->type;
	mg_obj * result;
	switch (t) {
		case TYPE_INTEGER:
			result = (mg_obj *) new mg_int(
				((mg_int *)variable)->value
			);
			break;
		case TYPE_FLOAT:
			result = (mg_obj *) new mg_flt(
				((mg_flt *)variable)->value
			);
			break;
		case TYPE_STRING:
			result = (mg_obj *) new mg_str(
				((mg_str *)variable)->value
			);
			break;
		case TYPE_FUNCTION:
			eval_stmt(((mg_func *)variable)->value);
			result = return_address;
			break;
		case TYPE_TYPE:
			result = (mg_obj *) variable;
			break;
		case INSTANCE:
			result = (mg_obj *) variable;
			break;
	}
	return result;
}

void func_cleanup() {
	for (
		auto it = scope[current_scope].begin();
		it != scope[current_scope].end();
		++it
	) {
		delete scope[current_scope][it->first];
	}
	scope[current_scope--].clear();
	scope.pop_back();
}

bool has_field(mg_instance * object, string field_name) {
	auto iter = object->fields.find(field_name);
	return iter != object->fields.end();
}

mg_obj * eval_access(mg_obj * object, string field_name) {
	cout << object->type << endl;
	cout << INSTANCE << endl;
	cout << TYPE_TYPE << endl;
	if (object->type != INSTANCE) {
		error("access of non-instance type", linecount);
	}
	mg_instance * instance = ((mg_instance *)object);
	if (!has_field(instance, field_name)) {
		error("field" + field_name + " not found in object", linecount);
	}
	return instance->fields[field_name];
}

mg_obj * eval_expr(struct node * node) {
	bool t_val;
	mg_obj * left = NULL;
	mg_obj * right = NULL;
	mg_obj * result = NULL;
	string id;
	int token = node->token;
	int t;
	switch(token) {
		case IDENTIFIER: {
			id = string((char *) node->value);
			result = get_value(id);
		} break;
		case INTEGER_LITERAL:
			result = new mg_int(*(int *)node->value);
			break;
		case FLOAT_LITERAL:
			result = new mg_flt(*(double *)node->value);
			break;
		case STRING_LITERAL:
			result = new mg_str((char *)node->value);
			break;
		case FUNC_CALL: {
			result = eval_func(node);
			func_cleanup();
		} break;
		case INPUT: {
			string buffer;
			getline(cin, buffer);
			result = new mg_str(buffer);
		} break;
		case PAREN_OPEN:
			return eval_expr(node->children[0]);
		case BRACKET_OPEN:
			left = eval_expr(node->children[0]);
			right = eval_expr(node->children[1]);
			result = eval_index(left, right);
			break;
		case ACCESS:
			left = eval_expr(node->children[0]);
			id = string((char *)(node->children[1]->value));
			result = eval_access(left, id);
			break;	
		case LOG_NOT:
			right = eval_expr(node->children[0]);
			t_val = !eval_bool(right);
			result = new mg_int(t_val);
			break;
		case LOG_OR:
		case LOG_XOR:
		case LOG_AND:
		case LOG_IMPLIES:
			left = eval_expr(node->children[0]);
			right = eval_expr(node->children[1]);
			result = eval_logical(left, token, right);
			break;
		case BIT_NOT:
		case BIT_AND:
		case BIT_OR:
		case BIT_XOR:
		case LEFT_SHIFT:
		case RIGHT_SHIFT:
			if (token == BIT_NOT) {
				left = NULL;
				right = eval_expr(node->children[0]);
			} else {
				left = eval_expr(node->children[0]);
				right = eval_expr(node->children[1]);
			}
			result = eval_bitwise(left, token, right);
			break;
		case LESS_THAN:
		case LESS_EQUAL:
		case EQUAL:
		case NOT_EQUAL:
		case GREATER_THAN:
		case GREATER_EQUAL:
			// use token passed in to determine the operation in eval_comp()
			left = eval_expr(node->children[0]);
			right = eval_expr(node->children[1]);
			result = eval_comp(left, token, right);
			break;
		case TIMES:
		case DIVIDE:
		case INT_DIVIDE:
		case MODULO:
		case PLUS:
		case LOG:
		case POWER:
			left = eval_expr(node->children[0]);
			right = eval_expr(node->children[1]);
			result = eval_math(left, token, right);
			break;
		case MINUS:
			// unary minus
			if (node->num_children == 1) {
				left = NULL;
				right = eval_expr(node->children[0]);
			} else {
				left = eval_expr(node->children[0]);
				right = eval_expr(node->children[1]);
			}
			result = subtract(left, right);
			break;
		case LEN: // a built-in called like a function, handled like an op
			left = eval_expr(node->children[0]);
			right = NULL;
			if (left->type != TYPE_STRING) {
				result = new mg_int(-1);
			} else {
				result = new mg_int(((mg_str *)left)->value.length());
			}
			break;
		case QUESTION: {
			int nodes = node->num_children;
			mg_obj * test;
			if (nodes == 3) {
				test = eval_expr(node->children[0]);
				left = eval_expr(node->children[1]);
				right = eval_expr(node->children[2]);
				result = eval_bool(test) ? left : right;
				delete test;
			} else {
				left = eval_expr(node->children[0]);
				right = eval_expr(node->children[1]);
				result = eval_bool(left) ? left : right;
			}
		}
		// handle deletion and return separately from binary operators
		delete (mg_obj *) (result == left ? right : left);
		return result;
	}
	// avoid double-delete error when contending with `ident .op. ident`
	if (left == right) {
		delete left;
	} else {
		delete left;
		delete right;
	} 
	return result;
}

// given a CASE node, it's option mg_obj and option type this function will
// evaluate whether option == case and if so will execute the statements
// within the scope of that case
// returns false if a break statement is reached
// returns true otherwise
// raises a type mismatch error if option and case don't match
bool eval_case(struct node * n, mg_obj * option, int option_type) {
	mg_obj * c = eval_expr(n->children[0]);
	if (c->type != option_type) {
		error("case type and option type do not match", linecount);
	}
	
	mg_int * i_option, * i_c;
	mg_flt * d_option, * d_c;
	mg_str * s_option, * s_c;
	bool match = false;
	
	switch (option_type) {
		case TYPE_INTEGER:
			i_option = (mg_int *) option;
			i_c = (mg_int *) c;
			if (i_c->value == i_option->value) {
				match = true;
			}
			break;
		case TYPE_FLOAT:
			d_option = (mg_flt *) option;
			d_c = (mg_flt *) c;
			if (d_c->value == d_option->value) {
				match = true;
			}
			break;
		case TYPE_STRING:
			s_option = (mg_str *) option;
			s_c = (mg_str *) c;
			if (s_c->value == s_option->value) {
				match = true;
			}
			break;
		default:
			error("Invalid type used in option block", linecount);
	}
	delete c;
	if (match) {
		struct node * stmts = n->children[1];
			try {
				eval_stmt(stmts);
			} catch (break_except &e) {
				return false;
			}
	}
	return true;
}

// given an OPTION node this function will
// evaluate all case statements within it's scope
// until a break statement is reached
// or until the cases are exhausted
void eval_option(struct node * n) {
	mg_obj * option = eval_expr(n->children[0]);
	struct node * current = n->children[1];
	struct node * previous = NULL;
	bool unbroken;
	do {
		unbroken = eval_case(current, option, option->type);
		previous = current;
		if (current->num_children == 3) {
			current = current->children[2];
		}
	} while (unbroken && current != previous);
	delete option;
}

void eval_stmt(struct node * node) {
	view_map(current_scope);
	mg_obj * temp;
	int children = node->num_children;
	bool next = false;
	switch (node->token) {
		case ASSIGN:
			assign(node);
			break;
		
		case TYPE_TYPE: {
			string type_name = string((char *)node->children[0]->value);
			custom_types[type_name] = next_type;
			scope[current_scope][type_name] = new mg_type(node, next_type);
			next_type++;
		} break;

		case TYPE_FUNCTION: { // Function definition
			string id = string((char *)node->children[0]->value);
			temp = new mg_func(node);
			scope[current_scope][id] = temp;
		} break;
		
		case FUNC_CALL: {
			temp = eval_func(node);
			func_cleanup();
		} break;

		case RETURN: {
			return_address = eval_expr(node->children[0]);
			throw return_except();
		} break;
		
		case WHILE_LOOP: {
			temp = eval_expr(node->children[0]);
			while (eval_bool(temp)) {
				try {
					if (next) {
						next = false;
						continue;
					}
					eval_stmt(node->children[1]);
				} catch (break_except &e) {
					// clean up on interrupted control flow
					delete temp;
					break;
				} catch (next_except &e) {
					next = true;
				}
				delete temp;
				temp = eval_expr(node->children[0]);
			}
			// reclaim temp if loop exited at top of loop
			delete temp;
		} break;
		
		case FOR_LOOP: {
			int from = 0, to = INT_MAX, by = 1, ch_token = 0, bad = 0;
			mg_int * ptr;
			string iter_name;
			struct node * child;
			// handle variable arrangements of by/from/to clauses,
			// detect name of for-loop iterator variable
			for (int x = 0; x < children - 1; x++) {
				child = node->children[x];
				ch_token = child->token;
				if (ch_token == IDENTIFIER) {
					iter_name = string((char *) child->value);
					scope[current_scope][iter_name] = new mg_int(0);
				} else {
					ptr = ((mg_int *)eval_expr(child->children[0]));
					(ch_token == FROM ? from
						: ch_token == TO ? to
						: ch_token == BY ? by
						: bad
					) = ptr->value;
					delete ptr;
				}
			}
			if (bad) {
				error("invalid for loop", linecount);
			}	
			
			// a "do what I mean" feature; looping from x to y where x > y
			// with a negative number will work the same as a positive one
			// (i.e. no going for ages by looping around past the overflow
			// point
			by = abs(by);
			
			// initialize the iterator variable of the for-loop
			((mg_int *)scope[current_scope][iter_name])->value = from;
			
			// iterate upwards, from `from` to `to`-1 by `by` increments
			if (from < to) {
				for(int i = from; i < to; i += by) {
					((mg_int *)scope[current_scope][iter_name])->value = i;
					try {
						if (next) {
							next = false;
							continue;
						}
						eval_stmt(node->children[children - 1]);
					} catch (break_except &e) {
						break;
					} catch (next_except &e) {
						next = true;
					}
				}
			// iterate backwards if `to` < `from`
			} else if (from > to) {
				for (int i = from; i > to; i -= by) {
					((mg_int *)scope[current_scope][iter_name])->value = i;
					try {
						if (next) {
							next = false;
							continue;
						}
						eval_stmt(node->children[children - 1]);
					} catch (break_except &e) {
						break;
					} catch (next_except &e) {
						next = true;
					}
				}
			}
			// remove temporary loop variable from scope
			delete scope[current_scope][iter_name];
			scope[current_scope].erase(iter_name);
		} break;
		
		case IF:
			// evaluate IF-expr and differentiate between IF and IF-ELSE
			// each block must delete the temporary object,
			// since it is possible that eval_stmt() will lead us away
			// from the current path of execution.
			temp = eval_expr(node->children[0]);
			if (eval_bool(temp)) {
				delete temp;
				eval_stmt(node->children[1]);
			} else if (children == 3) {
				delete temp;
				eval_stmt(node->children[2]);
			} else {
				delete temp;
			}
			break;
		case OPTION:
			eval_option(node);
			break;
		case STATEMENT:
			for (int i = 0; i < children; i++) {
				eval_stmt(node->children[i]);
			}
			break;
		case PRINT:
			if (!node->num_children) {
				cout << endl;
				break;
			}
			temp = eval_expr(node->children[0]);
			switch (temp->type) {
				// operator<< is overloaded in mg_types.cpp
				case TYPE_INTEGER:
					cout << *(mg_int *)temp << endl;
					break;
				case TYPE_FLOAT:
					cout << *(mg_flt *)temp << endl;
					break;
				case TYPE_STRING:
					cout << *(mg_str *)temp << endl;
					break;
				case TYPE_FUNCTION:
					cout << *(mg_func *)temp << endl;
					break;
			}
			delete temp;
			break;
		case BREAK:
			throw break_except();
		case NEXT:
			throw next_except();
	}
}
