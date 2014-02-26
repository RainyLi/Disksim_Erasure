/*
 * global.h
 *
 *  Created on: Feb 21, 2014
 *      Author: zyz915
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_

// supported MDS erasure codes
#define RAID5			1
#define RDP				2
#define EVENODD			3
#define HCODE			4
#define XCODE			5

int is_prime(int number);
int set_params(int disks, int code, int unit_kb, const char *hdd_name);

int phy_disks();			// physical disk number (data + parity disks)
int num_disks();			// logical disk number (data disks)
int unit_size_kb();			// stripe unit size in KB
int unit_size_blk();		// stripe unit size in blocks

const char* raid_name();
const char* hdd_name();
const char* hdd_model();

const char* get_parv_file();
const char* get_sh_file();

void add_run_priv(const char *filename);

#endif /* GLOBAL_PARAM_H_ */
