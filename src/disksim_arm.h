/*
 * disksim_arm.h
 *
 *  Created on: Apr 19, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_ARM_H_
#define DISKSIM_ARM_H_

#include "disksim_erasure.h"

#define STRATEGY_ROW		0
#define STRATEGY_DIAG		1
#define STRATEGY_OPTIMAL	2
#define STRATEGY_MIN_STD	3
#define STRATEGY_MIN_L		4
#define STRATEGY_MIN_L2		5
#define STRATEGY_RANDOM		6
#define STRATEGY_MIN_DOT	7
#define STRATEGY_MIN_MAX	8
#define STRATEGY_MIN_DIFF	9

typedef struct {
	int *distr; // reconstruction distribution
} arm_desc;

typedef struct {
	metadata_t *meta; // the configuration of underlying erasure code

	int *gdistr; // I/O distribution of applications
} arm_struct;

const char* arm_method_name(int method);

double arm_score(int *a, int *b, int *mask, int disks, int method);

void arm_rebuild(metadata_t *meta, ioreq_t *req, int stripeno, int pattern);

int arm_get_rebuild_distr(metadata_t *meta, int stripeno, int pattern, int *distr);

void arm_preprocessing(metadata_t *meta, int pattcnt);

int arm_select_pattern(metadata_t *meta, int stripeno, int method, int *distr, int coef);

void arm_initialize_patterns_new(metadata_t *meta, int pattcnt, int limit);

#endif /* DISKSIM_ARM_H_ */
