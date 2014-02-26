/*
 * disksim_erasure.c
 *
 *  Created on: Feb 22, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>

#include "disksim_erasure.h"
#include "disksim_logorg.h"

int check_prime(int number) {
	int i;
	for (i = 2; i * i <= number; i++)
		if (number % i == 0)
			return 0;
	return 1;
}

int use_erasure_code(int reduntype) {
	if (reduntype == RAID6_RDP || reduntype == RAID6_EVENODD ||
			reduntype == RAID6_HCODE || reduntype == RAID6_XCODE || reduntype == RAID7)
		return 1;
	else
		return 0;
}

static void evenodd_initialize(logorg *currlogorg) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity_chain *chain;
	element *elem;
	metadata *meta;

	meta = currlogorg->meta;
	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks - 2)) {
		fprintf(stderr, "invalid disk number using Row-Diagonal Parity code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks - 2;
	rows = meta->rows = meta->phydisks - 3;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parity_chain*) malloc(meta->numchains * sizeof(parity_chain));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
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
	meta->entry = (entry*) malloc(meta->dataunits * sizeof(entry));
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

static void rdp_initialize(logorg *currlogorg) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity_chain *chain;
	element *elem;
	metadata *meta;

	meta = currlogorg->meta;
	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->numdisks + 1)) {
		fprintf(stderr, "invalid disk number using EVENODD\n");
		exit(1);
	}
	p = meta->prime = meta->numdisks + 1;
	rows = meta->rows = meta->numdisks;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parity_chain*) malloc(meta->numchains * sizeof(parity_chain));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
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
		delta = r; // delta = r - c;
		chain = meta->chains + rows + r;
		chain->dest = (element*) malloc(sizeof(element));
		chain->dest->row = r;
		chain->dest->col = p;
		chain->deps = NULL;
		for (c = 0; c < p; c++)
			if ((c + delta + 1) % p) {
				elem = (element*) malloc(sizeof(element));
				elem->row = (c + delta) % p;
				elem->col = c;
				elem->next = chain->deps;
				chain->deps = elem;
			}
		chain->dest->next = chain->deps;
	}

	// map the data blocks to units in a stripe
	meta->dataunits = rows * meta->numdisks;
	meta->totalunits = rows * cols;
	meta->entry = (entry*) malloc(meta->dataunits * sizeof(entry));
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

static void hcode_initialize(logorg *currlogorg) {
	int i, r, c;
	int delta, sumrc;
	int p; // the prime number
	int rows, cols;
	int unitno, id;
	parity_chain *chain;
	element *elem;
	metadata *meta;

	meta = currlogorg->meta;
	meta->numdisks = meta->phydisks - 2;
	if (!check_prime(meta->phydisks - 1)) {
		fprintf(stderr, "invalid disk number using Row-Diagonal Parity code\n");
		exit(1);
	}
	p = meta->prime = meta->phydisks - 1;
	rows = meta->rows = meta->phydisks - 2;
	cols = meta->cols = meta->phydisks;
	// initialize parity chains
	meta->numchains = 2 * rows;
	meta->chains = (parity_chain*) malloc(meta->numchains * sizeof(parity_chain));
	for (r = 0; r < rows; r++) {
		chain = meta->chains + r;
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
	meta->entry = (entry*) malloc(meta->dataunits * sizeof(entry));
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

static void print(ioreq_event *curr) {
	ioreq_event *tmp = curr;
	depends *dep = (depends*) curr->prev, *t;
	int i;

	do {
		printf("time=%f devno=%d blkno=%d bcount=%d flag=%d opid=%d | id=%d prev=%d next=%d\n",
				tmp->time, tmp->devno, tmp->blkno, tmp->bcount, tmp->flags, tmp->opid,
				(int)tmp, (int)tmp->prev, (int)(tmp->next));
		tmp = tmp->next;
	} while (curr != tmp);

	while (dep != NULL) {
		printf(" (%d %d) affects", dep->devno, dep->blkno);
		t = dep;
		for (i = 0; i < dep->numdeps; i++) {
			tmp = t->deps[i % 10];
			printf(" (%d %d - %d)", tmp->devno, tmp->blkno, tmp->flags);
			if ((i + 1) % 10 == 0)
				t = t->cont;
		}
		printf("\n");
		dep = dep->next;
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

static void erasure_make_table(logorg *currlogorg) {
	metadata *meta;
	table_t *table = malloc(sizeof(table_t));
	int rot;
	int unitno, id, no;
	element *dep, *tmp;

	meta = currlogorg->meta;
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
	table->rottype = 0;
	table->dataunits = meta->dataunits * meta->phydisks;
	table->totalunits = meta->totalunits * meta->phydisks;
	table->entry = (entry*) malloc(table->dataunits * sizeof(entry));
	table->rows = meta->rows * meta->phydisks;
	table->cols = meta->cols;
	for (rot = 0; rot < meta->phydisks; rot++) {
		for (unitno = 0; unitno < meta->dataunits; unitno++) {
			no = rot * meta->dataunits + unitno;
			table->entry[no].row = meta->entry[unitno].row + rot * meta->rows;
			table->entry[no].col = (meta->entry[unitno].col + rot) % meta->phydisks;
			table->entry[no].depends = NULL;
			for (dep = meta->entry[unitno].depends; dep != NULL; dep = dep->next) {
				tmp = (element*) malloc(sizeof(element));
				tmp->row = dep->row + rot * meta->rows;
				tmp->col = (dep->col + rot) % meta->phydisks;
				tmp->next = table->entry[no].depends;
				table->entry[no].depends = tmp;
			}
		}
	}
	table->hit = malloc(table->totalunits * sizeof(int));
	table->hit_l = malloc(table->totalunits * sizeof(int));
	table->hit_r = malloc(table->totalunits * sizeof(int));
	table->reqs = malloc(table->dataunits * sizeof(void*));
	table->depend = malloc(table->dataunits * sizeof(void*));
	currlogorg->tab = table;
}

void erasure_initialize(logorg *currlogorg) {
	metadata *meta;
	meta = (metadata*) malloc(sizeof(metadata));
	meta->codetype = currlogorg->reduntype;
	meta->phydisks = currlogorg->actualnumdisks;
	currlogorg->meta = meta;
	currlogorg->maptype = ASIS;
	switch (meta->codetype) {
	case RAID6_RDP:
		rdp_initialize(currlogorg);
		break;
	case RAID6_EVENODD:
		evenodd_initialize(currlogorg);
		break;
	case RAID6_HCODE:
		hcode_initialize(currlogorg);
		break;
	default:
		fprintf(stderr, "unrecognized reduntype in erasure_initialize\n");
		exit(1);
	}
	erasure_gen_matrix(meta);
	erasure_make_table(currlogorg);
}

static void table_entry_update(table_t *table, int row, int col, int ll, int rr) {
	int no = row * table->cols + col;
	if (table->hit[no] == 0) {
		table->hit[no] = -1;
		table->hit_l[no] = ll;
		table->hit_r[no] = rr;
	} else {
		table->hit_l[no] = min(ll, table->hit_l[no]);
		table->hit_r[no] = max(rr, table->hit_r[no]);
	}
}

static void add_dependency(table_t *table, int id1, int id2) {
	depends *dep = table->depend[id1];
	int i, numdeps = dep->numdeps;
	for (i = 0; i < numdeps; i++) {
		if (dep->deps[i % 10] == table->reqs[id2]->prev)
			return;
		if ((i + 1) % 10 == 0) {
			if (i + 1 == numdeps)
				dep->cont = (depends*) getfromextraq();
			dep = dep->cont;
		}
	}
	table->depend[id1]->numdeps += 1;
	dep->deps[(i % 10)] = table->reqs[id2]->prev;
	table->reqs[id2]->prev->opid++;
}

int erasure_maprequest(logorg *currlogorg, ioreq_event *curr, int numreqs) {
	table_t *table = currlogorg->tab;
	metadata *meta = currlogorg->meta;
	int unitsize = currlogorg->stripeunit;
	int stacksize = table->dataunits * unitsize;
	int start = curr->blkno;
	int end = curr->blkno + curr->bcount;
	int stackno = start / stacksize;
	int unitno, no1, no2;
	int offset;
	int ll, rr;
	int idx, id, id2;
	int blkno;
	int r, c;
	int last;
	int i;
	depends *dlst = NULL, *tmpdep;
	element *dep;
	ioreq_event *lst = NULL, *temp;
	int THRESHOLD = 64;

	numreqs = 0;
	while (stackno * stacksize < end) {
		memset(table->hit, 0, sizeof(int) * table->totalunits);
		for (unitno = 0; unitno < table->dataunits; unitno++) {
			offset = stackno * stacksize + unitno * unitsize;
			ll = max(offset, start);
			rr = min(offset + unitsize, end);
			if (ll >= rr) continue;
			ll -= offset;
			rr -= offset; // range in stripe unit
			r = table->entry[unitno].row;
			c = table->entry[unitno].col;
			table_entry_update(table, r, c, ll, rr);
			if (!(curr->flags & READ))
				for (dep = table->entry[unitno].depends; dep != NULL; dep = dep->next)
					table_entry_update(table, dep->row, dep->col, ll, rr);
		}
		idx = 0;
		blkno = stackno * table->rows * unitsize;
		for (c = 0; c < table->cols; c++) {
			last = -1;
			for (r = 0; r < table->rows; r++) {
				no1 = r * table->cols + c;
				if (table->hit[no1] == -1) {
					ll = blkno + r * unitsize + table->hit_l[no1];
					rr = blkno + r * unitsize + table->hit_r[no1];
					if (last != -1 && ll - last < THRESHOLD) {
						// merge requests
						table->hit[no1] = idx;
						table->reqs[idx]->bcount = rr - table->reqs[idx]->blkno;
					} else {
						table->hit[no1] = ++idx;
						temp = ioreq_copy(curr);
						temp->devno = c;
						temp->blkno = ll;
						temp->bcount = rr - ll;
						temp->next = NULL;
						temp->prev = NULL;
						table->reqs[idx] = temp;
					}
					last = rr;
				}
			}
		}
		if (curr->flags & READ) {
			for (i = 1; i <= idx; i++) {
				table->reqs[i]->next = lst;
				lst = table->reqs[i];
				numreqs++;
			}
		} else {
			// add dependency
			for (i = 1; i <= idx; i++) {
				table->reqs[i]->prev = ioreq_copy(table->reqs[i]);
				table->reqs[i]->flags = curr->flags | READ;
				table->reqs[i]->prev->opid = 0;
				tmpdep = (depends*) getfromextraq();
				tmpdep->devno = table->reqs[i]->devno;
				tmpdep->blkno = table->reqs[i]->blkno;
				tmpdep->numdeps = 1;
				tmpdep->deps[0] = table->reqs[i]->prev;
				table->reqs[i]->prev->opid++;
				tmpdep->cont = NULL;
				table->depend[i] = tmpdep;
			}
			for (unitno = 0; unitno < table->dataunits; unitno++) {
				r = table->entry[unitno].row;
				c = table->entry[unitno].col;
				id = table->hit[r * table->cols + c];
				if (id < 1) continue;
				for (dep = table->entry[unitno].depends; dep != NULL; dep = dep->next) {
					id2 = table->hit[dep->row * table->cols + dep->col];
					add_dependency(table, id, id2);
				}
			}
			for (i = 1; i <= idx; i++) {
				table->reqs[i]->prev = NULL;
				table->reqs[i]->next = lst;
				lst = table->reqs[i];
				table->depend[i]->next = dlst;
				dlst = table->depend[i];
				numreqs++;
			}
			numreqs += idx;
		}
		stackno += 1;
	}
	if (lst == NULL) {
		fprintf(stderr, "something wrong in erasure_maprequest\n");
		exit(1);
	}
	*curr = *lst;
	addtoextraq((event*)lst);
	curr->prev = (ioreq_event*) dlst;
	for (lst = curr; lst->next != NULL; lst = lst->next);
	lst->next = curr;
	return numreqs;
}
