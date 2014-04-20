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

const char* arm_method_name(int method);

double arm_score(int *a, int *b, int *mask, int disks, int method);

void arm_rebuild(metadata *meta, ioreq *req, int stripeno, int pattern);

int arm_get_rebuild_distr(metadata *meta, int stripeno, int pattern, int *distr);

void arm_initialize_patterns(metadata *meta, int pattcnt);

int arm_select_pattern(metadata *meta, int stripeno, int method, int *distr, int coef);

#endif /* DISKSIM_ARM_H_ */
