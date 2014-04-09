/*
 * disksim_erasure.c
 *
 *  Created on: Feb 22, 2014
 *	  Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "disksim_global.h"
#include "disksim_erasure.h"
#include "disksim_interface.h"

#define THRESHOLD	0
#define MAXP		72

const char* get_code_name(int code) {
	switch (code) {
	case CODE_RDP:
		return "RDP";
	case CODE_EVENODD:
		return "EVENODD";
	case CODE_HCODE:
		return "HCODE";
	case CODE_XCODE:
		return "XCODE";
	}
	return "";
}

const char* get_method_name(int method) {
	switch (method) {
	case STRATEGY_OPTIMAL:
		return "Optimal";
	case STRATEGY_MIN_DIFF:
		return "Min.DIFF";
	case STRATEGY_MIN_L:
		return "Min.L";
	case STRATEGY_MIN_STD:
		return "Min.STD";
	case STRATEGY_MIN_L2:
		return "Min.L2";
	case STRATEGY_MIN_DOT:
		return "Min.DOT";
	case STRATEGY_MIN_MAX:
		return "Min.MAX";
	case STRATEGY_RANDOM:
		return "Random";
	case STRATEGY_ROW:
		return "Row.Parity";
	}
	return "";
}

static int check_prime(int number) {
	int i;
	for (i = 2; i * i <= number; i++)
		if (number % i == 0)
			return 0;
	return 1;
}

static void evenodd_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	paritys *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks - 2)) {
		fprintf(stderr, "invalid disk number using EVENODD\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks - 2;
	rows = meta->rows = meta->phydisks - 3;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (paritys*) malloc(meta->numchains * sizeof(paritys));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
		chain->type = 0;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p;
		chain->deps = NULL;
		for (c = 0; c < p; c++) {
			elem = (element*) malloc(sizeof(element));
			elem->row = r;
			elem->col = c;
			elem->next = chain->deps;
			chain->deps = elem;
		}
		chain->dest->next = chain->deps;
	}
	for (r = 0; r < rows; r++) {
		sumrc = r + p;
		chain = meta->chains + rows + r;
		chain->type = 1;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p + 1;
		chain->deps = NULL;
		for (c = 0; c < p; c++)
			if ((sumrc - c + 1) % p) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (sumrc - c) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		for (c = 1; c < p; c++) {
			elem = (element*) malloc(sizeof(element));
			elem->row = p - 1 - c;
			elem->col = c;
			elem->next = chain->deps;
			chain->deps = elem;
		}
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = rows * meta->numdisks;
	meta->totalunits = rows * cols;
	meta->entry = (struct entry_t*) malloc(meta->dataunits * sizeof(struct entry_t));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		meta->entry[unitno].row = r;
		meta->entry[unitno].col = c;
		meta->entry[unitno].depends = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void rdp_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	paritys *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->numdisks + 1)) {
		fprintf(stderr, "invalid disk number using RDP\n");
		exit(1);
	}
	p = meta->prime = meta->numdisks + 1;
	rows = meta->rows = meta->numdisks;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (paritys*) malloc(meta->numchains * sizeof(paritys));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
		chain->type = 0; // row parity
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p - 1;
		chain->deps = NULL;
		for (c = 0; c < p - 1; c++) {
			elem = (element*) malloc(sizeof(element));
			elem->row = r;
			elem->col = c;
			elem->next = chain->deps;
			chain->deps = elem;
		}
		chain->dest->next = chain->deps;
	}
	for (r = 0; r < rows; r++) {
		chain = meta->chains + rows + r;
		chain->type = 1; // diagonal parity
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p;
		chain->deps = NULL;
		for (c = 0; c < p; c++)
			if ((r - c + p) % p != p - 1) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (r - c + p) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}

	// map the data blocks to units in a stripe
	meta->dataunits = rows * meta->numdisks;
	meta->totalunits = rows * cols;
	meta->entry = (struct entry_t*) malloc(meta->dataunits * sizeof(struct entry_t));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		meta->entry[unitno].row = r;
		meta->entry[unitno].col = c;
		meta->entry[unitno].depends = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void hcode_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	paritys *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks - 1)) {
		fprintf(stderr, "invalid disk number using H-Code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks - 1;
	rows = meta->rows = meta->phydisks - 2;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (paritys*) malloc(meta->numchains * sizeof(paritys));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
		chain->type = 0;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p;
		chain->deps = NULL;
		for (c = 0; c < p; c++)
			if (r + 1 != c) {
				elem = (element*) malloc(sizeof(element));
				elem->row = r;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}
	for (r = 0; r < rows; r++) {
		delta = (r + 2) % p; // delta = c - r
		chain = meta->chains + rows + r;
		chain->type = 1;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = r + 1;
		chain->deps = NULL;
		for (c = 0; c < p; c++)
			if ((c + p - delta + 1) % p) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (c + p - delta) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = rows * meta->numdisks;
	meta->totalunits = rows * cols;
	meta->entry = (struct entry_t*) malloc(meta->dataunits * sizeof(struct entry_t));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		if (c > r) ++c;
		meta->entry[unitno].row = r;
		meta->entry[unitno].col = c;
		meta->entry[unitno].depends = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void xcode_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	paritys *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks)) {
		fprintf(stderr, "invalid disk number using X-Code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks;
	rows = meta->rows = meta->phydisks;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * cols;
	meta->chains = (paritys*) malloc(meta->numchains * sizeof(paritys));
	for (c = 0; c < p; c++) {
		chain = meta->chains + c;
		chain->type = 0;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = p - 2;
		chain->dest->col = c;
		chain->deps = NULL;
		delta = (c + 2) % p;
		for (r = 0; r < p - 2; r++) {
			elem = (element*) malloc(sizeof(element));
			elem->row = r;
			elem->col = (r + delta) % p;
			elem->next = chain->deps;
			chain->deps = elem;
		}
		chain->dest->next = chain->deps;
	}
	for (c = 0; c < p; c++) {
		chain = meta->chains + p + c;
		chain->type = 1;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = p - 1;
		chain->dest->col = c;
		chain->deps = NULL;
		sumrc = (c + p - 2) % p;
		for (r = 0; r < p - 2; r++) {
			elem = (element*) malloc(sizeof(element));
			elem->row = r;
			elem->col = (sumrc + p - r) % p;
			elem->next = chain->deps;
			chain->deps = elem;
		}
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = (rows - 2) * cols;
	meta->totalunits = rows * cols;
	meta->entry = (struct entry_t*) malloc(meta->dataunits * sizeof(struct entry_t));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->cols;
		c = unitno % meta->cols;
		meta->entry[unitno].row = r;
		meta->entry[unitno].col = c;
		meta->entry[unitno].depends = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void erasure_make_table(metadata *meta) {
	int rot;
	int unitno, id, no;
	element *dep, *tmp;

	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		for (id = 0; id < meta->totalunits; id++)
			if (meta->rmap[id] != unitno && meta->matrix[id * meta->dataunits + unitno] == 1) {
				tmp = (element*) malloc(sizeof(element));
				tmp->row = id / meta->cols;
				tmp->col = id % meta->cols;
				tmp->next = meta->entry[unitno].depends;
				meta->entry[unitno].depends = tmp;
			}
	}
}

static void gen_matrix(int id, metadata *meta, int *visit) {
	int ch = 0;
	int id1;
	int unitno;
	element *elem;
	while (ch < meta->numchains) {
		elem = meta->chains[ch].dest;
		id1 = elem->row * meta->cols + elem->col;
		if (id == id1) break;
		ch += 1;
	}
	if (ch == meta->numchains) {
		fprintf(stderr, "undefined parity block (%d, %d)\n", id / meta->cols, id % meta->cols);
		exit(1);
	}
	for (elem = meta->chains[ch].deps; elem != NULL; elem = elem->next) {
		id1 = elem->row * meta->cols + elem->col;
		if (visit[id1] == 0)
			gen_matrix(id1, meta, visit);
		for (unitno = 0; unitno < meta->dataunits; unitno++)
			meta->matrix[id * meta->dataunits + unitno] ^= meta->matrix[id1 * meta->dataunits + unitno];
	}
	visit[id] = 1;
}

static void erasure_gen_matrix(metadata *meta) {
	int *visit;
	int unitno;
	int id;
	visit = (int*) malloc(meta->totalunits * sizeof(int));
	memset(visit, 0, meta->totalunits * sizeof(int));
	meta->matrix = (int*) malloc(meta->totalunits * meta->dataunits * sizeof(int));
	memset(meta->matrix, 0, meta->totalunits * meta->dataunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		id = meta->entry[unitno].row * meta->cols + meta->entry[unitno].col;
		visit[id] = 1;
		meta->matrix[id * meta->dataunits + unitno] = 1;
	}
	for (id = 0; id < meta->totalunits; id++)
		if (visit[id] == 0)
			gen_matrix(id, meta, visit);
	free(visit);
}

static void erasure_init_rottable(metadata *meta) {
	int totalunits = meta->totalunits;
	meta->ph1 = (rottable*) malloc(sizeof(rottable));
	meta->ph1->rows = meta->rows;
	meta->ph1->cols = meta->cols;
	meta->ph1->hit = (int*) malloc(sizeof(int) * totalunits);
	meta->ph1->ll = (int*) malloc(sizeof(int) * totalunits);
	meta->ph1->rr = (int*) malloc(sizeof(int) * totalunits);
	meta->ph1->entry = meta->entry;
	meta->ph2 = (rottable*) malloc(sizeof(rottable));
	meta->ph2->rows = meta->rows;
	meta->ph2->cols = meta->cols;
	meta->ph2->hit = (int*) malloc(sizeof(int) * totalunits);
	meta->ph2->ll = (int*) malloc(sizeof(int) * totalunits);
	meta->ph2->rr = (int*) malloc(sizeof(int) * totalunits);
	meta->ph2->entry = meta->entry;
}

static int read_optimal_rdp(int w, int k, int p) {
	if (k == p)
		return w == ((1 << p) - 1);
	if (k == p - 1)
		return w == 0;
	int i, s = 0;
	for (i = 0; i < p; i++)
		s += 1 & (w >> i);
	if (s + s != p - 1)
		return 0;
	return ((1 & (w >> (p - 1 - k))) == 0)
			&& ((1 & (w >> (p - 1))) == 0);
}

static void erasure_rebuild_rdp(metadata *meta) {
	int i, j, r, c, w, mask = (1 << meta->rows) - 1, patt, p = meta->prime;
	element *elem;
	meta->normal = (int*) malloc(sizeof(int) * meta->cols);
	for (c = 0; c < meta->cols; c++)
		meta->normal[c] = (c == meta->cols - 1 ? mask : 0);
	meta->optimal = (int*) malloc(sizeof(int) * meta->cols);
	int sp = 0, nsp;
	for (i = 1; i < p; i++)
		sp = sp | (1 << i * i % p);
	nsp = ((1 << p) - 2) & ~sp;

	for (c = 0; c < meta->cols; c++) {
		if (c == p - 1) {
			w = 0;
		}  else if (c == p) {
			w = (1 << p) - 1;
		} else if ((1 & (sp >> c)) ^ ((p & 3) == 1)) {
			w = ((nsp >> (c + 1)) | nsp << (p - (c + 1)));
		} else {
			w = ((sp >> (c + 1)) | sp << (p - (c + 1)));
		}
		meta->optimal[c] = mask & w;
		w &= (1 << p) - 1;
	}
	int *mint = (int*) malloc(sizeof(int) * meta->cols);
	int *maxt = (int*) malloc(sizeof(int) * meta->cols);
	for (c = 0; c < meta->cols; c++) {
		memset(mint, 0, sizeof(int) * meta->cols);
		memset(maxt, 0, sizeof(int) * meta->cols);
		int best = 0x3fffffff;
		meta->distr[c] = (int**) malloc(sizeof(void*) * MAXP);
		meta->distrpatt[c] = (int*) malloc(sizeof(int) * MAXP);
		for (w = 0; w < (1 << p); w++)
			if (read_optimal_rdp(w, c, p)) {
				int valid = 1;
				patt = w & mask;
				memset(meta->test, 0, sizeof(int) * meta->totalunits);
				for (r = 0; r < meta->rows; r++) {
					int no = r * meta->cols + c;
					paritys *ch = NULL;
					for (j = 0; j < meta->chl[no]; j++)
						if (meta->chs[no][j]->type == (1 & (patt >> r)))
							ch = meta->chs[no][j];
					for (elem = ch->dest; elem != NULL; elem = elem->next)
						meta->test[elem->row * meta->cols + elem->col] = 1;
				}
				int flag = (c >= meta->numdisks);
				for (i = 0; i < meta->cols; i++) {
					int sum = 0;
					for (j = 0; j < meta->rows; j++)
						sum += meta->test[j * meta->cols + i];
					if (i != c && sum == meta->rows - 1 && maxt[i] < 5)
						flag = maxt[i] += 1;
					if (i != c && sum == meta->rows / 2 && mint[i] < 5)
						flag = mint[i] += 1;
				}

				int idx = meta->distrlen[c];
				if ((flag && idx < MAXP) || patt == meta->optimal[c]) {
					meta->distrlen[c] += 1;
					meta->distrpatt[c][idx] = patt;
					meta->distr[c][idx] = (int*) malloc(sizeof(int) * meta->cols);
					memset(meta->distr[c][idx], 0, sizeof(int) * meta->cols);
					for (j = 0; j < meta->totalunits; j++)
						meta->distr[c][idx][j % meta->cols] += meta->test[j];
				}
			}
		if (meta->distrlen[c] == 0)
			exit(-1);
	}
}

static void erasure_rebuild_evenodd(metadata *meta) {

}

static void erasure_rebuild_init(metadata *meta) {
	int i, j, unitsize = meta->unitsize;
	element *elem;
	meta->numfailures = 0;
	meta->failed = (int*) malloc(sizeof(int) * meta->phydisks);
	memset(meta->failed, 0, sizeof(int) * meta->phydisks);
	meta->laststripe = -1;
	meta->chs = (paritys***) malloc(sizeof(void*) * meta->totalunits);
	meta->chl = (int*) malloc(sizeof(int) * meta->totalunits);
	memset(meta->chl, 0, sizeof(int) * meta->totalunits);
	for (i = 0; i < meta->totalunits; i++)
		meta->chs[i] = (paritys**) malloc(sizeof(void*) * 3);
	for (i = 0; i < meta->numchains; i++) {
		paritys *chain = meta->chains + i;
		for (elem = chain->dest; elem != NULL; elem = elem->next) {
			int no = elem->row * meta->cols + elem->col;
			meta->chs[no][meta->chl[no]++] = chain;
		}
	}
	int w, mask = (1 << meta->rows) - 1, p = meta->prime;
	meta->distr = (int***) malloc(sizeof(void*) * meta->cols);
	meta->distrlen = (int*) malloc(sizeof(int) * meta->cols);
	meta->distrpatt = (int**) malloc(sizeof(void*) * meta->cols);
	memset(meta->distrlen, 0, sizeof(int) * meta->cols);
	meta->test = (int*) malloc(sizeof(int) * meta->totalunits);
	switch (meta->codetype) {
	case CODE_RDP:
		erasure_rebuild_rdp(meta);
		break;
	case CODE_EVENODD:
		erasure_rebuild_evenodd(meta);
		break;
	case CODE_HCODE:
		break;
	}
	meta->last = 0;
}

void erasure_initialize(metadata *meta, int codetype, int disks, int unitsize) {
	meta->codetype = codetype;
	meta->phydisks = disks;
	meta->unitsize = unitsize;
	switch (meta->codetype) {
	case CODE_RDP:
		rdp_initialize(meta);
		break;
	case CODE_EVENODD:
		evenodd_initialize(meta);
		break;
	case CODE_HCODE:
		hcode_initialize(meta);
		break;
	case CODE_XCODE:
		xcode_initialize(meta);
		break;
	default:
		fprintf(stderr, "unrecognized reduntype in erasure_initialize\n");
		exit(1);
	}
	erasure_gen_matrix(meta);
	erasure_make_table(meta);
	erasure_init_rottable(meta);
	erasure_rebuild_init(meta);
}

static void rottable_update(rottable *tab, int r, int c, int ll, int rr) {
	int no = r * tab->cols + c;
	if (tab->hit[no] == 0) {
		tab->hit[no] = -1;
		tab->ll[no] = ll;
		tab->rr[no] = rr;
	} else {
		tab->ll[no] = min(ll, tab->ll[no]);
		tab->rr[no] = max(rr, tab->rr[no]);
	}
}

void erasure_maprequest(metadata *meta, ioreq *req) {
	int numreqs = 0;
	int unitsize = meta->unitsize;
	int stripesize = meta->dataunits * unitsize;
	int start = req->blkno;
	int end = req->blkno + req->bcount;
	int stripeno = start / stripesize;
	element *elem;
	rottable *ph1 = meta->ph1;
	rottable *ph2 = meta->ph2;
	int unitno, llim, rlim, i, no;

	req->curr = NULL;
	while (stripeno * stripesize < end) {
		int rot = stripeno % meta->cols;
		memset(ph1->hit, 0, sizeof(int) * meta->totalunits);
		memset(ph2->hit, 0, sizeof(int) * meta->totalunits);
		int llim = max(0, start / unitsize - stripeno * meta->dataunits);
		int rlim = min(meta->dataunits, end / unitsize - stripeno * meta->dataunits + 1);
		for (unitno = llim; unitno < rlim; unitno++) {
			int offset = (stripeno * meta->dataunits + unitno) * unitsize;
			int ll = max(start - offset, 0);
			int rr = min(end - offset, unitsize);
			if (ll == rr) break;
			int r = meta->entry[unitno].row;
			int dc = (meta->entry[unitno].col + rot) % meta->cols;
			if (!meta->failed[dc]) {
				rottable_update(ph1, r, dc, ll, rr);
				if (req->flag == DISKSIM_WRITE) {
					for (elem = meta->entry[unitno].depends; elem != NULL; elem = elem->next)
						if (!meta->failed[(elem->col + rot) % meta->cols]) {
							rottable_update(ph1, elem->row, (elem->col + rot) % meta->cols, ll, rr);
							rottable_update(ph2, elem->row, (elem->col + rot) % meta->cols, ll, rr);
						}
					rottable_update(ph2, r, dc, ll, rr);
				}
			} else { // degraded read/write
				no = r * meta->cols + dc;
				for (elem = meta->chs[no][0]->dest; elem != NULL; elem = elem->next)
					if (!meta->failed[(elem->col + rot) % meta->cols])
						rottable_update(ph1, elem->row, (elem->col) % meta->cols, ll, rr);
				if (req->flag == DISKSIM_WRITE) { // same as not missing
					for (elem = meta->entry[unitno].depends; elem != NULL; elem = elem->next)
						if (!meta->failed[(elem->col + rot) % meta->cols]) {
							rottable_update(ph1, elem->row, (elem->col + rot) % meta->cols, ll, rr);
							rottable_update(ph2, elem->row, (elem->col + rot) % meta->cols, ll, rr);
						}
				}
			}
		}
		int r, nxt, dc;
		struct disksim_request *tmp = (struct disksim_request*) getfromextraq();
		iogroup *g1 = create_ioreq_group();
		for (dc = 0; dc < meta->cols; dc++) {
			if (meta->failed[dc]) continue;
			for (r = 0; r < meta->rows; r = nxt)
				if (ph1->hit[r * meta->cols + dc]) {
					no = r * meta->cols + dc;
					int ll = ph1->ll[no] + (stripeno * meta->rows + r) * unitsize;
					int rr = ph1->rr[no] + (stripeno * meta->rows + r) * unitsize;
					for (nxt = r + 1; nxt < meta->rows; nxt++)
						if (ph1->hit[nxt * meta->cols + dc]) {
							int no2 = nxt * meta->cols + dc;
							int l2 = ph1->ll[no2] + (stripeno * meta->rows + nxt) * unitsize;
							int r2 = ph1->rr[no2] + (stripeno * meta->rows + nxt) * unitsize;
							if (rr + THRESHOLD >= l2)
								rr = r2;
							else {
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
		for (dc = 0; dc < meta->cols; dc++) {
			if (meta->failed[dc]) continue;
			for (r = 0; r < meta->rows; r = nxt)
				if (ph2->hit[r * meta->cols + dc]) {
					no = r * meta->cols + dc;
					int ll = ph2->ll[no] + (stripeno * meta->rows + r) * unitsize;
					int rr = ph2->rr[no] + (stripeno * meta->rows + r) * unitsize;
					for (nxt = r + 1; nxt < meta->rows; nxt++)
						if (ph1->hit[nxt * meta->cols + dc]) {
							int no2 = nxt * meta->cols + dc;
							int l2 = ph2->ll[no2] + (stripeno * meta->rows + nxt) * unitsize;
							int r2 = ph2->rr[no2] + (stripeno * meta->rows + nxt) * unitsize;
							if (rr + THRESHOLD >= l2)
								rr = r2;
							else {
								break;
							}
						}
					tmp = (struct disksim_request*) getfromextraq();
					tmp->start = req->time;
					tmp->devno = dc;
					tmp->blkno = ll;
					tmp->bytecount = (rr - ll) * 512;
					tmp->flags = DISKSIM_WRITE; // write
					tmp->next = g2->reqs;
					tmp->reqctx = req;
					g2->reqs = tmp;
					g2->numreqs++;
				} else nxt = r + 1;
		}
		add_to_ioreq(req, g1);
		add_to_ioreq(req, g2);
		stripeno++;
	}
}

int cmp(const void *a, const void *b) {
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

double erasure_get_score(int *a, int *b, int *mask, int disks, int method) {
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

void erasure_disk_failure(metadata *meta, int devno) {
	int temp = meta->numfailures;
	if (!meta->failed[devno]) {
		meta->failed[devno] = 1;
		meta->numfailures += 1;
	}
	if (meta->numfailures + meta->numdisks > meta->phydisks) {
		fprintf(stderr, "data permanently lost.\n");
		exit(-1);
	}
	meta->laststripe = -1;
}

void erasure_adaptive_rebuild(metadata *meta, ioreq *req, int stripeno, int pattern) {
	static int rotated[50];
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
	paritys *chain;
	element *elem;

	if (meta->numfailures > 1) {
		// normal
		fprintf(stderr, "should not reach!\n");
		exit(-1);
	} else {
		int *dd = (int*) malloc(sizeof(int) * meta->cols);
		memset(dd, 0, sizeof(int) * meta->cols);

		int dc = 0, ch;
		for (dc = 0; !meta->failed[dc]; dc++);
		int c = (dc + meta->cols - rot) % meta->cols;
		memset(meta->test, 0, sizeof(int) * meta->totalunits);
		for (r = 0; r < meta->rows; r++) {
			int no = r * meta->cols + c; // logical
			int t = 0;
			while (meta->chs[no][t]->type != (1 & (pattern >> r))) t++;
			for (elem = meta->chs[no][t]->dest; elem != NULL; elem = elem->next)
				meta->test[elem->row * meta->cols + (elem->col + rot) % meta->cols] = unitsize;
		}
		int nxt;
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
								if (rr + THRESHOLD >= l2) {
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
						dd[dc] += rr - ll;
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

int erasure_get_rebuild_distr(metadata *meta, int stripeno, int pattern, int *distr) {
	if (meta->numfailures == 0)
		return -1;

	memset(distr, 0, sizeof(int) * meta->cols);
	memset(meta->test, 0, sizeof(int) * meta->totalunits);

	int rot = stripeno % meta->cols, dc = 0;
	while (!meta->failed[dc]) dc++;
	int c = (dc + meta->cols - rot) % meta->cols;
	int i, j, r;
	element *elem;

	for (r = 0; r < meta->rows; r++) {
		int flag = 0;
		for (i = 0; i < meta->chl[c]; i++)
			if (meta->chs[c][i]->type == (1 & (pattern >> r))) {
				flag = 1;
				for (elem = meta->chs[c][i]->dest; elem != NULL; elem = elem->next)
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
