/*
 * test.c
 *
 *  Created on: Feb 21, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "parv.h"

int print_help(char **argv) {
	printf("usage: %s [options]\n", argv[0]);
	printf("\t-n\t--disks\t{number}\tnumber of disks\n");
	printf("\t-r\t--raid\t{string}\traid type: {evenodd, rdp, raid5}\n");
	printf("\t-u\t--unit\t{number}\tstripe unit size (in KB)\n");
	printf("\t-m\t--model\t{string}\tdisk model\n");
	printf("\t-o\t--outv\t{string}\tstatistics output file\n");
	printf("\t-t\t--trace\t{string}\ttrace file\n");
	printf("\t-h\t--help\t\t\tprint help information\n");
	return 0;
}

int main(int argc, char **argv)
{
	int disks = 5;
	int level = RAID5;
	int unitsize = 32; // 32KB
	char *diskname = "cheetah9LP";
	char *outv = "test.outv";
	char *trace = "t.trace";
	char *flag, *argu;
	int i = 1;

	if (argc == 1)
		argv[argc++] = "-h";
	while (i < argc) {
		flag = argv[i++];
		if (flag[0] != '-') {
			fprintf(stderr, "invalid flag\n");
			return print_help(argv);
		}
		if (!strcmp(flag, "--help") || !strcmp(flag, "-h"))
			return print_help(argv);
		if (i >= argc) {
			fprintf(stderr, "invalid number of arguments\n");
			exit(1);
		}
		argu = argv[i++];
		if (!strcmp(flag, "--disks") || !strcmp(flag, "-n") || !strcmp(flag, "-d"))
			disks = atoi(argu);
		else if (!strcmp(flag, "--unit") || !strcmp(flag, "-u"))
			unitsize = atoi(argu);
		else if (!strcmp(flag, "--model") || !strcmp(flag, "-m"))
			diskname = argu;
		else if (!strcmp(flag, "--outv") || !strcmp(flag, "-o"))
			outv = argu;
		else if (!strcmp(flag, "--trace") || !strcmp(flag, "-t"))
			trace = argu;
		else if (!strcmp(flag, "--raid") || !strcmp(flag, "-r")) {
			if (!strcmp(argu, "evenodd"))
				level = EVENODD;
			if (!strcmp(argu, "rdp"))
				level = RDP;
			if (!strcmp(argu, "hcode"))
				level = HCODE;
		}
	}

	set_param(disks, level, unitsize, diskname);
	gen_parv(get_parv_file(), phy_disks(), unit_size_blk(),
			 raid_name(), hdd_name(), hdd_model());
	gen_sh(get_sh_file(), get_parv_file(), outv,
		   trace, phy_disks());
	add_run_priv(get_sh_file());
}
