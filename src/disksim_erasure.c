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

#define THRESHOLD	8

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
	case CODE_LIBERATION:
		return "LIBERATION";
	case CODE_SHCODE:
		return "SHCODE";
	default:
		fprintf(stderr, "invalid code ID: %d\n", code);
		exit(-1);
	}
	return "";
}

int get_code_id(const char *code) {
	if (!strcmp(code, "rdp"))
		return CODE_RDP;
	if (!strcmp(code, "evenodd"))
		return CODE_EVENODD;
	if (!strcmp(code, "hcode"))
		return CODE_HCODE;
	if (!strcmp(code, "xcode"))
		return CODE_XCODE;
	if (!strcmp(code, "liberation"))
		return CODE_LIBERATION;
	if (!strcmp(code, "shcode"))
		return CODE_SHCODE;
	fprintf(stderr, "invalid code: %s\n", code);
	exit(-1);
	return -1;
}

iogroup* create_ioreq_group() {
	iogroup *ret = (iogroup*) getfromextraq();
	ret->numreqs = 0;
	ret->reqs = NULL;
	return ret;
}

void add_to_ioreq(ioreq *req, iogroup *group) {
	if (group->numreqs > 0) {
		if (req->curr == NULL)
			req->groups = group;
		else
			req->curr->next = group;
		req->curr = group;
		group->next = NULL;
	}
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
	parities *chain;
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
	meta->chains = (parities*) malloc(meta->numchains * sizeof(parities));
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
	meta->dataunits = rows * (cols - 2);
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
	parities *chain;
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
	meta->chains = (parities*) malloc(meta->numchains * sizeof(parities));
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
	meta->dataunits = rows * (cols - 2);
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
	parities *chain;
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
	meta->chains = (parities*) malloc(meta->numchains * sizeof(parities));
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
	meta->dataunits = rows * (cols - 2);
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

static void shortened_hcode_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parities *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks)) {
		fprintf(stderr, "invalid disk number using SH-Code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks;
	rows = meta->rows = meta->prime - 1;
	cols = meta->cols = meta->prime;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parities*) malloc(meta->numchains * sizeof(parities));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
		chain->type = 0;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p;
		chain->deps = NULL;
		for (c = 0; c < p - 1; c++)
			if (r != c) {
				elem = (element*) malloc(sizeof(element));
				elem->row = r;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}
	for (r = 0; r < rows; r++) {
		delta = (r + 1) % p; // delta = c - r
		chain = meta->chains + rows + r;
		chain->type = 1;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = r;
		chain->deps = NULL;
		for (c = 0; c < p - 1; c++)
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
	meta->dataunits = rows * (cols - 2);
	meta->totalunits = rows * cols;
	meta->entry = (struct entry_t*) malloc(meta->dataunits * sizeof(struct entry_t));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		if (c >= r) ++c;
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
	parities *chain;
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
	meta->chains = (parities*) malloc(meta->numchains * sizeof(parities));
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

static void liberation_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parities *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->numdisks)) {
		fprintf(stderr, "invalid disk number using Liberation code\n");
		exit(1);
	}
	p = meta->prime = meta->numdisks;
	rows = meta->rows = meta->numdisks;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parities*) malloc(meta->numchains * sizeof(parities));
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
		chain = meta->chains + rows + r;
		chain->type = 1;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p + 1;
		chain->deps = NULL;
		for (c = 0; c < p; c++) {
			elem = (element*) malloc(sizeof(element));
			elem->row = (r + c) % p;
			elem->col = c;
			elem->next = chain->deps;
			chain->deps = elem;
		}
	}
	for (c = 1; c < p; c++) {
		int y = c * (p - 1) / 2 % p;
		chain = meta->chains + rows + y;
		elem = (element*) malloc(sizeof(element));
		elem->row = (y + c - 1) % p;
		elem->col = c;
		elem->next = chain->deps;
		chain->deps = elem;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = rows * (cols - 2);
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

static void erasure_rebuild_init(metadata *meta) {
	int i, j, unitsize = meta->unitsize;
	element *elem;
	meta->numfailures = 0;
	meta->failed = (int*) malloc(sizeof(int) * meta->phydisks);
	memset(meta->failed, 0, sizeof(int) * meta->phydisks);
	meta->laststripe = -1;
	meta->chs = (parities***) malloc(sizeof(void*) * meta->totalunits);
	meta->chl = (int*) malloc(sizeof(int) * meta->totalunits);
	memset(meta->chl, 0, sizeof(int) * meta->totalunits);
	for (i = 0; i < meta->totalunits; i++)
		meta->chs[i] = (parities**) malloc(sizeof(void*) * 3);
	for (i = 0; i < meta->numchains; i++) {
		parities *chain = meta->chains + i;
		for (elem = chain->dest; elem != NULL; elem = elem->next) {
			int no = elem->row * meta->cols + elem->col;
			meta->chs[no][meta->chl[no]++] = chain;
		}
	}
	meta->test = (int*) malloc(sizeof(int) * meta->totalunits);
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
	case CODE_LIBERATION:
		liberation_initialize(meta);
		break;
	case CODE_SHCODE:
		shortened_hcode_initialize(meta);
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
		tab->hit[no] = 1;
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
	req->numxors = 0;
	req->numIOs = 0;
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
						rottable_update(ph1, elem->row, (elem->col + rot) % meta->cols, ll, rr);
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
						if (ph2->hit[nxt * meta->cols + dc]) {
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
		// calculate number of XORs
		if (req->flag == DISKSIM_WRITE) {
			// calculate the delta on each data element
			for (unitno = 0; unitno < meta->dataunits; unitno++) {
				int r = meta->entry[unitno].row;
				int dc = (meta->entry[unitno].col + rot) % meta->cols;
				int no = r * meta->cols + dc;
				if (ph1->hit[no])
					req->numxors++;
			}
			// calculate the delta on each parity element
			for (i = 0; i < meta->numchains; i++) {
				parities *chain = meta->chains + i;
				for (elem = chain->deps; elem != NULL; elem = elem->next) {
					int r = elem->row;
					int dc = (elem->col + rot) % meta->cols;
					int no = r * meta->cols + dc;
					if (ph2->hit[no])
						req->numxors++;
				}
			}
		}
		// calculate number of I/Os
		for (unitno = 0; unitno < meta->totalunits; unitno++)
			req->numIOs += ph1->hit[unitno] + ph2->hit[unitno];
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
