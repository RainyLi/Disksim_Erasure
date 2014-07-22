/*
 * disksim_arm.h
 *
 *  Created on: Jul 19, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_ARM_H_
#define DISKSIM_ARM_H_

#include "disksim_req.h"
#include "disksim_erasure.h"

#define ARM_NORMAL	0
#define ARM_STATIC	1
#define ARM_DO_MAX	2
#define ARM_DO_MIX	3
#define ARM_DO_SUM	4
#define ARM_DO_STD	5

typedef void(*arm_complete_t)(double time);
typedef void(*arm_internal_t)(double time, void *ctx);

typedef struct arm_struct {
	int method;
	int threads;
	int max_stripes;

	metadata_t *meta;
	arm_internal_t internal_fn;
	arm_complete_t complete_fn;

	double lastreq;
	double delay;
	int patterns;
	int progress;  // handled stripes
	int completed; // completed stripes

	int *cache_c; // a counter
	int **cache;  // store best results

	int *map, *distr; // temporary arrays
} arm_t;

void arm_init(arm_t *arm, int method, int threads, int max_sectors, double delay, int patterns,
		metadata_t *meta, arm_internal_t internal, arm_complete_t complete);

void arm_run(double time, arm_t *arm);

const char* arm_get_method_name(int method);

void arm_internal_event(double time, arm_t *arm, void* ctx);

#endif /* DISKSIM_ARM_H_ */
