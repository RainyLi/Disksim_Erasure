/*
 * disksim_iostat.c
 *
 *  Created on: Mar 27, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "disksim_iostat.h"

static const int P = 65536;
static hashnode *htable;
static int items_in_table = 0;
static double peak = 0;

void hashtable_init() {
	htable = (hashnode*) malloc(sizeof(hashnode) * P);
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

static int numreqs, numdisks, numwrites;
static double total_response_time;
static int *iocount;
static statnode *space = NULL;
static statnode *head = NULL, *tail = NULL;
static double currtime = 0;
static long long total_blks;
static long long total_XORs;
static long long total_IOs;

void iostat_initialize(int disks) {
	hashtable_init();
	numreqs = 0;
	numwrites = 0;
	numdisks = disks;
	total_response_time = 0;
	total_blks = 0;
	total_XORs = 0;
	total_IOs = 0;
	iocount = malloc(sizeof(int) * disks);
	memset(iocount, 0, sizeof(int) * disks);
}

double iostat_avg_response_time() {
	return total_response_time / numreqs;
}

double iostat_throughput() {
	return total_blks / (2.048 * currtime);
}

double iostat_peak_throughput() {
	return peak;
}

double iostat_avg_xors_per_write() {
	return total_XORs * 1.0 / numwrites;
}

double iostat_avg_IOs_per_request() {
	return total_IOs * 1.0 / numreqs;
}

void iostat_ioreq_start(double time, ioreq *req) {
	if (req->stat) {
		hashtable_set(req->reqno, time);
		items_in_table += 1;
		if (items_in_table + items_in_table > P) {
			fprintf(stderr, "Stop simulation because of saturation.\n");
			printf("items in table %d\n", items_in_table);
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
		numwrites += (req->flag == 0);
		items_in_table -= 1;
		total_XORs += req->numXORs;
		total_IOs += req->numIOs;
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
		node->next = NULL;
	} else {
		tail->next = node;
		node->next = NULL;
		tail = node;
	}
	iocount[node->devno] += node->bcount;
	currtime = node->time;
	total_blks += node->bcount;
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
}
