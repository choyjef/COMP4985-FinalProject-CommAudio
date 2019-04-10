#pragma once

#include <iostream>

using namespace std;

struct node
{
	char *data;
	int size;
	node *next;
};

class linked_list
{
private:
	node *head, *tail;

public:

	linked_list() {
		head = NULL;
		tail = NULL;
	}

	void add_node(char *d, int size) {
		node *tmp = new node;
		tmp->size = size;
		tmp->data = (char*)malloc(size);
		memcpy(tmp->data, d, size);
		tmp->next = NULL;

		if (head == NULL) {
			head = tmp;
			tail = tmp;
		}
		else {
			tail->next = tmp;
			tail = tail->next;
		}

	}

	node* get_next() {
		node *tmp = head;
		if (head == NULL) {
			printf("Queue is empty\n");
			return NULL;
		}

		if (head == tail) {
			head = tail = NULL;
		}
		else {
			head = head->next;
		}

		return tmp;
	}

	static void free_node(node *n) {
		free(n->data);
		delete n;
	}

	void print() {
		node * tmp = head;
		while (tmp != NULL) {
			printf("%s", tmp->data);
			tmp = tmp->next;
		}
	}
};