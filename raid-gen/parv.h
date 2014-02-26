/*
 * parv.h
 *
 *  Created on: Feb 21, 2014
 *      Author: zyz915
 */

#ifndef PARV_H_
#define PARV_H_

#include <stdio.h>

int gen_parv(const char *filename, int disks, int unit_blk, const char *raid,
			 const char *hdd_name, const char *hdd_model);

int gen_parv_f(FILE *parv, int disks, int unit_blk, const char *raid,
			   const char *hdd_name, const char *hdd_model);

int gen_sh(const char *filename, const char *parv_file, const char *out_file,
		   const char *trace_file, int disks);

int gen_sh_f(FILE *sh, const char *parv_file, const char *out_file,
			 const char *trace_file, int disks);

#endif /* PARV_H_ */
