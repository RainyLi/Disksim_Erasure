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
	int pattern;
	paritys ***chs; // parity chains protecting the unit
	int *chl; // num of chains
	int ***distr;
	int **distrpatt;
	int *distrlen;
	int *test;
	double last;
	rottable *ph1, *ph2;
} metadata;


const char* get_method_name(int method);
void erasure_initialize(metadata *meta, int codetype, int disks, int unit);
void  erasure_maprequest(metadata *meta, ioreq *req);

void erasure_failure(metadata *meta, int devno);
void erasure_rebuild(metadata *meta, int *distr, int method, ioreq *req);

#endif /* DISKSIM_ERASURE_H_ */
