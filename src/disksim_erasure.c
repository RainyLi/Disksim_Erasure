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
#include "disksim_malloc.h"
#include "disksim_req.h"
#include "disksim_reqflags.h"

static codespec_t specs[32];
static int nr_codes = 0, code_id;

extern int el_idx, sb_idx;

const char* get_code_name(int code)
{
	for (code_id = 0; code_id < nr_codes; code_id++)
		if (code == specs[code_id].codeID)
			return specs[code_id].name;
	fprintf(stderr, "invalid code ID: %d\n", code);
	exit(-1);
	return "";
}

int get_code_id(const char *flag)
{
	for (code_id = 0; code_id < nr_codes; code_id++)
		if (!strcmp(flag, specs[code_id].flag))
			return specs[code_id].codeID;
	fprintf(stderr, "invalid code name: %s\n", flag);
	exit(-1);
	return -1;
}

static int check_prime(int number)
{
	int i;
	for (i = 2; i * i <= number; i++)
		if (number % i == 0)
			return 0;
	return 1;
}

static element_t* create_elem(int row, int col, element_t *next)
{
	element_t *ret = (element_t*) disksim_malloc(el_idx);
	ret->row = row;
	ret->col = col;
	ret->next = next;
	return ret;
}

static int evenodd_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 2;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r + p - c + 1) % p)
				elem = create_elem((r + p - c) % p, c, elem);
		for (c = 1; c < p; c++)
			elem = create_elem(p - 1 - c, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p + 1, elem);
	}
	return 0;
}

static int rdp_initialize(metadata_t *meta)
{
	int r, c, p = meta->k + 1;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p - 1, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r - c + p + 1) % p)
				elem = create_elem((r - c + p) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p, elem);
	}
	return 0;
}

static int hcode_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 1;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if (r + 1 != c)
				elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((c + p - r - 1) % p)
				elem = create_elem((c + p - r - 2) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, r + 1, elem);
	}
	return 0;
}

static int shortened_hcode_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 2;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			if (r != c)
				elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p - 1, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			if ((c + p - r) % p)
				elem = create_elem((c + p - r - 1) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, r, elem);
	}
	return 0;
}

static int xcode_initialize(metadata_t *meta) {
	int r, c, p = meta->pr = meta->n;
	if (!check_prime(p)) return -1;
	meta->w = p;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (c = 0; c < p; c++) {
		element_t *elem = NULL;
		for (r = 0; r < p - 2; r++)
			elem = create_elem(r, (r + c + 2) % p, elem);
		meta->chains[r] = create_elem(p - 2, c, elem);
	}
	for (c = 0; c < p; c++) {
		element_t *elem = NULL;
		for (r = 0; r < p - 2; r++)
			elem = create_elem(r, (c + p - r - 2) % p, elem);
		meta->chains[meta->w + r] = create_elem(p - 1, c, elem);
	}
	return 0;
}

static int liberation_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 2;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (c = 1; c < p; c++) {
		int y = c * (p - 1) / 2 % p;
		meta->chains[meta->w + y] = create_elem((y + c - 1) % p, c, NULL);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = meta->chains[meta->w + r];
		for (c = 0; c < p; c++)
			elem = create_elem((r + c) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p + 1, elem);
	}
	return 0;
}

static int ext_hcode_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 2;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if (r + 1 != c)
				elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((c + p - r - 1) % p)
				elem = create_elem((c + p - r - 2) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, r + 1, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++) {
			int r1 = (r + p - c) % p;
			if (r1 + 1 != c)
				elem = create_elem((r1 + 1 == p ? c - 1 : r1), c, elem);
			meta->chains[meta->w * 2 + r] = create_elem(r, p + 1, elem);
		}
	}
	return 0;
}

static int star_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 3;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 1; c < p; c++)
			elem = create_elem(p - 1 - c, c, elem);
		for (c = 0; c < p; c++)
			if ((r + p - c + 1) % p)
				elem = create_elem((r + p - c) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p + 1, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 1; c < p; c++)
			elem = create_elem(c - 1, c, elem);
		for (c = 0; c < p; c++)
			elem = create_elem((r + c) % p, c, elem);
		meta->chains[meta->w * 2 + r] = create_elem(r, p + 2, elem);
	}
	return 0;
}

static int triple_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 2;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p - 1, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r + p - c + 1) % p)
				elem = create_elem((r + p - c) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r + c + 1) % p)
				elem = create_elem((r + c) % p, c, elem);
		meta->chains[meta->w * 2 + r] = create_elem(r, p + 1, elem);
	}
	return 0;
}

static int pcode_initialize(metadata_t *meta)
{
	int i, j, r, c, p = meta->pr = meta->n + 1;
	if (!check_prime(p)) return -1;
	meta->w = meta->n / 2;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	int *rno = malloc(sizeof(int) * meta->n);
	memset(rno, 0, sizeof(int) * meta->n);
	for (i = 0; i < p - 1; i++)
		for (j = i + 1; j < p - 1; j++) {
			c = (i + j + 1) % p;
			if (c == p - 1) continue;
			r = ++rno[c];
			meta->chains[i] = create_elem(r, c, meta->chains[i]);
			meta->chains[j] = create_elem(r, c, meta->chains[j]);
		}
	free(rno);
	for (c = 0; c < p - 1; c++)
		meta->chains[c] = create_elem(0, c, meta->chains[c]);
	return 0;
}

static int code56_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			if (r != c)
				elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, r, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			if ((r + c + 2) % p)
				elem = create_elem((r + c + 1) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p - 1, elem);
	}
	return 0;
}

static int cyclic_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n + 1;
	if (!check_prime(p)) return -1;
	meta->w = meta->n / 2;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	int alpha = -1, beta = -1, i, j, k = 2;
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
	int *perm = (int*) malloc(sizeof(int) * meta->n);
	for (i = 0; i < p - 1; i++) {
		int a = 1;
		for (j = 0; j < i; j++)
			a = a * beta % p;
		perm[(a + p - 1) % p] = i;
	}
	int ll = 1, rr = alpha, rowno = 1;
	for (i = 0; i < meta->w; i++) {
		if (ll < p - 1 && rr < p - 1) {
			for (c = 0; c < meta->n; c++) {
				int x = (perm[ll] + c) % meta->n;
				int y = (perm[rr] + c) % meta->n;
				meta->chains[x] = create_elem(rowno, c, meta->chains[x]);
				meta->chains[y] = create_elem(rowno, c, meta->chains[y]);
			}
			rowno += 1;
		}
		ll = ll * beta % p;
		rr = rr * beta % p;
	}
	free(perm);
	for (c = 0; c < p - 1; c++)
		meta->chains[c] = create_elem(0, c, meta->chains[c]);
	return 0;
}

static int raid5_initialize(metadata_t *meta)
{
	int c;
	meta->w = 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (c = 0l; c < meta->n; c++)
		meta->chains[0] = create_elem(0, c, meta->chains[0]);
	return 0;
}

static int erasure_make_table(metadata_t *meta)
{
	int i, j, units = 0;
	element_t *elem;
	int nr_data = meta->w * meta->k;
	int nr_total = meta->w * meta->n;
	int nr_parity = meta->w * meta->m;
	meta->map = (element_t*) malloc(nr_data * sizeof(element_t));
	meta->page = (page_t*) malloc(nr_total * sizeof(page_t));
	int *parity = (int*) malloc(nr_total * sizeof(int));
	memset(parity, 0, nr_total * sizeof(int));
	for (i = 0; i < nr_parity; i++) {
		elem = meta->chains[i];
		parity[elem->row * meta->n + elem->col] = 1; // parity blocks
	}
	for (i = 0; i < nr_total; i++)
		if (!parity[i]) {
			meta->map[units].col = i % meta->n;
			meta->map[units].row = i / meta->n;
			meta->map[units].next = NULL;
			units += 1;
		}
	if (units != nr_data) return -1;
	int *depend = (int*) malloc(nr_parity * sizeof(int));
	int *occur  = (int*) malloc(nr_total  * sizeof(int));
	memset(depend, 0, nr_parity * sizeof(int));
	memset(occur , 0, nr_total  * sizeof(int));
	for (i = 0; i < nr_parity; i++)
		for (elem = meta->chains[i]->next; elem != NULL; elem = elem->next)
			if (parity[elem->row * meta->n + elem->col])
				depend[i] += 1;
	for (i = 0; i < nr_parity; i++) {
		int ch = 0;
		while (ch < nr_parity && depend[ch] != 0) ch++;
		if (ch == nr_parity) return -1;
		depend[ch]--;
		element_t *head = meta->chains[ch];
		for (elem = head->next; elem != NULL; elem = elem->next)
			occur[elem->row * meta->n + elem->col] ^= 1;
		for (j = 0; j < nr_data; j++) {
			int protect = 0;
			for (elem = meta->map + j; elem != NULL; elem = elem->next)
				protect ^= occur[elem->row * meta->n + elem->col];
			if (protect)
				meta->map[j].next = create_elem(head->row, head->col, meta->map[j].next);
		}
		for (elem = head->next; elem != NULL; elem = elem->next)
			occur[elem->row * meta->n + elem->col] ^= 1;
		int currid = head->row * meta->n + head->col;
		for (j = 0; j < nr_parity; j++)
			for (elem = meta->chains[j]->next; elem != NULL; elem = elem->next)
				if (elem->row * meta->n + elem->col == currid)
					depend[j] -= 1;
	}
	free(parity);
	free(depend);
	free(occur);
	return 0;
}

static void create_code(int code, int level, const char *flag, const char *name, initializer_t func)
{
	specs[nr_codes].codeID = code;
	specs[nr_codes].level = level;
	specs[nr_codes].flag = flag;
	specs[nr_codes].name = name;
	specs[nr_codes].init = func;
	nr_codes++;
}

void erasure_initialize()
{
	create_code(CODE_RAID5, 1,"raid5", "RAID-5", raid5_initialize);
	create_code(CODE_RDP, 2, "rdp", "RDP", rdp_initialize);
	create_code(CODE_EVENODD, 2, "evenodd", "EVENODD", evenodd_initialize);
	create_code(CODE_HCODE, 2, "hcode", "H-code", hcode_initialize);
	create_code(CODE_XCODE, 2, "xcode", "X-code", xcode_initialize);
	create_code(CODE_LIBERATION, 2, "liberation", "Liberation", liberation_initialize);
	create_code(CODE_PCODE, 2, "pcode", "P-code", pcode_initialize);
	create_code(CODE_CYCLIC, 2, "cyclic", "Cyclic", cyclic_initialize);
	create_code(CODE_CODE56, 2, "code56", "Code.5-6", code56_initialize);
	create_code(CODE_STAR, 3, "star", "STAR", star_initialize);
	create_code(CODE_TRIPLE, 3, "triple", "Triple-Star", triple_initialize);
	create_code(CODE_EXT_HCODE, 3, "exthcode", "Extended.H-code", ext_hcode_initialize);
}

void erasure_iocomplete(double time, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	metadata_t *meta = (metadata_t*) subreq->meta;
	if ((--subreq->out_reqs) == 0) {
		// check state & reqflag
		if ((subreq->flag & DISKSIM_READ) || subreq->state == STATE_WRITE) {
			sh_release_stripe(time, meta->sctlr, subreq->stripeno);
			ioreq_t *req = (ioreq_t*) subreq->reqctx;
			if ((--req->out_reqs) == 0)
				meta->comp_fn(time, (ioreq_t*) subreq->reqctx, NULL);
		} else
			erasure_maprequest(time, subreq, sh);
	}
}

void erasure_maprequest(double time, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	subreq->state += 1;
	subreq->out_reqs = 1;
	metadata_t *meta = (metadata_t*) subreq->meta;
	int usize = meta->usize;
	int begin = subreq->blkno, end = begin + subreq->bcount;
	if (subreq->flag & DISKSIM_READ) {
		int unitno = subreq->blkno / meta->usize, lo = unitno * usize;
		while (lo < end && lo + usize > begin) {
			sh_request_t *shreq = (sh_request_t*) disksim_malloc(sb_idx);
			shreq->time = time;
			shreq->flag = subreq->flag;
			shreq->stripeno = subreq->stripeno;
			shreq->devno = meta->map[unitno].col;
			shreq->blkno = meta->map[unitno].row;
			shreq->v_begin = max(0, begin - lo);
			shreq->v_end = min(usize, end - lo);
			shreq->reqctx = (void*) subreq;
			shreq->meta = (void*) meta->sctlr;
			subreq->out_reqs += 1;
			sh_request_arrive(time, meta->sctlr, sh, shreq);
			unitno += 1;
			lo += usize;
		}
	} else {
		int nr_total = meta->n * meta->w, unit_id;
		int unitno = subreq->blkno / meta->usize, lo = unitno * usize;
		memset(meta->page, 0, sizeof(page_t) * nr_total);
		while (lo < end && lo + usize > begin) {
			element_t *elem;
			int vb = max(0, begin - lo), ve = min(usize, end - lo);
			for (elem = meta->map + unitno; elem; elem = elem->next) {
				page_t *pg = meta->page + unitno;
				if (pg->state) {
					pg->v_begin = min(pg->v_begin, vb);
					pg->v_end = max(pg->v_end, ve);
				} else {
					pg->state = 1;
					pg->v_begin = vb;
					pg->v_end = ve;
				}
			}
			unitno += 1;
			lo += usize;
		}
		int flag = subreq->flag;
		if (subreq->state == STATE_READ)
			flag |= DISKSIM_READ;
		for (unit_id = 0; unit_id < nr_total; unit_id++) {
			page_t *pg = meta->page + unit_id;
			if (meta->page[unit_id].state) {
				sh_request_t *shreq = (sh_request_t*) disksim_malloc(sb_idx);
				shreq->time = time;
				shreq->flag = flag;
				shreq->stripeno = subreq->stripeno;
				shreq->devno = unit_id % meta->n;
				shreq->blkno = unit_id / meta->n;
				shreq->v_begin = pg->v_begin;
				shreq->v_end = pg->v_end;
				shreq->reqctx = (void*) subreq;
				shreq->meta = (void*) meta->sctlr;
				subreq->out_reqs += 1;
				sh_request_arrive(time, meta->sctlr, sh, shreq);
			}
		}
	}
	erasure_iocomplete(time, subreq, sh);
}

void erasure_code_init(metadata_t *meta, int codetype, int disks, int usize, erasure_complete_t comp)
{
	meta->codetype = codetype;
	meta->n = disks;
	meta->m = specs[code_id].level;
	meta->k = meta->n - meta->m;
	meta->usize = usize;
	meta->comp_fn = comp;
	for (code_id = 0; code_id < nr_codes; code_id++)
		if (meta->codetype == specs[code_id].codeID) {
			if (specs[code_id].init(meta) < 0) {
				fprintf(stderr, "invalid disk number using %s\n", specs[code_id].name);
				exit(-1);
			}
			erasure_make_table(meta);
			return;
		}
	fprintf(stderr, "unrecognized codetype in erasure_init_code()\n");
	exit(-1);
	meta->sctlr = (stripe_ctlr_t*) malloc(sizeof(stripe_ctlr_t));
	sh_init(meta->sctlr, 256, disks, meta->w, usize);
	sh_set_mapreq_callback(meta->sctlr, erasure_maprequest);
	sh_set_complete_callback(meta->sctlr, erasure_iocomplete);
}

void erasure_handle_request(double time, metadata_t *meta, ioreq_t *req)
{
	req->out_reqs = 1;
	int end = req->blkno + req->bcount;
	int stripe_sz = meta->k * meta->w * meta->usize;
	int stripe_no = req->blkno / stripe_sz;
	int curr = stripe_no * stripe_sz;
	while (curr < end) {
		sub_ioreq_t *subreq = (sub_ioreq_t*) disksim_malloc(sb_idx);
		subreq->state = STATE_BEGIN;
		subreq->reqno = req->reqno;
		subreq->stripeno = stripe_no;
		subreq->blkno = max(curr, req->blkno) - curr;
		subreq->bcount = min(curr + stripe_sz, end) - subreq->blkno;
		subreq->reqctx = req;
		subreq->meta = (void*)meta;
		req->out_reqs += 1;
		sh_get_active_stripe(time, meta->sctlr, subreq);
		stripe_no += 1;
		curr += stripe_sz;
	}
	if ((--req->out_reqs) == 0)
		meta->comp_fn(time, req, NULL);
}

