/*
 * disksim_erasure.h
 *
 *  Created on: Feb 22, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_ERASURE_H_
#define DISKSIM_ERASURE_H_

#include "disksim_global.h"
#include "disksim_logorg.h"

typedef struct erasure_elem_t {
	int row;
	int col;
	struct erasure_elem_t *next;
} element;

typedef struct erasure_chain_t {
	element *dest;
	element *deps;
	struct erasure_chain_t *next;
} parity_chain;

typedef struct erasure_entry_t {
	int row; // unit number
	int col; // device number
	element *depends; // depends
} entry;

typedef struct erasure_metadata_t {
	// general attributes
	int codetype;
	int phydisks;
	int numdisks;
	int unitsperpb; // stripe units per parity unit

	// stripe level variables, maintained by functions provided by erasure codes
	int prime;
	int rows;
	int cols;
	parity_chain *chains;
	int numchains;
	int dataunits;
	int totalunits;
	entry *entry; // map from data block number to position on disks
	int *matrix; // totalunits by dataunits
	int *rmap; // map from position to data block number
} metadata;

typedef struct erasure_table_t {

	int rottype; // default 0
	int dataunits;
	int totalunits;
	entry *entry;
	int *hit;
	int *hit_l, *hit_r; // range
	int rows;
	int cols;
	ioreq_event **reqs;
	depends **depend;
	struct tree_node_t *set;
} table_t;

int use_erasure_code(int reduntype);
void erasure_initialize(logorg *currlogorg);
int erasure_maprequest(logorg *currlogorg, ioreq_event *curr, int numreqs);

#endif /* DISKSIM_ERASURE_H_ */
