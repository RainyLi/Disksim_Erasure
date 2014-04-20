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

void arm_rebuild(metadata *meta, ioreq *req, int stripeno, int pattern) {
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
	parities *chain;
	element *elem;

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
		iogroup *g1 = create_ioreq_group();
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
						tmp->next = g1->reqs;
						tmp->reqctx = req;
						g1->reqs = tmp;
						g1->numreqs++;
					} else nxt = r + 1;
			}
		iogroup *g2 = create_ioreq_group();
		for (dc = 0; dc < meta->cols; dc++)
			if (meta->failed[dc]) {
				tmp = (struct disksim_request*) getfromextraq();
				tmp->start = req->time;
				tmp->devno = dc;
				tmp->blkno = stripeno * meta->rows * unitsize;
				tmp->bytecount = meta->rows * unitsize * 512;
				tmp->flags = DISKSIM_WRITE;
				tmp->next = g2->reqs;
				tmp->reqctx = req;
				g2->reqs = tmp;
				g2->numreqs++;
			}
		req->curr = NULL;
		add_to_ioreq(req, g1);
		add_to_ioreq(req, g2);
	}
}

static int arm_get_rebuild_distr_evenodd(metadata *meta, int stripeno, int pattern, int *distr) {
	int rot = stripeno % meta->cols, dc = 0;
	while (!meta->failed[dc]) dc++;
	int c = (dc + meta->cols - rot) % meta->cols, r;
	int mask = (1 << meta->rows) - 1;

	int i, j, k;
	if (c >= meta->cols - 2) {
		if (c == meta->cols - 2 && pattern != 0)
			return -1;
		if (c == meta->cols - 1 && pattern != mask)
			return -1;
		for (i = 0; i < meta->rows; i++)
			for (j = 0; j < meta->cols; j++)
				if (j != c)
					meta->test[i * meta->cols + (j + rot) % meta->cols] = 1;
	} else {
		if (pattern != 0)
			for (i = 0; i < meta->rows; i++)
				for (j = meta->cols - 2; j < meta->cols; j++)
					meta->test[i * meta->cols + (j + rot) % meta->cols] = 1;
		for (r = 0; r < meta->rows; r++) {
			if (1 & (pattern >> r)) { // diagonal
				for (j = 0; j < meta->cols - 2; j++) {
					i = (r + c + meta->prime - j) % meta->prime;
					if (i != meta->rows - 1 && j != c)
						meta->test[i * meta->cols + (j + rot) % meta->cols] = 1;
				}
			} else { // row
				for (j = 0; j < meta->cols - 1; j++)
					if (j != c)
						meta->test[r * meta->cols + (j + rot) % meta->cols] = 1;
			}
		}
	}

	for (i = 0; i < meta->rows; i++)
		for (j = 0; j < meta->cols; j++)
			distr[j] += meta->test[i * meta->cols + j];
	return 0;
}

int arm_get_rebuild_distr(metadata *meta, int stripeno, int pattern, int *distr) {
	if (meta->numfailures == 0)
		return -1;

	memset(distr, 0, sizeof(int) * meta->cols);
	memset(meta->test, 0, sizeof(int) * meta->totalunits);

	//if (meta->codetype == CODE_EVENODD)
	//	return erasure_get_rebuild_distr_evenodd(meta, stripeno, pattern, distr);

	int rot = stripeno % meta->cols, dc = 0;
	while (!meta->failed[dc]) dc++;
	int c = (dc + meta->cols - rot) % meta->cols;
	int i, j, r;
	element *elem;

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
	for (i = 0; i < meta->rows; i++)
		for (j = 0; j < meta->cols; j++)
			distr[j] += meta->test[i * meta->cols + j];
	return 0;
}

int arm_select_pattern(metadata *meta, int stripeno, int method, int *distr, int coef) {
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
		return patt;
	}
}

void shuffle(int *a, int n) {
	int i, j, t;
	for (i = 1; i < n; i++) {
		j = rand() % (i + 1);
		t = a[i];
		a[i] = a[j];
		a[j] = t;
	}
}

unsigned hash(int *a, int n) {
	unsigned i, ret = 0;
	for (i = 0; i < n; i++)
		ret = ret * 23 + a[i];
	return ret;
}

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
	int i, j, patt, *distr = (int*) malloc(dsize);
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
	int totalpatts = 0, stripeno;
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
		shuffle(sh, tot);
		memset(se, 0, sizeof(int) * tot);

		numpatts[stripeno] = 0;
		for (i = 0; i < tot; i++) {
			int idx = sh[i], *b = a + idx * meta->cols;
			if (sm[idx] < sum) continue;
			unsigned hh = hash(b, disks), f = 1;
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
	free(a);  free(s);
	free(se); free(sh);  free(sm);
	free(ma); free(mac);
	free(mi); free(mic);
	free(distr);
}
