/*
 * disksim_iostat.c
 *
 *  Created on: Mar 27, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disksim_iostat.h"

static const int P = 65536;
static hashnode *htable;
static int items_in_table = 0;
static double peak = 0;

void hashtable_init() {
	htable = malloc(sizeof(hashnode) * P);
	memset(htable, -1, sizeof(hashnode) * P);
}

double hashtable_get(int key) {
	int pos = key & 0xFFFF, i = 32;
	while (htable[pos].key != key)
		pos = (pos + (++i)) % P;
	return htable[pos].value;
}

void hashtable_set(int key, double value) {
	int pos = key & 0xFFFF, i = 32;
	while (htable[pos].key != -1)
		pos = (pos + (++i)) & 0xFFFF;
	htable[pos].key = key;
	htable[pos].value = value;
}

void hashtable_remove(int key) {
	int pos = key % P, i = 32;
	while (htable[pos].key != key)
		pos = (pos + (++i)) & 0xFFFF;
	htable[pos].key = -1;
}

static int numreqs, numdisks;
static double total_response_time;
static int *iocount;
static statnode *space = NULL;
static statnode *head = NULL, *tail = NULL;
static double currtime = 0;
static long long totalblks = 0;

void iostat_initialize(int disks) {
	hashtable_init();
	numreqs = 0;
	numdisks = disks;
	total_response_time = 0;
	iocount = malloc(sizeof(int) * disks);
	memset(iocount, 0, sizeof(int) * disks);
}

void iostat_print() {
	printf("Average Response Time = %f ms\n", total_response_time / numreqs);
	printf("Throughput = %f MB/s\n", totalblks / (2.048 * currtime));
	printf("Peak Throughput = %f MB/s\n", peak);
}

void iostat_ioreq_start(double time, ioreq *req) {
	if (req->stat) {
		hashtable_set(req->reqno, time);
		items_in_table += 1;
		if (items_in_table + items_in_table > P) {
			fprintf(stderr, "Stop simulation because of saturation.\n");
			exit(0);
		}
	}
}

void iostat_ioreq_complete(double time, ioreq *req) {
	if (req->stat) {
		double start = hashtable_get(req->reqno);
		hashtable_remove(req->reqno);
		total_response_time += (time - start);
		numreqs += 1;
		items_in_table -= 1;
	}
}

static statnode* get_stat_node() {
	if (space == NULL) {
		int i;
		space = malloc(sizeof(statnode) * 16);
		for (i = 0; i < 15; i++)
			space[i].next = space + i + 1;
		space[15].next = NULL;
	}
	statnode *ret = space;
	space = space->next;
	return ret;
}

static void free_stat_node(statnode *node) {
	node->next = space;
	space = node;
}

statnode *iostat_create_node(double time, int devno, int bcount) {
	statnode *node = get_stat_node();
	node->time = time;
	node->devno = devno;
	node->bcount = bcount;
	return node;
}

void iostat_add(statnode *node) {
	if (tail == NULL) {
		head = node;
		tail = node;
	} else {
		tail->next = node;
		node->next = NULL;
		tail = node;
	}
	iocount[node->devno] += node->bcount;
	currtime = node->time;
	totalblks += node->bcount;
}

static void iostat_reset(double time) {
	statnode *temp;
	while (head != NULL && head->time < time) {
		temp = head->next;
		iocount[head->devno] -= head->bcount;
		free_stat_node(head);
		head = temp;
	}
	if (head == NULL)
		tail = NULL;
}

void iostat_distribution(double currtime, double interval, int *distr) {
	iostat_reset(currtime - interval);
	memcpy(distr, iocount, sizeof(int) * numdisks);
}

void iostat_detect_peak(double currtime, double interval) {
	int i, total = 0;
	iostat_reset(currtime - interval);
	for (i = 0; i < numdisks; i++)
		total += iocount[i];
	double curr = total / (2.048 * interval);
	if (curr > peak) peak = curr;
	//printf("%8.0f: %f MB/s\n", resetpoint, curr);
	//fflush(stdout);
}

int iostat_get_num_in_table() {
	return items_in_table;
}

