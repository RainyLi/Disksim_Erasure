/*
 * disksim_set.c
 *
 *  Created on: Feb 27, 2014
 *      Author: zyz915
 */

#include "disksim_set.h"

static node empty;
static node *NIL = &empty;
static node *space = NULL;

static void allocate_space() {
	int i;
	node *tmp = (node*) malloc(sizeof(node) * 128);
	for (i = 0; i < 128; i++) {
		tmp[i].next = space;
		space = &tmp[i];
	}
}

static node* getnewnode() {
	node *ret;
	if (space == NULL)
		allocate_space();
	ret = space;
	space = space->next;
	return ret;
}

node* create_set() {
	NIL->left = NIL;
	NIL->right = NIL;
	return NIL;
}

void free_set(node *curr) {
	if (curr->left != NIL)
		free_set(curr->left);
	if (curr->right != NIL)
		free_set(curr->right);
	if (curr != NIL) {
		curr->next = space;
		space = curr;
	}
}

node* make_list(node *curr, node *tail) {
	if (curr == NIL) return tail;
	curr->next = make_list(curr->right, tail);
	return make_list(curr->left, curr);
}

static void left_rotate(node **root) {
	node *x = *root;
	node *y = x->right;
	x->right = y->left;
	y->left = x;
	*root = y;
}

static void right_rotate(node **root) {
	node *x = *root;
	node *y = x->left;
	x->left = y->right;
	y->right = x;
	*root = y;
}

static void maintain(node **root, int flag) {
	node *curr = *root;
	if (!flag) {
		if (curr->left->left->size > curr->right->size)
			right_rotate(root);
		else {
			if (curr->left->right > curr->right) {
				left_rotate(&(curr->left));
				right_rotate(root);
			} else return;
		}
	} else {
		if (curr->right->right->size > curr->left->size)
			left_rotate(root);
		else {
			if (curr->right->left->size > curr->left->size) {
				right_rotate(&(curr->right));
				left_rotate(root);
			} else return;
		}
	}
	maintain(&(curr->left), 0);
	maintain(&(curr->right), 1);
	maintain(root, 0);
	maintain(root, 1);
}

void insert(node **root, int first, int second) {
	node *curr = *root;
	int cmp;
	if (curr == NIL) {
		curr = getnewnode();
		curr->first = first;
		curr->second = second;
		curr->left = NIL;
		curr->right = NIL;
		curr->size = 1;
		*root = curr;
		return;
	}
	cmp = (first != curr->first ? first < curr->first : second < curr->second);
	if (cmp)
		insert(&(curr->left), first, second);
	else
		insert(&(curr->right), first, second);
	curr->size += 1;
	//maintain(root, !cmp);
}

int contains(node *curr, int first, int second) {
	if (curr == NIL) return 0;
	if (first == curr->first && second == curr->second)
		return 1;
	if (first != curr->first ? first < curr->first : second < curr->second)
		return contains(curr->left, first, second);
	else
		return contains(curr->right, first, second);
}
