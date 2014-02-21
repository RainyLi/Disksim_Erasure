#ifndef COMMON_H_
#define COMMON_H_

#include <cstring>
#include <cstdio>

struct Instruction {
	double timestamp;
	int dev;
	int start, blks;
	int op;
	Instruction():timestamp(0),dev(0),start(0),blks(0),op(0) {}
	Instruction(double t, int d, int s, int b, int o):timestamp(t),dev(d),start(s),blks(b),op(o){}
};

struct Statistics {
	int disks, unit;
	int *disk_distrb;
	int row_size, array_size;
	int queue_size, *queue, ptr;
	int hits;
	int *table;
	
	Statistics(int disks=5, int unit=8, int array_size=200, int queue_size=10, int *table=NULL);
	~Statistics();
	
	void Add(const Instruction &);
	int NextArrayno();
};

class Sortter {
private:
	Instruction *buffer;
	int ptr, size;
	FILE *fout;
public:
	Sortter(FILE* f, int size=1000);
	~Sortter();
	void push_back(const Instruction&);
	void print();
};

bool cmp_timestamp(const Instruction &a, const Instruction &b);

#endif

