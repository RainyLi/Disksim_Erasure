/*
 * disksim_arm.c
 *
 *  Created on: Jul 19, 2014
 *      Author: zyz915
 */

#include <stdlib.h>
#include <string.h>

#include "disksim_arm.h"
#include "disksim_global.h"

extern int sb_idx, sh_idx;

static int arm_calculate(arm_t *arm, int disk, int *ptr, stripe_head_t *sh)
{
	static long long s_sum = 0, s_num = 0;
	metadata_t *meta = arm->meta;
	int pat = *ptr; // pattern bitmap
	int *map = arm->map, *distr = arm->distr;
	int nr_total = meta->w * meta->n;
	int r, c, uid;
	memset(map, 0, sizeof(int) * nr_total);
	memset(distr, 0, sizeof(int) * meta->n);
	for (r = 0; r < meta->w; r++) {
		uid = ID(r, disk);
		if (meta->map_p2[uid][1 & (pat >> r)] == -1) // only consider 0/1
			pat ^= (1 << r);
		element_t *elem = meta->chains[meta->map_p2[uid][1 & (pat >> r)]];
		for (; elem; elem = elem->next)
			if (elem->col != disk)
				map[ID(elem->row, elem->col)] = 1;
	}
	*ptr = pat; // return actual pattern
	for (uid = 0; uid < nr_total; uid++)
		if (map[uid])
			distr[uid % meta->n] += meta->usize;// * (1 - sh->page[uid].state);
	int *cnt = meta->sctlr->count, maxi = 0, sum = 0;
	for (c = 0; c < meta->n; c++) {
		maxi = max(maxi, distr[(c + sh->stripeno) % meta->n] + cnt[c]);
		sum += distr[(c + sh->stripeno) % meta->n];
	}
	s_sum += sum;
	s_num += 1;
	switch (arm->method) {
	case ARM_DO_MAX:
		return maxi;
	case ARM_DO_SUM:
		return sum;
	case ARM_DO_MIX:
		return maxi + (s_sum < sum * s_num ? sum : 0);
	default:
		fprintf(stderr, "invalid method: %d\n", arm->method);
		exit(-1);
	}
	return 0;
}

static void arm_make_request(double time, arm_t *arm)
{
	if (arm->progress < arm->max_stripes) {
		sub_ioreq_t *subreq = (sub_ioreq_t*) disksim_malloc(sb_idx);
		subreq->time = time;
		subreq->reqtype = REQ_TYPE_RECOVERY;
		subreq->state = STATE_BEGIN;
		subreq->stripeno = arm->progress++;
		subreq->meta = (void*) arm;
		subreq->reqctx = NULL;
		if (arm->lastreq <= time) {
			sh_get_active_stripe(time, arm->meta->sctlr, subreq);
			arm->lastreq = time;
		} else
			arm->internal_fn(arm->lastreq, (void*)subreq);
		arm->lastreq += arm->delay;
	}
}

static void arm_complete(double time, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	arm_t *arm = (arm_t*) subreq->meta;
	if (!(--subreq->out_reqs)) {
		if (subreq->state == STATE_WRITE) {
			sh_release_stripe(time, arm->meta->sctlr, subreq->stripeno);
			disksim_free(sb_idx, subreq);
			arm->completed += 1;
			arm->meta->sctlr->rec_prog++;
			if (arm->completed == arm->max_stripes) {
				arm->complete_fn(time);
			} else
				arm_make_request(time, arm);
		} else
			sh_redo_callback(time, arm->meta->sctlr, subreq, sh);
	}
}

static void arm_do_recovery(double time, stripe_head_t *sh, sub_ioreq_t *subreq, int disk, int pattern)
{
	subreq->state += 1;
	subreq->out_reqs = 1;
	arm_t *arm = (arm_t*) subreq->meta;
	metadata_t *meta = arm->meta;
	stripe_ctlr_t *sctlr = meta->sctlr;
	int uid, r, nr_total = meta->w * meta->n, *map = arm->map;
	memset(map, 0, sizeof(int) * nr_total);
	if (subreq->state == STATE_READ) {
		for (r = 0; r < meta->w; r++) {
			int ch = 1 & (pattern >> r);
			uid = ID(r, disk);
			while (meta->map_p2[uid][ch] == -1) --ch;
			element_t *elem = meta->chains[meta->map_p2[uid][ch]];
			for (; elem; elem = elem->next)
				if (elem->col != disk)
					map[ID(elem->row, elem->col)] = 1;
		}
	} else {
		for (r = 0; r < meta->w; r++)
			map[ID(r, disk)] = 1;
	}
	int flag = (subreq->state == STATE_READ ? DISKSIM_READ : DISKSIM_WRITE);
	for (uid = 0; uid < nr_total; uid++)
		if (map[uid]) {
			sh_request_t *shreq = (sh_request_t*) disksim_malloc(sh_idx);
			shreq->time = time;
			shreq->flag = flag;
			shreq->stripeno = subreq->stripeno;
			shreq->devno = uid % meta->n;
			shreq->blkno = uid / meta->n;
			shreq->v_begin = 0;
			shreq->v_end = meta->usize;
			shreq->reqctx = (void*) subreq;
			shreq->meta = (void*) sctlr;
			subreq->out_reqs += 1;
			sh_request_arrive(time, sctlr, sh, shreq);
		}
	arm_complete(time, subreq, sh);
}

static int arm_mdrr(int c, int p)
{
	int r, ret = 0;
	for (r = 0; r < ((p - 3) >> 1); r++)
		ret ^= (r & 1) << r;
	for (r = (p - 3) >> 1; r < p; r++)
		ret ^= (!(r & 1)) << r;
	return ret;
}

static int arm_rdor(int c, int p)
{
	int sp = 0, nsp, i, w;
	for (i = 1; i < p; i++)
		sp = sp | (1 << i * i % p);
	nsp = ((1 << p) - 2) & ~sp;

	if (c == p - 1) {
		w = 0;
	}  else if (c == p) {
		w = (1 << p) - 1;
	} else if ((1 & (sp >> c)) ^ ((p & 3) == 1)) {
		w = ((nsp >> (c + 1)) | nsp << (p - (c + 1)));
	} else {
		w = ((sp >> (c + 1)) | sp << (p - (c + 1)));
	}
	w &= (1 << p) - 1;
	return w;
}

static void arm_recovery(double time, sub_ioreq_t *subreq, stripe_head_t *sh, int fails, int *fd)
{
	arm_t *arm = (arm_t*) subreq->meta;
	int pattern = 0;
	int i, f_disk = 0, mask = (1 << arm->meta->w) - 1;
	while (!fd[f_disk]) ++f_disk;
	int *cache = arm->cache[f_disk], incache = 0;
	int *cnt = arm->meta->sctlr->count;
	if (arm->method == ARM_NORMAL)
		arm_do_recovery(time, sh, subreq, f_disk, pattern);
	else if (arm->method == ARM_STATIC) {
		if (arm->meta->codetype == CODE_RDP)
			pattern = arm_rdor(f_disk, arm->meta->pr);
		if (arm->meta->codetype == CODE_XCODE)
			pattern = arm_mdrr(f_disk, arm->meta->pr);
		arm_do_recovery(time, sh, subreq, f_disk, pattern);
	} else {
		int score = arm_calculate(arm, f_disk, &pattern, sh);
		for (i = 0; i < arm->patterns; i++) {
			int pat = rand() & mask;
			int cur = arm_calculate(arm, f_disk, &pat, sh);
			if (cur < score) { // higher is better
				score = cur;
				pattern = pat;
			}
		}
		for (i = 0; i < arm->patterns; i++) {
			int pat = cache[i];
			if (pat == -1) continue;
			int cur = arm_calculate(arm, f_disk, &pat, sh);
			if (cur < score) { // smaller is better
				score = cur;
				pattern = pat;
			}
			if (pattern == cache[i])
				incache = 1;
		}
		if (!incache)
			cache[(arm->cache_c[f_disk]++) % arm->patterns] = pattern;
		arm_do_recovery(time, sh, subreq, f_disk, pattern);
	}
}

void arm_init(arm_t *arm, int method, int threads, int max_sectors, double delay, int patterns,
		metadata_t *meta, arm_internal_t internal, arm_complete_t complete)
{
	int i;
	arm->method = method;
	arm->threads = threads;
	arm->delay = delay;
	arm->meta = meta;
	arm->patterns = patterns;
	arm->map = (int*) malloc(sizeof(int) * meta->w * meta->n);
	arm->distr = (int*) malloc(sizeof(int) * meta->n);
	arm->cache_c = (int*) malloc(sizeof(int) * meta->n);
	memset(arm->map, 0, sizeof(int) * meta->w * meta->n);
	memset(arm->distr, 0, sizeof(int) * meta->n);
	memset(arm->cache_c, 0, sizeof(int) * meta->n);
	arm->cache = (int**) malloc(sizeof(void*) * meta->n);
	for (i = 0; i < meta->n; i++) {
		arm->cache[i] = (int*) malloc(sizeof(int) * patterns);
		memset(arm->cache[i], -1, sizeof(int) * patterns);
	}
	arm->max_stripes = max_sectors / (meta->w * meta->usize);
	sh_set_rec_callbacks(meta->sctlr, arm_recovery, arm_complete);
	arm->internal_fn = internal;
	arm->complete_fn = complete;
}

void arm_run(double time, arm_t *arm)
{
	arm->progress = 0;
	arm->completed = 0;
	arm->lastreq = 0;
	int i;
	for (i = 0; i < arm->threads; i++)
		arm_make_request(time, arm);
}

const char* arm_get_method_name(int method)
{
	switch (method) {
	case ARM_NORMAL:
		return "nothing";
	case ARM_DO_MAX:
		return "min.MAX";
	case ARM_STATIC:
		return "static";
	case ARM_DO_SUM:
		return "min.SUM";
	case ARM_DO_MIX:
		return "min.BOTH";
	case ARM_DO_STD:
		return "min.STD";
	default:
		return "WRONG METHOD!";
	}
}

void arm_internal_event(double time, arm_t *arm, void* ctx)
{
	sh_get_active_stripe(time, arm->meta->sctlr, (sub_ioreq_t*)ctx);
}
