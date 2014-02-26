/*
 * parv.c
 *
 *  Created on: Feb 21, 2014
 *      Author: zyz915
 */

#include "parv.h"

#include <stdio.h>
#include <stdlib.h>

static void gen_global(FILE *parv)
{
	fprintf(parv, "disksim_global Global {\n");
	fprintf(parv, "  Init Seed = 42,\n");
	fprintf(parv, "  Real Seed = 42,\n");
	fprintf(parv, "  Stat definition file = statdefs\n");
	fprintf(parv, "}\n");
	fprintf(parv, "\n");
}

static void gen_stats(FILE *parv)
{
	fprintf(parv, "disksim_stats Stats {\n");
	fprintf(parv, "  iodriver stats = disksim_iodriver_stats {\n");
	fprintf(parv, "  Print driver size stats = 1,\n");
	fprintf(parv, "  Print driver locality stats = 1,\n");
	fprintf(parv, "  Print driver blocking stats = 1,\n");
	fprintf(parv, "  Print driver interference stats = 1,\n");
	fprintf(parv, "  Print driver queue stats = 1,\n");
	fprintf(parv, "  Print driver crit stats = 1,\n");
	fprintf(parv, "  Print driver idle stats = 1,\n");
	fprintf(parv, "  Print driver intarr stats = 1,\n");
	fprintf(parv, "  Print driver streak stats = 1,\n");
	fprintf(parv, "  Print driver stamp stats = 1,\n");
	fprintf(parv, "  Print driver per-device stats = 1 },\n");
	fprintf(parv, "  bus stats = disksim_bus_stats {\n");
	fprintf(parv, "    Print bus idle stats = 1,\n");
	fprintf(parv, "    Print bus arbwait stats = 1\n");
	fprintf(parv, "  },\n");
	fprintf(parv, "  ctlr stats = disksim_ctlr_stats {\n");
	fprintf(parv, "    Print controller cache stats = 1,\n");
	fprintf(parv, "    Print controller size stats = 1,\n");
	fprintf(parv, "    Print controller locality stats = 1,\n");
	fprintf(parv, "    Print controller blocking stats = 1,\n");
	fprintf(parv, "    Print controller interference stats = 1,\n");
	fprintf(parv, "    Print controller queue stats = 1,\n");
	fprintf(parv, "    Print controller crit stats = 1,\n");
	fprintf(parv, "    Print controller idle stats = 1,\n");
	fprintf(parv, "    Print controller intarr stats = 1,\n");
	fprintf(parv, "    Print controller streak stats = 1,\n");
	fprintf(parv, "    Print controller stamp stats = 1,\n");
	fprintf(parv, "    Print controller per-device stats = 1\n");
	fprintf(parv, "  },\n");
	fprintf(parv, "  device stats = disksim_device_stats {\n");
	fprintf(parv, "    Print device queue stats = 1,\n");
	fprintf(parv, "    Print device crit stats = 1,\n");
	fprintf(parv, "    Print device idle stats = 1,\n");
	fprintf(parv, "    Print device intarr stats = 1,\n");
	fprintf(parv, "    Print device size stats = 1,\n");
	fprintf(parv, "    Print device seek stats = 1,\n");
	fprintf(parv, "    Print device latency stats = 1,\n");
	fprintf(parv, "    Print device xfer stats = 1,\n");
	fprintf(parv, "    Print device acctime stats = 1,\n");
	fprintf(parv, "    Print device interfere stats = 1,\n");
	fprintf(parv, "    Print device buffer stats = 1\n");
	fprintf(parv, "  },\n");
	fprintf(parv, "  process flow stats = disksim_pf_stats {\n");
	fprintf(parv, "    Print per-process stats =  1,\n");
	fprintf(parv, "    Print per-CPU stats =  1,\n");
	fprintf(parv, "    Print all interrupt stats =  1,\n");
	fprintf(parv, "    Print sleep stats =  1\n");
	fprintf(parv, "  }\n");
	fprintf(parv, "}\n");
	fprintf(parv, "\n");
}

static void gen_iodriver(FILE *parv)
{
	fprintf(parv, "disksim_iodriver DRIVER0 {\n");
	fprintf(parv, "  type = 1,\n");
	fprintf(parv, "  Constant access time = 0.0,\n");
	fprintf(parv, "  Scheduler = disksim_ioqueue {\n");
	fprintf(parv, "    Scheduling policy = 3,\n");
	fprintf(parv, "    Cylinder mapping strategy = 1,\n");
	fprintf(parv, "    Write initiation delay = 0.0,\n");
	fprintf(parv, "    Read initiation delay = 0.0,\n");
	fprintf(parv, "    Sequential stream scheme = 0,\n");
	fprintf(parv, "    Maximum concat size = 0,\n");
	fprintf(parv, "    Overlapping request scheme = 0,\n");
	fprintf(parv, "    Sequential stream diff maximum = 0,\n");
	fprintf(parv, "    Scheduling timeout scheme = 0,\n");
	fprintf(parv, "    Timeout time/weight = 30,\n");
	fprintf(parv, "    Timeout scheduling = 3,\n");
	fprintf(parv, "    Scheduling priority scheme = 0,\n");
	fprintf(parv, "    Priority scheduling = 3\n");
	fprintf(parv, "  },\n");
	fprintf(parv, "  Use queueing in subsystem = 0\n");
	fprintf(parv, "}\n");
	fprintf(parv, "\n");
}

static void gen_bus(FILE *parv)
{
	fprintf(parv, "disksim_bus BUS0 {\n");
	fprintf(parv, "  type = 2,\n");
	fprintf(parv, "  Arbitration type = 1,\n");
	fprintf(parv, "  Arbitration time = 0.0,\n");
	fprintf(parv, "  Read block transfer time = 0.0,\n");
	fprintf(parv, "  Write block transfer time = 0.0,\n");
	fprintf(parv, "  Print stats =  0\n");
	fprintf(parv, "}\n");
	fprintf(parv, "\n");

	fprintf(parv, "disksim_bus BUS1 {\n");
	fprintf(parv, "  type = 1,\n");
	fprintf(parv, "  Arbitration type = 1,\n");
	fprintf(parv, "  Arbitration time = 0.0,\n");
	fprintf(parv, "  Read block transfer time = 0.0512,\n");
	fprintf(parv, "  Write block transfer time = 0.0512,\n");
	fprintf(parv, "  Print stats =  1\n");
	fprintf(parv, "}\n");
	fprintf(parv, "\n");
}

static void gen_ctlr(FILE *parv)
{
	fprintf(parv, "disksim_ctlr CTLR0 {\n");
	fprintf(parv, "  type = 1,\n");
	fprintf(parv, "  Scale for delays = 0.0,\n");
	fprintf(parv, "  Bulk sector transfer time = 0.0,\n");
	fprintf(parv, "  Maximum queue length = 0,\n");
	fprintf(parv, "  Print stats =  1\n");
	fprintf(parv, "}\n");
	fprintf(parv, "\n");
}

static void gen_source(FILE *parv, const char *model)
{
	fprintf(parv, "source %s.diskspecs\n", model);
	fprintf(parv, "\n");
}

static void gen_comp(FILE *parv, int disks, const char *model)
{
	fprintf(parv, "instantiate [ statfoo ] as Stats\n");
	fprintf(parv, "instantiate [ ctlr0 .. ctlr%d ] as CTLR0\n", disks - 1);
	fprintf(parv, "instantiate [ bus0 ] as BUS0\n");
	fprintf(parv, "instantiate [ disk0 .. disk%d ] as %s\n", disks - 1, model);
	fprintf(parv, "instantiate [ driver0 ] as DRIVER0\n");
	fprintf(parv, "instantiate [ bus1 .. bus%d ] as BUS1\n", disks);
	fprintf(parv, "\n");
}

void gen_topology(FILE *parv, int disks)
{
	int i;
	fprintf(parv, "topology disksim_iodriver driver0 [\n");
	fprintf(parv, "  disksim_bus bus0 [\n");
	for (i = 0; i < disks; i++) {
		fprintf(parv, "    disksim_ctlr ctlr%d [\n", i);
		fprintf(parv, "      disksim_bus bus%d [\n", i + 1);
		fprintf(parv, "        disksim_disk disk%d []\n", i);
		fprintf(parv, "      ]\n");
		fprintf(parv, "    ]%c\n", (i == disks - 1 ? ' ' : ','));
	}
	fprintf(parv, "  ]\n");
	fprintf(parv, "]\n");
	fprintf(parv, "\n");
}

static void gen_logorg(FILE *parv, int disks, int stripe_unit, const char *raid)
{
	fprintf(parv, "disksim_logorg org0 {\n");
	fprintf(parv, "  Addressing mode = Array,\n");
	fprintf(parv, "  Distribution scheme = Striped,\n");
	fprintf(parv, "  Redundancy scheme = %s,\n", raid);
	fprintf(parv, "  devices = [ disk0 .. disk%d ],\n", disks - 1);
	fprintf(parv, "  Stripe unit  =  %d,\n", stripe_unit);
	fprintf(parv, "  Synch writes for safety =  0,\n");
	fprintf(parv, "  Number of copies =  2,\n");
	fprintf(parv, "  Copy choice on read =  6,\n");
	fprintf(parv, "  RMW vs. reconstruct =  0.5,\n");
	fprintf(parv, "  Parity stripe unit =  %d,\n", stripe_unit);
	fprintf(parv, "  Parity rotation type =  1,\n");
	fprintf(parv, "  Time stamp interval =  0.000000,\n");
	fprintf(parv, "  Time stamp start time =  0.000000,\n");
	fprintf(parv, "  Time stamp stop time =  10000000000.000000,\n");
	fprintf(parv, "  Time stamp file name =  stamps\n");
	fprintf(parv, "}\n");
	fprintf(parv, "\n");
}

int gen_parv_f(FILE *parv, int disks, int unit_blk, const char *raid,
			   const char *hdd_name, const char *hdd_model)
{
	gen_global(parv);
	gen_stats(parv);
	gen_iodriver(parv);
	gen_bus(parv);
	gen_ctlr(parv);
	gen_source(parv, hdd_name);
	gen_comp(parv, disks, hdd_model);
	gen_topology(parv, disks);
	gen_logorg(parv, disks, unit_blk, raid);
	return 0;
}

int gen_parv(const char *filename, int disks, int unit_blk, const char *raid,
			 const char *hdd_name, const char *hdd_model)
{
	FILE *parv = fopen(filename, "w");
	if (parv == NULL) {
		fprintf(stderr, "cannot open parv file.\n");
		exit(0);
	}
	gen_parv_f(parv, disks, unit_blk, raid, hdd_name, hdd_model);
	fclose(parv);
	return 0;
}

int gen_sh_f(FILE *sh, const char *parv_file, const char *out_file,
			 const char *trace_file, int disks)
{
	int i;
	fprintf(sh, "PREFIX=../src\n\n");
	fprintf(sh, "${PREFIX}/disksim %s %s ascii %s 0\n",
			parv_file, out_file, trace_file);
	fprintf(sh, "grep \"IOdriver Response time average\" %s\n", out_file);
	for (i = 0; i < disks; i++) {
		fprintf(sh, "echo \"== DISK #%d ==\"\n", i);
		fprintf(sh, "grep \"Disk #%d Seek time average\" %s\n", i, out_file);
		fprintf(sh, "grep \"Disk #%d Rotational latency average\" %s\n", i, out_file);
		fprintf(sh, "grep \"Disk #%d Transfer time average\" %s\n", i, out_file);
	}
	return 0;
}

int gen_sh(const char *filename, const char *parv_file, const char *out_file,
		   const char *trace_file, int disks)
{
	FILE *sh = fopen(filename, "w");
	if (sh == NULL) {
		fprintf(stderr, "cannot open sh file.\n");
		exit(0);
	}
	gen_sh_f(sh, parv_file, out_file, trace_file, disks);
	fclose(sh);
	return 0;
}

