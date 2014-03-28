/*
 * disksim_iostat.h
 *
 *  Created on: Mar 27, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_IOSTAT_H_
#define DISKSIM_IOSTAT_H_

#include "disksim_event_queue.h"

typedef struct hash_node_t {
	int key;
	double value;
} hashnode;

typedef struct stat_node_t {
	double time;
	int devno;
	int bcount;
	struct stat_node_t *next;
} statnode;

void   hashtable_init();
double hashtable_get(int key);
void   hashtable_set(int key, double value);
void   hashtable_remove(int key);

void iostat_initialize(int disks);
void iostat_print();
void iostat_ioreq_start(double time, ioreq *req);
void iostat_ioreq_complete(double time, ioreq *req);

/* for throughput */
statnode* iostat_create_node(double time, int devno, int bcount);
void iostat_add(statnode *node);
void iostat_distribution(double currtime, double interval, int *distr);
void iostat_detect_peak(double currtime, double interval);

/* debug */
int iostat_get_num_in_table();

#endif /* DISKSIM_IOSTAT_H_ */
