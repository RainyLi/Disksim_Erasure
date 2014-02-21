#ifndef RAID5_H_
#define RAID5_H_

#include "common.h"

#include <cstdio>
#include <vector>

class Raid5 {
private:
	int row_size, array_size;
	int disks, unit;
	int broke;
	int array_max;
	int *table, *mark;
	int curr_to_fix;
	int curr_no;
	double curr_time;

	bool recovery;
	double interval;
	int instrs_per_sec;

	std::vector<Instruction> fix; /// stripe recovery trace
	std::vector<Instruction> queue, temp;
	int remains;
	Statistics *stat;
	Sortter *sorted;

	/**
	 * Handle single instruction & put it in temp
	 */
	void HandleInstruction(double timestamp, int dev, int start, int blks, int op);

	/**
	 * Handle the situation that one of the disks is down.
	 * A recovery trace is generated concurrently.
	 */
	void HandleRecovery(double timestamp, int dev, int start, int blks, int op);

	/**
	 * Handle the situation that all disks are good.
	 */
	void HandleNormal(double timestamp, int dev, int start, int blks, int op);

	/**
	 * Generate Recovery Trace
	 */
	void GenRecoveryTrace();

public:

	Raid5(int disks, int blk_max, int unit, int broke, FILE *fout);

	void SetRecoveryParams(double interval, int instrs_per_sec);

	void Handle(double timestamp, int dev, int start, int blks, int op);

	void Print();

	int Count();

	~Raid5();
};

#endif


