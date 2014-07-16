/*
 * disksim_arm.c
 *
 *  Created on: Apr 19, 2014
 *      Author: zyz915
 */

#include <stdlib.h>

#include "disksim_arm.h"
#include "disksim_erasure.h"
#include "disksim_global.h"
#include "disksim_interface.h"
#include "disksim_timer.h"

static int *numpatts = NULL;
static int **patts = NULL;

static int **patts_distr = NULL;
static int *rowpatt = NULL;
static int *diagpatt = NULL;
static int *optimal = NULL;

const char* arm_method_name(int method) {
	switch (method) {
	case STRATEGY_ROW:
		return "Row";
	case STRATEGY_DIAG:
		return "Diagonal";
	case STRATEGY_OPTIMAL:
		return "Optimal";
	case STRATEGY_MIN_L:
		return "Min.L";
	case STRATEGY_MIN_STD:
		return "Min.STD";
	case STRATEGY_MIN_L2:
		return "Min.L2";
	case STRATEGY_RANDOM:
		return "Random";
	case STRATEGY_MIN_DOT:
		return "Min.DOT";
	case STRATEGY_MIN_MAX:
		return "Min.MAX";
	case STRATEGY_MIN_DIFF:
		return "Min.DIFF";
	}
	return "";
}

static int cmp(const void *a, const void *b) {
	return *(int*)a - *(int*)b;
}

static int ss[32];
double min_L(int *a, int *b, int *mask, int disks) {
	int i, n = 0;
	for (i = 0; i < disks; i++)
		if (!mask[i])
			ss[n++] = a[i] + b[i];
	qsort(ss, n, sizeof(int), cmp);
	return (double)(ss[n - 1] + 1) / (ss[0] + 1);
}

double min_diff(int *a, int *b, int *mask, int disks) {
	int i, n = 0;
	for (i = 0; i < disks; i++)
		if (!mask[i])
			ss[n++] = a[i] + b[i];
	qsort(ss, n, sizeof(int), cmp);
	return (double)(ss[n - 1] - ss[0]);
}

double min_L2(int *a, int *b, int *mask, int disks) {
	int i, n = 0;
	for (i = 0; i < disks; i++)
		if (!mask[i])
			ss[n++] = a[i] + b[i];
	qsort(ss, n, sizeof(int), cmp);
	return (double)(ss[n - 1] + ss[n - 2] + 1) / (ss[0] + ss[1] + 1);
}

double min_std(int *a, int *b, int *mask, int disks) {
	int i, n = 0;
	double s = 0, s2 = 0, c;
	for (i = 0; i < disks; i++)
		if (!mask[i]) {
			c = (a[i] + b[i]);
			s += c;
			s2 += c * c;
			n++;
		}
	return sqrt((s2 * n - s * s) / (n * n));
}

double min_max(int *a, int *b, int *mask, int disks) {
	int ret = 0x3fffffff, i;
	for (i = 0; i < disks; i++)
		ret = min(ret, a[i] + b[i]);
	return (double) ret;
}

double min_dot(int *a, int *b, int *mask, int disks) {
	int i, sum = 0, ret = 0;
	for (i = 0; i < disks; i++)
		if (!mask[i]) sum += b[i];
	if (sum == 0)
		return min_std(a, b, mask, disks);
	for (i = 0; i < disks; i++)
		if (!mask[i]) ret += a[i] * b[i];
	return (double) ret;
}

double s_random(int *a, int *b, int *mask, int disks) {
	return (double)rand() / RAND_MAX;
}

double arm_get_score(int *a, int *b, int *mask, int disks, int method) {
	switch (method) {
	case STRATEGY_MIN_L:
		return min_L(a, b, mask, disks);
	case STRATEGY_MIN_DIFF:
		return min_diff(a, b, mask ,disks);
	case STRATEGY_MIN_STD:
		return min_std(a, b, mask, disks);
	case STRATEGY_MIN_L2:
		return min_L2(a, b, mask, disks);
	case STRATEGY_MIN_DOT:
		return min_dot(a, b, mask, disks);
	case STRATEGY_MIN_MAX:
		return min_max(a, b, mask, disks);
	case STRATEGY_RANDOM:
		return s_random(a, b, mask, disks);
	default:
		fprintf(stderr, "invalid strategy number.\n");
		exit(-1);
	}
}

void arm_rebuild(metadata_t *meta, ioreq_t *req, int stripeno, int pattern) {
	static int distr[50];
	if (meta->numfailures == 0) return;
	int unitsize = meta->unitsize;
	int rot = stripeno % meta->cols;
	if (stripeno * meta->rows * unitsize > 6000000) {
		meta->numfailures = 0;
		memset(meta->failed, 0, sizeof(int) * meta->cols);
		meta->laststripe = -1;
		return;
	}
	int bcount = unitsize * meta->rows;
	int blkno = stripeno * bcount;
	int i, numreqs = 0, r;
	parity *chain;
	element_t *elem;

	if (meta->numfailures > 1) {
		// normal
		fprintf(stderr, "should not reach!\n");
		exit(-1);
	} else {
		int dc = 0, ch, nxt;
		for (dc = 0; !meta->failed[dc]; dc++);
		int c = (dc + meta->cols - rot) % meta->cols;
		arm_get_rebuild_distr(meta, stripeno, pattern, distr);
		for (i = 0; i < meta->totalunits; i++)
			meta->test[i] *= unitsize;
		struct disksim_request *tmp;
		for (dc = 0; dc < meta->cols; dc++)
			if (!meta->failed[dc]) {
				for (r = 0; r < meta->rows; r = nxt)
					if (meta->test[r * meta->cols + dc]) {
						int ll = (stripeno * meta->rows + r) * unitsize;
						int rr = (stripeno * meta->rows + r + 1) * unitsize;
						for (nxt = r + 1; nxt < meta->rows; nxt++)
							if (meta->test[nxt * meta->cols + dc]) {
								int l2 = (stripeno * meta->rows + nxt) * unitsize;
								int r2 = (stripeno * meta->rows + nxt + 1) * unitsize;
								if (rr == l2) {
									rr = r2;
								} else {
									break;
								}
							}
						tmp = (struct disksim_request*) getfromextraq();
						tmp->start = req->time;
						tmp->devno = dc;
						tmp->blkno = ll;
						tmp->bytecount = (rr - ll) * 512;
						tmp->flags = DISKSIM_READ; // read
						tmp->reqctx = req;
						add_to_ioreq(req, tmp);
					} else nxt = r + 1;
			}
		for (dc = 0; dc < meta->cols; dc++)
			if (meta->failed[dc]) {
				tmp = (struct disksim_request*) getfromextraq();
				tmp->start = req->time;
				tmp->devno = dc;
				tmp->blkno = stripeno * meta->rows * unitsize;
				tmp->bytecount = meta->rows * unitsize * 512;
				tmp->flags = DISKSIM_WRITE;
				tmp->reqctx = req;
				add_to_ioreq(req, tmp);
			}
	}
}

int arm_get_rebuild_distr(metadata_t *meta, int stripeno, int pattern, int *distr) {
	if (meta->numfailures == 0)
		return -1;
	memset(distr, 0, sizeof(int) * meta->cols);
	memset(meta->test, 0, sizeof(int) * meta->totalunits);

	int rot = stripeno % meta->cols, dc = 0;
	while (!meta->failed[dc]) dc++;
	int c = (dc + meta->cols - rot) % meta->cols;
	int i, j, r;
	element_t *elem;

	for (r = 0; r < meta->rows; r++) {
		int flag = 0;
		int no = r * meta->cols + c;
		for (i = 0; i < meta->chl[no]; i++)
			if (meta->chs[no][i]->type == (1 & (pattern >> r))) {
				flag = 1;
				for (elem = meta->chs[no][i]->dest; elem != NULL; elem = elem->next)
					if (elem->col != c)
						meta->test[elem->row * meta->cols + (elem->col + rot) % meta->cols] = 1;
			}
		if (flag == 0)
			return -1;
	}
	for (j = 0; j < meta->cols; j++)
		for (i = 0; i < meta->rows; i++)
			distr[j] += meta->test[i * meta->cols + j];
	return 0;
}

int arm_select_pattern(metadata_t *meta, int stripeno, int method, int *distr, int coef) {
	static int tmp[24];
	int disks = meta->cols;
	stripeno %= meta->cols;
	if (method == STRATEGY_OPTIMAL)
		return optimal[stripeno];
	else if (method == STRATEGY_ROW)
		return rowpatt[stripeno];
	else if (method == STRATEGY_DIAG)
		return diagpatt[stripeno];
	else {
		timer_start(2);
		int i, j, patt = -1, ch;
		double best;
		for (i = 0; i < numpatts[stripeno]; i++) {
			for (j = 0; j < meta->cols; j++)
				tmp[j] = patts_distr[stripeno][i * disks + j] * coef;
			double score = arm_get_score(tmp, distr, meta->failed, meta->cols, method);
			if (patt == -1 || score < best) {
				best = score;
				patt = patts[stripeno][i];
				ch = i;
			}
		}
		timer_end(2);
		return patt;
	}
}

void arm_shuffle(int *a, int n) {
	int i, j, t;
	for (i = 1; i < n; i++) {
		j = rand() % (i + 1);
		t = a[i];
		a[i] = a[j];
		a[j] = t;
	}
}

unsigned arm_hash(int *a, int n) {
	unsigned i, ret = 0;
	for (i = 0; i < n; i++)
		ret = ret * 23 + a[i];
	return ret;
}

int arm_filtering(metadata_t *meta, int stripeno, int *distrs, int *patts, int disks) {
	int patt, dsize = sizeof(int) * disks, tot = 0;
	int *distr = malloc(dsize);
	for (patt = 0; patt < (1 << meta->rows); patt++)
		if (arm_get_rebuild_distr(meta, stripeno, patt, distr) == 0) {
			memcpy(distrs + tot * disks, distr, dsize);
			patts[tot++] = patt;
			rowpatt[stripeno] = min(rowpatt[stripeno], patt);
			diagpatt[stripeno] = max(diagpatt[stripeno], patt);
		}
	free(distr);
	return tot;
}

int arm_sampling(metadata_t *meta, int stripeno, int *distrs, int *patts, int disks, int limit) {
	int patt, dsize = sizeof(int) * disks, tot = 0, iter;
	int mask = (1 << meta->rows) - 1;
	int *distr = malloc(dsize);
	for (iter = 0; iter < limit; iter++) {
		patt = rand() & mask;
		if (arm_get_rebuild_distr(meta, stripeno, patt, distr) == 0) {
			memcpy(distrs + tot * disks, distr, dsize);
			patts[tot++] = patt;
			rowpatt[stripeno] = min(rowpatt[stripeno], patt);
			diagpatt[stripeno] = max(diagpatt[stripeno], patt);
		}
	}
	free(distr);
	return tot;
}

void arm_selection_threshold(int *distrs, int disks, int tot, int *max_v, int *min_v) {
	int c, i;
	int *sort = (int*) malloc(sizeof(int) * tot);
	for (c = 0; c < disks; c++) {
		for (i = 0; i < tot; i++)
			sort[i] = distrs[i * disks + c];
		qsort(sort, tot, sizeof(int), cmp);
		min_v[c] = sort[tot / disks];
		max_v[c] = sort[tot - 1 - tot / disks];
	}
	free(sort);
}

void arm_initialize_patterns_new(metadata_t *meta, int pattcnt, int limit) {
	int disks = meta->cols, rows = meta->rows;
	int maxpatts = min(limit, 1 << rows);
	int sample = (1 << rows) > limit;
	size_t dsize = sizeof(int) * meta->cols;
	int *distrs = (int*) malloc(dsize * maxpatts);
	int *tmppatt = (int*) malloc(sizeof(int) * maxpatts);
	int *s = (int*) malloc(sizeof(int) * maxpatts);
	int *sort = (int*) malloc(sizeof(int) * maxpatts);
	int *max_v = (int*) malloc(dsize);
	int *min_v = (int*) malloc(dsize);
	int *max_cnt = (int*) malloc(dsize);
	int *min_cnt = (int*) malloc(dsize);
	int *shuf = (int*) malloc(sizeof(int) * maxpatts);
	patts = (int**) malloc(dsize);
	patts_distr = (int**) malloc(dsize);
	numpatts = (int*) malloc(dsize);
	rowpatt = (int*) malloc(dsize);
	diagpatt = (int*) malloc(dsize);
	optimal = (int*) malloc(dsize);
	int totalpatts = 0, no, i, j;
	for (no = 0; no < disks; no++) {
		int tot = 0, near_opt = 0;
		rowpatt[no] = (1 << rows) - 1;
		diagpatt[no] = 0;
		if (sample)
			tot = arm_sampling(meta, no, distrs, tmppatt, disks, limit);
		else
			tot = arm_filtering(meta, no, distrs, tmppatt, disks);
		// row & diagonal only
		rowpatt[no] = diagpatt[no] = tmppatt[0];
		for (i = 0; i < tot; i++) {
			rowpatt[no] &= tmppatt[i];
			diagpatt[no] |= tmppatt[i];
		}
		// near optimal recovery scheme
		memset(s, 0, sizeof(int) * tot);
		for (i = 0; i < tot; i++) {
			for (j = 0; j < disks; j++)
				s[i] += distrs[i * disks + j];
			sort[i] = s[i];
		}
		qsort(sort, tot, sizeof(int), cmp);
		int thresh = sort[tot / 3];
		for (i = 0; i < tot; i++)
			if (s[i] <= thresh) {
				memcpy(distrs + near_opt * disks, distrs + i * disks, dsize);
				tmppatt[near_opt] = tmppatt[i];
				shuf[near_opt] = near_opt;
				near_opt++;
			}
		// Optimal & Balanced
		int std = 0x3fffffff, pos = 0;
		for (i = 0; i < tot; i++)
			if (s[i] == sort[0]) {
				int x = 0, sx = 0, sxx = 0;
				int *a = distrs + i * disks;
				for (j = 0; j < disks; j++) {
					x = a[j];
					sx += x;
					sxx += x * x;
				}
				int curr = sxx * (disks - 1) -sx * sx;
				if (curr < std) {
					std = curr;
					optimal[no] = tmppatt[i];
				}
			}
		arm_selection_threshold(distrs, disks, near_opt, max_v, min_v);
		arm_shuffle(shuf, near_opt);
		memset(max_cnt, 0, dsize);
		memset(min_cnt, 0, dsize);
		numpatts[no] = 0;
		for (i = 0; i < near_opt; i++) {
			int ch = pos, *a = distrs + shuf[i] * disks;
			do {
				if (a[ch] >= max_v[ch] && max_cnt[ch] < pattcnt)
					break;
				if (a[ch] <= min_v[ch] && min_cnt[ch] < pattcnt)
					break;
				ch = (ch + 1 == disks ? 0 : ch + 1);
			} while (ch != pos);
			if (a[ch] >= max_v[ch] && max_cnt[ch] < pattcnt) {
				numpatts[no]++;
				max_cnt[ch] += 1;
				s[shuf[i]] = -1; // selected
			}
			else if (a[ch] <= min_v[ch] && min_cnt[ch] < pattcnt) {
				numpatts[no]++;
				min_cnt[ch] += 1;
				s[shuf[i]] = -1; // selected
			}
			pos = (pos + 1 == disks ? 0 : pos + 1);
		}
		patts[no] = (int*) malloc(sizeof(int) * numpatts[no]);
		patts_distr[no] = (int*) malloc(dsize * numpatts[no]);
		int t = 0;
		for (i = 0; i < near_opt; i++)
			if (s[i] == -1) { // selected
				patts[no][t] = tmppatt[i];
				memcpy(patts_distr[no] + (t++) * disks, distrs + i * disks, dsize);
			}
		totalpatts += numpatts[no];
	}
	free(distrs); free(tmppatt); free(s); free(sort); free(shuf);
	free(max_v); free(max_cnt); free(min_v); free(min_cnt);
}

/*
void arm_initialize_patterns(metadata *meta, int pattcnt) {
	int disks = meta->cols;
	int *a = (int*) malloc(sizeof(int) * meta->cols * (1 << meta->rows));
	int *p = (int*) malloc(sizeof(int) * (1 << meta->rows));
	double *s = (double*) malloc(sizeof(double) * (1 << meta->rows));
	int *ha = (int*) malloc(sizeof(int) * pattcnt * disks * 2);
	int *se = (int*) malloc(sizeof(int) * (1 << meta->rows));
	int *sh = (int*) malloc(sizeof(int) * (1 << meta->rows));
	int *sm = (int*) malloc(sizeof(int) * (1 << meta->rows));
	int *sm2 = (int*) malloc(sizeof(int) * (1 << meta->rows));
	size_t dsize = sizeof(int) * meta->cols;
	int *distr = (int*) malloc(dsize);
	int *ma = (int*) malloc(dsize);
	int *mi = (int*) malloc(dsize);
	int *mac = (int*) malloc(dsize);
	int *mic = (int*) malloc(dsize);
	patts = (int**) malloc(dsize);
	patts_distr = (int**) malloc(dsize);
	numpatts = (int*) malloc(dsize);
	rowpatt = (int*) malloc(dsize);
	diagpatt = (int*) malloc(dsize);
	optimal = (int*) malloc(dsize);
	int i, j, patt, totalpatts = 0, stripeno;
	for (stripeno = 0; stripeno < disks; stripeno++) {
		int tot = 0;
		rowpatt[stripeno] = (1 << meta->rows) - 1;
		diagpatt[stripeno] = 0;
		for (patt = 0; patt < (1 << meta->rows); patt++)
			if (arm_get_rebuild_distr(meta, stripeno, patt, distr) == 0) {
				memcpy(a + tot * disks, distr, dsize);
				p[tot++] = patt;
				rowpatt[stripeno] = min(rowpatt[stripeno], patt);
				diagpatt[stripeno] = max(diagpatt[stripeno], patt);
			}
		memset(ma, 0, dsize);
		memset(mi, 7, dsize);

		double std = 1e9;
		for (i = 0; i < tot; i++) {
			int sx = 0, sxx = 0, x;
			for (j = 1; j < meta->cols; j++) {
				x = a[i * meta->cols + j];
				sx += x;
				sxx += x * x;
			}
			sm[i] = sx;
			s[i] = (double)(disks - 1) * sxx - (double)sx * sx;
		}

		memcpy(sm2, sm, sizeof(int) * tot);
		qsort(sm2, tot, sizeof(int), cmp);
		int sum = sm2[tot / 3];

		for (i = 0; i < tot; i++)
			if (sm[i] >= sum) {
				std = min(std, s[i]);
				for (j = 0; j < meta->cols; j++) {
					int x = a[i * meta->cols + j];
					ma[j] = max(ma[j], x);
					mi[j] = min(mi[j], x);
				}
			}
		for (i = 0; i < meta->cols; i++) {
			mac[i] = mic[i] = pattcnt;
			ma[i]--;
			mi[i]++;
		}
		for (i = 0; i < tot; i++)
			sh[i] = i;
		arm_shuffle(sh, tot);
		memset(se, 0, sizeof(int) * tot);

		numpatts[stripeno] = 0;
		for (i = 0; i < tot; i++) {
			int idx = sh[i], *b = a + idx * meta->cols;
			if (sm[idx] < sum) continue;
			unsigned hh = arm_hash(b, disks), f = 1;
			for (j = 0; j < numpatts[stripeno]; j++)
				if (hh == ha[j])  f = 0;
			if (f == 0) continue;
			ha[numpatts[stripeno]] = hh;
			for (j = 1; j < meta->cols; j++) {
				se[idx] |= (b[j] == ma[j] && mac[j] > 0);
				se[idx] |= (b[j] == mi[j] && mic[j] > 0);
			}
			se[idx] |= (s[idx] == std);
			if (!se[idx]) continue;
			numpatts[stripeno]++;
			for (j = 1; j < meta->cols; j++) {
				if (b[j] == ma[j]) mac[j]--;
				if (b[j] == mi[j]) mic[j]--;
			}
			if (s[idx] == std)
				optimal[stripeno] = p[idx];
		}

		patts[stripeno] = (int*) malloc(sizeof(int) * numpatts[stripeno]);
		patts_distr[stripeno] = (int*) malloc(dsize * numpatts[stripeno]);
		int t = 0;
		for (i = 0; i < tot; i++)
			if (se[i]) {
				patts[stripeno][t] = p[i];
				memcpy(patts_distr[stripeno] + (t++) * disks, a + i * disks, dsize);
			}
		totalpatts += numpatts[stripeno];
	}
	free(a);  free(p);   free(s);  free(ha);
	free(se); free(sh);  free(sm); free(sm2);
	free(ma); free(mac); free(mi); free(mic);
	free(distr);
}
*/
