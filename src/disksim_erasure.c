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

#define THRESHOLD	16

static codespec specs[32];
static int ncodes = 0, codeid;

const char* get_code_name(int code) {
	for (codeid = 0; codeid < ncodes; codeid++)
		if (code == specs[codeid].codeID)
			return specs[codeid].name;
	fprintf(stderr, "invalid code ID: %d\n", code);
	exit(-1);
	return "";
}

int get_code_id(const char *flag) {
	for (codeid = 0; codeid < ncodes; codeid++)
		if (!strcmp(flag, specs[codeid].flag))
			return specs[codeid].codeID;
	fprintf(stderr, "invalid code name: %s\n", flag);
	exit(-1);
	return -1;
}

void add_to_ioreq(ioreq *req, struct disksim_request *tmp) {
	tmp->next = NULL;
	if (req->reqs == NULL)
		req->reqs = req->tail = tmp;
	else {
		req->tail->next = tmp;
		req->tail = tmp;
	}
	req->numreqs++;
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
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks - 2)) {
		fprintf(stderr, "invalid disk number using %s\n",
				get_code_name(meta->codetype));
		exit(1);
	}
	p = meta->prime = meta->phydisks - 2;
	rows = meta->rows = meta->phydisks - 3;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void rdp_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->numdisks + 1)) {
		fprintf(stderr, "invalid disk number using %s\n",
				get_code_name(meta->codetype));
		exit(1);
	}
	p = meta->prime = meta->numdisks + 1;
	rows = meta->rows = meta->numdisks;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void hcode_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
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
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		if (c > r) ++c;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void shortened_hcode_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks)) {
		fprintf(stderr, "invalid disk number using Shortened H-Code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks;
	rows = meta->rows = meta->prime - 1;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
		chain->type = 0;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p - 1;
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
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		if (c >= r) ++c;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void xcode_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
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
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->cols;
		c = unitno % meta->cols;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void liberation_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
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
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void ext_hcode_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 3;
	if (!check_prime(meta->phydisks - 2)) {
		fprintf(stderr, "invalid disk number using extended H-Code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks - 2;
	rows = meta->rows = meta->prime - 1;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 3 * rows;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
	for (r = 0; r < rows; r++) {
		sumrc = r;
		chain = meta->chains + rows * 2 + r;
		chain->type = 2;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p + 1;
		chain->deps = NULL;
		for (c = 0; c < p; c++) {
			int r1 = (sumrc + p - c) % p;
			if (r1 + 1 != c) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (r1 + 1 == p ? c - 1 : r1);
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		}
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = rows * (cols - 3);
	meta->totalunits = rows * cols;
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		if (c > r) ++c;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void star_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 3;
	if (!check_prime(meta->phydisks - 3)) {
		fprintf(stderr, "invalid disk number using STAR code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks - 3;
	rows = meta->rows = meta->prime - 1;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 3 * rows;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
	for (r = 0; r < rows; r++) {
		chain = meta->chains + rows * 2 + r;
		chain->type = 2;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p + 2;
		chain->deps = NULL;
		for (c = 0; c < p; c++)
			if ((r + c + 1) % p) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (r + c) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		for (c = 1; c < p; c++) {
			elem = (element*) malloc(sizeof(element));
			elem->row = c - 1;
			elem->col = c;
			elem->next = chain->deps;
			chain->deps = elem;
		}
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = rows * (cols - 3);
	meta->totalunits = rows * cols;
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void triple_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 3;
	if (!check_prime(meta->numdisks + 1)) {
		fprintf(stderr, "invalid disk number using Triple Parity\n");
		exit(1);
	}
	p = meta->prime = meta->numdisks + 1;
	rows = meta->rows = meta->numdisks;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 3 * rows;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
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
			if ((r - c + p + 1) % p) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (r - c + p) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}
	for (r = 0; r < rows; r++) {
		chain = meta->chains + rows * 2 + r;
		chain->type = 2;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p + 1;
		chain->deps = NULL;
		for (c = 0; c < p; c++)
			if ((r + c + 1) % p) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (r + c) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = rows * (cols - 3);
	meta->totalunits = rows * cols;
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void pcode_initialize(metadata *meta) {
	int i, j, r, c;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks + 1)) {
		fprintf(stderr, "invalid disk number using P-Code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks + 1;
	rows = meta->rows = (meta->prime - 1) / 2;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = cols;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
	for (c = 0; c < p - 1; c++) {
		chain = meta->chains + c;
		chain->type = 0;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = 0;
		chain->dest->col = c;
		chain->deps = NULL;
	}
	int *rno = malloc(sizeof(int) * cols);
	memset(rno, 0, sizeof(int) * cols);
	for (i = 0; i < p - 1; i++)
		for (j = i + 1; j < p - 1; j++) {
			c = (i + j + 1) % p;
			if (c == p - 1) continue;
			r = ++rno[c];
			chain = meta->chains + i;
			elem = (element*) malloc(sizeof(element));
			elem->row = r;
			elem->col = c;
			elem->next = chain->deps;
			chain->deps = elem;
			chain = meta->chains + j;
			elem = (element*) malloc(sizeof(element));
			elem->row = r;
			elem->col = c;
			elem->next = chain->deps;
			chain->deps = elem;
		}
	for (c = 0; c < p - 1; c++) {
		chain = meta->chains + c;
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = (rows - 1) * cols;
	meta->totalunits = rows * cols;
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->cols + 1;
		c = unitno % meta->cols;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void code56_initialize(metadata *meta) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks)) {
		fprintf(stderr, "invalid disk number using Code 5-6\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks;
	rows = meta->rows = meta->prime - 1;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
		chain->type = 0; // row parity
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = r;
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
		chain = meta->chains + rows + r;
		chain->type = 1; // diagonal parity
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p - 1;
		chain->deps = NULL;
		for (c = 0; c < p - 1; c++)
			if ((r + c + 2) % p) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (r + c + 1) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}
	// map the data blocks to units in a stripe
	meta->dataunits = rows * (cols - 2);
	meta->totalunits = rows * cols;
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->numdisks;
		c = unitno % meta->numdisks;
		if (r <= c) c++;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void cyclic_initialize(metadata *meta) {
	int i, j, r, c;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks + 1)) {
		fprintf(stderr, "invalid disk number using Cyclic code\n");
		exit(-1);
	}
	p = meta->prime = meta->phydisks + 1;
	rows = meta->rows = (meta->prime - 1) / 2;
	cols = meta->cols = meta->phydisks;

	meta->numchains = cols;
	meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
	for (c = 0; c < p - 1; c++) {
		chain = meta->chains + c;
		chain->type = 0;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = 0;
		chain->dest->col = c;
		chain->deps = NULL;
	}

	int alpha = -1, beta = -1, k = 2;
	for (i = 2; i < p; i++) {
		int a = i;
		for (j = 2; a != 1 && j < p; j++) {
			a = a * i % p;
			if (j == k && a == 1)
				alpha = i;
			if (j == p - 1 && a == 1 && beta < 0)
				beta = i;
		}
	}
	int *perm = (int*) malloc(sizeof(int) * cols);
	for (i = 0; i < p - 1; i++) {
		int a = 1;
		for (j = 0; j < i; j++)
			a = a * beta % p;
		perm[(a + p - 1) % p] = i;
	}
	int ll = 1, rr = alpha, rowno = 1;
	for (i = 0; i < rows; i++) {
		if (ll < p - 1 && rr < p - 1) {
			for (c = 0; c < cols; c++) {
				chain = meta->chains + (perm[ll] + c) % cols;
				elem = (element*) malloc(sizeof(element));
				elem->row = rowno;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
				chain = meta->chains + (perm[rr] + c) % cols;
				elem = (element*) malloc(sizeof(element));
				elem->row = rowno;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
			rowno += 1;
		}
		ll = ll * beta % p;
		rr = rr * beta % p;
	}
	for (i = 0; i < cols; i++) {
		chain = meta->chains + i;
		chain->dest->next = chain->deps;
	}
	free(perm);
	meta->dataunits = (rows - 1) * cols;
	meta->totalunits = rows * cols;
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (unitno = 0; unitno < meta->dataunits; unitno++) {
		r = unitno / meta->cols + 1;
		c = unitno % meta->cols;
		meta->map[unitno].row = r;
		meta->map[unitno].col = c;
		meta->map[unitno].next = NULL;
		meta->rmap[r * meta->cols + c] = unitno;
	}
}

static void raid5_initialize(metadata *meta) {
	int r, c, i, p;
	int rows, cols;
	parity *chain;
	element *elem;

	meta->numdisks = meta->phydisks - 1;
	p = meta->prime = meta->phydisks;
	rows = meta->rows = 1;
	cols = meta->cols = meta->phydisks;

	meta->numchains = 1;
	chain = meta->chains = (parity*) malloc(meta->numchains * sizeof(parity));
	chain->type = 0;
	chain->dest = (element*) malloc(sizeof(element));
	chain->dest->row = 0;
	chain->dest->col = cols - 1;
	chain->deps = NULL;
	for (c = 0; c < cols - 1; c++) {
		elem = (element*) malloc(sizeof(element));
		elem->row = 0;
		elem->col = c;
		elem->next = chain->deps;
		chain->deps = elem;
	}
	chain->dest->next = chain->deps;
}

static void erasure_make_table(metadata *meta) {
	int i, j, unitno = 0;
	element *elem;
	parity *chain;
	meta->dataunits = meta->rows * meta->numdisks;
	meta->totalunits = meta->rows * meta->phydisks;
	meta->stripesize = meta->dataunits * meta->unitsize;
	meta->map = (element*) malloc(meta->dataunits * sizeof(element));
	meta->rmap = (int*) malloc(meta->totalunits * sizeof(int));
	memset(meta->rmap, -1, meta->totalunits * sizeof(int));
	for (i = 0; i < meta->numchains; i++) {
		elem = (meta->chains + i)->dest;
		meta->rmap[elem->row * meta->cols + elem->col] = -2; // parity blocks
	}
	for (i = 0; i < meta->totalunits; i++)
		if (meta->rmap[i] == -1) {
			meta->map[unitno].row = i / meta->cols;
			meta->map[unitno].col = i % meta->cols;
			meta->map[unitno].next = NULL;
			meta->rmap[i] = unitno++;
		}
	assert(unitno == meta->dataunits);
	int *depend = (int*) malloc(meta->numchains * sizeof(int));
	int *occur = (int*) malloc(meta->totalunits * sizeof(int));
	memset(depend, 0, meta->numchains * sizeof(int));
	memset(occur, 0, meta->totalunits * sizeof(int));
	for (i = 0; i < meta->numchains; i++)
		for (elem = (meta->chains + i)->deps; elem != NULL; elem = elem->next)
			if (meta->rmap[elem->row * meta->cols + elem->col] < 0)
				depend[i] += 1;
	for (i = 0; i < meta->numchains; i++) {
		int ch = 0;
		while (ch < meta->numchains && depend[ch] != 0) ch++;
		if (ch == meta->numchains) {
			fprintf(stderr, "dependency error in parity chains!\n");
			exit(0);
		}
		depend[ch]--;
		chain = meta->chains + ch;
		for (elem = chain->deps; elem != NULL; elem = elem->next)
			occur[elem->row * meta->cols + elem->col] ^= 1;
		for (j = 0; j < meta->dataunits; j++) {
			int protect = 0;
			for (elem = meta->map + j; elem != NULL; elem = elem->next)
				protect ^= occur[elem->row * meta->cols + elem->col];
			if (protect) {
				elem = (element*) malloc(sizeof(element));
				elem->row = chain->dest->row;
				elem->col = chain->dest->col;
				elem->next = meta->map[j].next;
				meta->map[j].next = elem;
			}
		}
		for (elem = chain->deps; elem != NULL; elem = elem->next)
			occur[elem->row * meta->cols + elem->col] ^= 1;
		int currid = chain->dest->row * meta->cols + chain->dest->col;
		for (j = 0; j < meta->numchains; j++)
			for (elem = (meta->chains + j)->deps; elem != NULL; elem = elem->next)
				if (elem->row * meta->cols + elem->col == currid)
					depend[j] -= 1;
	}
	free(depend);
	free(occur);
}

static void erasure_init_rottable(metadata *meta) {
	int totalsize = meta->totalunits * sizeof(int);
	meta->ph1 = (rottable*) malloc(sizeof(rottable));
	meta->ph1->rows = meta->rows;
	meta->ph1->cols = meta->cols;
	meta->ph1->hit = (int*) malloc(totalsize);
	meta->ph1->ll = (int*) malloc(totalsize);
	meta->ph1->rr = (int*) malloc(totalsize);
	meta->ph1->map = meta->map;
	meta->ph2 = (rottable*) malloc(sizeof(rottable));
	meta->ph2->rows = meta->rows;
	meta->ph2->cols = meta->cols;
	meta->ph2->hit = (int*) malloc(totalsize);
	meta->ph2->ll = (int*) malloc(totalsize);
	meta->ph2->rr = (int*) malloc(totalsize);
	meta->ph2->map = meta->map;
}

static void create_code(int code, const char *flag, const char *name, initializer func) {
	specs[ncodes].codeID = code;
	specs[ncodes].flag = flag;
	specs[ncodes].name = name;
	specs[ncodes].func = func;
	ncodes++;
}

void erasure_initialize() {
	create_code(CODE_RDP, "rdp", "RDP", rdp_initialize);
	create_code(CODE_EVENODD, "evenodd", "EVENODD", evenodd_initialize);
	create_code(CODE_HCODE, "hcode", "H-code", hcode_initialize);
	create_code(CODE_XCODE, "xcode", "X-code", xcode_initialize);
	create_code(CODE_LIBERATION, "liberation", "Liberation", liberation_initialize);
	create_code(CODE_PCODE, "pcode", "P-code", pcode_initialize);
	create_code(CODE_CYCLIC, "cyclic", "Cyclic", cyclic_initialize);
	create_code(CODE_CODE56, "code56", "Code.5-6", code56_initialize);
	create_code(CODE_STAR, "star", "STAR", star_initialize);
	create_code(CODE_TRIPLE, "triple", "Triple-Star", triple_initialize);
	create_code(CODE_EXT_HCODE, "exthcode", "Extended.H-code", ext_hcode_initialize);
	create_code(CODE_RAID5, "raid5", "RAID-5", raid5_initialize);
}



void erasure_init_code(metadata *meta, int codetype, int disks, int unitsize) {
	meta->codetype = codetype;
	meta->phydisks = disks;
	meta->unitsize = unitsize;
	for (codeid = 0; codeid < ncodes; codeid++)
		if (meta->codetype == specs[codeid].codeID) {
			specs[codeid].func(meta);
			erasure_make_table(meta);
			erasure_init_rottable(meta);
			return;
		}
	fprintf(stderr, "unrecognized reduntype in erasure_init_code()\n");
	exit(1);
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

void erasure_standard_maprequest(metadata *meta, ioreq *req) {
	int unitsize = meta->unitsize;
	int stripesize = meta->dataunits * unitsize;
	int start = req->blkno;
	int end = req->blkno + req->bcount;
	int stripeno = start / stripesize;
	element *elem;
	rottable *ph1 = meta->ph1;
	rottable *ph2 = meta->ph2;
	int unitno, llim, rlim, i, no;

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
			int r = meta->map[unitno].row;
			int dc = (meta->map[unitno].col + rot) % meta->cols;
			rottable_update(ph1, r, dc, ll, rr);
			if (req->flag == DISKSIM_WRITE) {
				for (elem = meta->map[unitno].next; elem != NULL; elem = elem->next) {
					rottable_update(ph1, elem->row, (elem->col + rot) % meta->cols, ll, rr);
					rottable_update(ph2, elem->row, (elem->col + rot) % meta->cols, ll, rr);
				}
				rottable_update(ph2, r, dc, ll, rr);
			} else { // degraded read/write
				no = r * meta->cols + dc;
				if (req->flag == DISKSIM_WRITE) // same as not missing
					for (elem = meta->map[unitno].next; elem != NULL; elem = elem->next) {
						rottable_update(ph1, elem->row, (elem->col + rot) % meta->cols, ll, rr);
						rottable_update(ph2, elem->row, (elem->col + rot) % meta->cols, ll, rr);
					}
			}
		}
		int r, nxt, dc;
		struct disksim_request *tmp = (struct disksim_request*) getfromextraq();
		for (dc = 0; dc < meta->cols; dc++) {
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
					tmp->reqctx = req;
					add_to_ioreq(req, tmp);
				} else nxt = r + 1;
		}
		for (dc = 0; dc < meta->cols; dc++) {
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
					tmp->reqctx = req;
					add_to_ioreq(req, tmp);
				} else nxt = r + 1;
		}
		stripeno++;
		// calculate number of XORs
		if (req->flag == DISKSIM_WRITE) {
			// calculate the delta on each data element
			for (unitno = 0; unitno < meta->dataunits; unitno++) {
				int r = meta->map[unitno].row;
				int dc = (meta->map[unitno].col + rot) % meta->cols;
				int no = r * meta->cols + dc;
				if (ph1->hit[no])
					req->numXORs++;
			}
			// calculate the delta on each parity element
			for (i = 0; i < meta->numchains; i++) {
				parity *chain = meta->chains + i;
				for (elem = chain->deps; elem != NULL; elem = elem->next) {
					int r = elem->row;
					int dc = (elem->col + rot) % meta->cols;
					int no = r * meta->cols + dc;
					if (ph2->hit[no])
						req->numXORs++;
				}
			}
		}
		// calculate number of I/Os
		for (unitno = 0; unitno < meta->totalunits; unitno++)
			req->numIOs += ph1->hit[unitno] + ph2->hit[unitno];
	}
}
