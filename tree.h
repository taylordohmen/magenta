#ifndef TREE_H
#define TREE_H

#include "mg_obj.h"
#include <string>

#define MAX_CHILDREN 3

typedef struct node {
	int token;
	mg_obj* value;
	int num_children;
	struct node* children[3];
} node;

node* make_node(int token, mg_obj* value);

void attach(node* parent, node* child);

#endif