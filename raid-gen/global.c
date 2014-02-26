/*
 * global.c
 *
 *  Created on: Feb 21, 2014
 *      Author: zyz915
 */

#include "global.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_init = 0;
static int g_phy_disks;
static int g_num_disks;
static int g_unit_size_kb;
static int g_unit_size_blk;
static char g_raid_name[32];
static char *g_hdd_model;
static char *g_hdd_name;
static char g_parv[128];
static char g_sh[128];
static char t_buff[64];

static inline void check_init()
{
	if (!g_init) {
		fprintf(stderr, "parameters not initialized.");
		exit(0);
	}
}

static char* get_model(const char *hdd_name)
{
	if (!strcmp(hdd_name, "atlas_III"))
		return "QUANTUM_QM39100TD-SW";
	if (!strcmp(hdd_name, "atlas10k"))
		return "QUANTUM_TORNADO_validate";
	if (!strcmp(hdd_name, "barracuda"))
		return "SEAGATE_ST32171W";
	if (!strcmp(hdd_name, "cheetah4LP"))
		return "SEAGATE_ST34501N_validate";
	if (!strcmp(hdd_name, "cheetah9LP"))
		return "SEAGATE_ST39102LW_validate";
	if (!strcmp(hdd_name, "dec_rz26"))
		return "DEC_RZ26_validate";
	if (!strcmp(hdd_name, "hp_c2247a"))
		return "HP_C2247A_validate";
	if (!strcmp(hdd_name, "hp_c2249a"))
		return "HP_C2249A";
	if (!strcmp(hdd_name, "hp_c3323a_validate"))
		return "HP_C3323A_validate";
	if (!strcmp(hdd_name, "hp_c3323a"))
		return "HP_C3323A";
	if (!strcmp(hdd_name, "hp_c2490a"))
		return "HP_C2490A_validate";
	if (!strcmp(hdd_name, "ibm18es"))
		return "IBM_DNES-309170W_validate";
	if (!strcmp(hdd_name, "st41601n"))
		return "Seagate_ST41601N_validate";
	fprintf(stderr, "invalid disk name.\n");
	exit(0);
	return NULL;
}

int is_prime(int number)
{
	int i;
	for (i = 2; i * i < number; i++)
		if (number % i == 0)
			return 0;
	return 1;
}

int set_param(int disks, int code, int unit_kb, const char *hdd_name)
{
	g_phy_disks = disks;
	switch (code) {
	case RAID5:
		g_num_disks = disks - 1;
		sprintf(g_raid_name, "Parity_rotated");
		break;
	case RDP:
		g_num_disks = disks - 2;
		sprintf(g_raid_name, "RDP");
		if (!is_prime(disks - 1)) {
			fprintf(stderr, "invalid disk number in RDP.\n");
			exit(0);
		}
		break;
	case EVENODD:
		g_num_disks = disks - 2;
		sprintf(g_raid_name, "EVENODD");
		if (!is_prime(disks - 2)) {
			fprintf(stderr, "invalid disk number in EVENODD.\n");
			exit(0);
		}
		break;
	case HCODE:
		g_num_disks = disks - 2;
		sprintf(g_raid_name, "HCODE");
		if (!is_prime(disks - 1)) {
			fprintf(stderr, "invalid disk number in HCODE.\n");
			exit(0);
		}
		break;
	case XCODE:
		g_num_disks = disks - 2;
		sprintf(g_raid_name, "XCODE");
		if (!is_prime(disks)) {
			fprintf(stderr, "invalid disk number in XCODE.\n");
			exit(0);
		}
		break;
	default:
		fprintf(stderr, "unrecognized erasure code.\n");
		exit(0);
	}
	g_unit_size_kb = unit_kb;
	g_unit_size_blk = unit_kb * 2; // 512 Bytes per block, so 1KB = 2blocks
	g_hdd_model = get_model(hdd_name);
	g_hdd_name = strdup(hdd_name);
	sprintf(g_parv, "../valid/%ddisks_%dK_%s_%s.parv", g_phy_disks, g_unit_size_kb, g_raid_name, hdd_name);
	sprintf(g_sh, "run_%ddisks_%dK_%s_%s.sh", g_phy_disks, g_unit_size_kb, g_raid_name, hdd_name);
	g_init = 1;
	return 0;
}

int num_disks()
{
	check_init();
	return g_num_disks;
}

int phy_disks()
{
	check_init();
	return g_phy_disks;
}

const char* raid_name()
{
	check_init();
	return g_raid_name;
}

const char* hdd_model()
{
	check_init();
	return g_hdd_model;
}

const char* hdd_name()
{
	check_init();
	return g_hdd_name;
}

const char* get_parv_file()
{
	check_init();
	return g_parv;
}

const char* get_sh_file()
{
	check_init();
	return g_sh;
}

int unit_size_kb()
{
	check_init();
	return g_unit_size_kb;
}
int unit_size_blk()
{
	check_init();
	return g_unit_size_blk;
}

void add_run_priv(const char *filename)
{
	sprintf(t_buff, "chmod +x %s", filename);
	system(t_buff);
}
