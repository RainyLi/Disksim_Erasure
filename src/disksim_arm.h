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

#define ARM_DO_MAX	0
#define ARM_DO_SUM	1
#define ARM_DO_STD	2

typedef void(*arm_complete_t)(double time, ioreq_t *req);

typedef struct arm_struct {
	int method;
	int stripes;
	metadata_t *meta;
	arm_complete_t *comp_fn;

	int *map, *distr;
} arm_t;

void arm_init(arm_t *arm, metadata_t *meta, arm_complete_t *comp);

void arm_run(double time, void *ctx);

void arm_recovery(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sub_ioreq_t *subreq);

void arm_complete(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sub_ioreq_t *subreq);

#endif /* DISKSIM_ARM_H_ */
