/*
 * disksim_erasure.h
 *
 *  Created on: Feb 22, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_ERASURE_H_
#define DISKSIM_ERASURE_H_

#define CODE_RDP		0	// RDP code
#define CODE_EVENODD	1	// EVENODD code
#define CODE_HCODE		2	// H-code
#define CODE_XCODE		3	// X-code
#define CODE_LIBERATION	4	// Liberation code
#define CODE_SHCODE		5	// Shortened H-code
#define CODE_EXT_HCODE	6	// Extended H-code
#define CODE_STAR		7	// STAR code
#define CODE_TRIPLE		8	// Triple Parity code
#define CODE_CODE56		9	// Code 5-6
#define CODE_PCODE		10	// P-code
#define CODE_CYCLIC		11	// Cyclic code
#define CODE_RAID0		12	// RAID-0
#define CODE_RAID5		13	// RAID-5

typedef struct element_t {
	int row;
	int col;
	struct element_t *next;
} element;

typedef struct parity_t {
	int type; // row=0, diagonal=1,2...
	element *dest;
	element *deps;
	struct parity_t *next;
} parity;

typedef struct {
	int rows, cols;
	int *hit; // hit
	int *ll, *rr; // range
	element *map;
} rottable;

typedef struct {
	int codetype;
	int phydisks;
	int numdisks;
	int unitsize;
	int prime;
	int rows, cols;
	parity *chains;
	int numchains;
	int dataunits;
	int totalunits;
	int stripesize;
	element *map; // map from data block ID to corresponding data & parity blocks
	int *rmap; // map from position to data block number
	rottable *ph1, *ph2;
} metadata;

typedef struct {
	int reqno;
	int reqtype;
	double time;
	int blkno;
	int bcount;
	int flag;
	int stat;

	struct disksim_request *reqs, *tail;
	int numreqs, donereqs;

	int numXORs;
	int numIOs;
} ioreq;

typedef void(*initializer)(metadata*);

typedef struct {
	int codeID;
	const char *flag;
	const char *name;
	initializer func;
} codespec;

const char* get_code_name(int code);
int get_code_id(const char *code);

void erasure_initialize();
void erasure_init_code(metadata *meta, int codetype, int disks, int unit);
void erasure_standard_maprequest(metadata *meta, ioreq *req);

void add_to_ioreq(ioreq *req, struct disksim_request *tmp);

#endif /* DISKSIM_ERASURE_H_ */
