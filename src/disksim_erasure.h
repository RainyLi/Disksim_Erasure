/*
 * disksim_erasure.h
 *
 *  Created on: Feb 22, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_ERASURE_H_
#define DISKSIM_ERASURE_H_

#include "disksim_event_queue.h"

#define CODE_RDP		0
#define CODE_EVENODD	1
#define CODE_HCODE		2
#define CODE_XCODE		3

#define STRATEGY_OPTIMAL	0
#define STRATEGY_MIN_DIFF	1
#define STRATEGY_MIN_L		2
#define STRATEGY_MIN_STD	3
#define STRATEGY_MIN_L2		4
#define STRATEGY_MIN_DOT	5
#define STRATEGY_RANDOM		6
#define STRATEGY_MIN_MAX	7

typedef struct element_t {
	int row;
	int col;
	struct element_t *next;
} element;

typedef struct parity_chain_t {
	int type; // row=0, diagonal=1,2...
	element *dest;
	element *deps;
	struct erasure_chain_t *next;
} paritys;

typedef struct entry_t {
	int row; // unit number
	int col; // device number
	element *depends; // depends
} entry;

typedef struct rottable_t {
	int rows, cols;
	int *hit; // hit
	int *ll, *rr; // range
	struct entry_t *entry;
} rottable;

typedef struct metadata_t {
	// general attributes
	int codetype;
	int phydisks;
	int numdisks;
	int unitsize;

	// stripe level variables, maintained by functions provided by erasure codes
	int prime;
	int rows;
	int cols;
	paritys *chains;
	int numchains;
	int dataunits;
	int totalunits;
	struct entry_t *entry; // map from data block number to position on disks
	int *matrix; // totalunits by dataunits
	int *rmap; // map from position to data block number

	// disk rebuild
	int numfailures;
	int *failed;
	int laststripe;
	int totstripes;

	paritys ***chs; // parity chains protecting the unit
	int *chl; // num of chains
	int ***distr;
	int **distrpatt;
	int *distrlen;
	int *test;
	double last;
	int *optimal;
	rottable *ph1, *ph2;
} metadata;

const char* get_code_name(int code);
const char* get_method_name(int method);

void erasure_initialize(metadata *meta, int codetype, int disks, int unit);
void erasure_maprequest(metadata *meta, ioreq *req);

void   erasure_disk_failure(metadata *meta, int devno);
double erasure_get_score(int *a, int *b, int *mask, int disks, int method);
void   erasure_adaptive_rebuild(metadata *meta, ioreq *req, int stripeno, int pattern);

#endif /* DISKSIM_ERASURE_H_ */
