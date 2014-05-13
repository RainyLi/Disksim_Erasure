/*
 * partial.c
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "disksim_arm.h"
#include "disksim_erasure.h"
#include "disksim_event_queue.h"
#include "disksim_global.h"
#include "disksim_interface.h"
#include "disksim_iostat.h"
#include "disksim_rand48.h"
#include "disksim_timer.h"

#define EVENT_STOP_SIM			0
#define EVENT_TRACE_FETCH		1
#define EVENT_TRACE_RECON		2
#define EVENT_TRACE_MAPREQ		3
#define EVENT_IO_COMPLETE		5
#define EVENT_IO_INTERNAL		6
#define EVENT_STAT_ADD			7
#define EVENT_STAT_PEAK			8

static equeue *eventq;
static metadata *meta; // erasure code metadata
static struct disksim_interface *interface;
static double currtime = 0;
static double scale = 1;
static int reqno = 0;
static int disks = 12;
static int *distr;
static int unit = 16; // size in KB
static int width = 2;
static int delay = 1000;
static long long limit = 0; // maximum operations
static int code = CODE_RDP;
static const char *result = "";
static const char *parfile = "../valid/12disks.parv";
static const char *outfile = "t.outv";

static int help(const char *main) {
	printf("usage: %s [options]:\n", main);
	printf("  options are:\n");
	printf("\t-n, --num     [number]\t number of disks\n");
	printf("\t-u, --unit    [number]\t stripe unit size (KB)\n");
	printf("\t-s, --stop    [number]\t stop after reconstruct how many stripes\n");
	printf("\t-d, --delay   [number]\t io_stat sampling interval\n");
	printf("\t-i, --input   [string]\t input trace file\n");
	printf("\t-o, --output  [string]\t DiskSim output file\n");
	printf("\t-c, --code    [string]\t erasure code [rdp, evenodd, hcode, xcode, liberation]\n");
	printf("\t-p, --parv    [string]\t parv file\n");
	printf("\t-a, --append  [string]\t append results to file\n");
	printf("\t-f, --failure [string]\t set failed disks separated by ','\n");
	printf("\t-h, --help            \t help information\n");
	return 0;
}

static void trace_add_next(FILE *f) {
	ioreq *req = (ioreq*) getfromextraq();
	static char line[201];
	if (fgets(line, 200, f) == NULL) {
		event_queue_add(eventq, create_event_node(currtime + 1000, EVENT_STOP_SIM, 0));
		return;
	}
	if (sscanf(line, "%lf%*d%d%d%d", &req->time, &req->blkno, &req->bcount, &req->flag) != 4) {
		fprintf(stderr, "Wrong number of arguments for I/O trace event type\n");
		fprintf(stderr, "line: %s\n", line);
		exit(-1);
	}
	req->time *= scale;
	req->stat = 1;
	req->curr = NULL;
	req->groups = NULL;
	req->reqno = ++reqno; // auto increment ID
	event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_MAPREQ, req));
	event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_FETCH, f));
}

static void trace_add_recon(double time) {
	ioreq *req = (ioreq*) getfromextraq();
	req->time = time;
	req->stat = 0;
	req->curr = NULL;
	req->groups = NULL;
	req->reqno = ++reqno; // auto increment ID
	event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_MAPREQ, req));
}

static void ioreq_maprequest(double time, ioreq *req) {
	struct disksim_request *tmpreq;
	erasure_maprequest(meta, req);
	if (req->groups == 0) {
		fprintf(stderr, "map_request failed.\n");
		exit(-1);
	}
	iostat_ioreq_start(time, req);
	req->curr = req->groups;
	req->curr->cnt = req->curr->numreqs;
	for (tmpreq = req->curr->reqs; tmpreq != NULL; tmpreq = tmpreq->next) {
		disksim_interface_request_arrive(interface, time, tmpreq);
		event_queue_add(eventq, create_event_node(time, EVENT_STAT_ADD,
				iostat_create_node(time, tmpreq->devno, tmpreq->bytecount / 512)));
	}
}

static void ioreq_complete(double time, ioreq *req) {
	struct disksim_request *tmpreq, *nq;
	iogroup *group, *ng;
	req->curr->cnt--;
	if (req->curr->cnt == 0) {
		if (req->curr->next != NULL) {
			req->curr = req->curr->next;
			req->curr->cnt = req->curr->numreqs;
			for (tmpreq = req->curr->reqs; tmpreq != NULL; tmpreq = tmpreq->next) {
				disksim_interface_request_arrive(interface, time, tmpreq);
				event_queue_add(eventq, create_event_node(time, EVENT_STAT_ADD,
						iostat_create_node(time, tmpreq->devno, tmpreq->bytecount / 512)));
			}
			event_queue_add(eventq, create_event_node(currtime, EVENT_TRACE_FETCH, 0));
		} else { // request complete
			iostat_ioreq_complete(time, req);
			for (group = req->groups; group != NULL; group = ng) {
				for (tmpreq = group->reqs; tmpreq != NULL; tmpreq = nq) {
					nq = tmpreq->next;
					addtoextraq((event*)tmpreq);
				}
				ng = group->next;
				addtoextraq((event*)group);
			}
			addtoextraq((event*)req);
		}
	}
}

static void calculate_peak_throughput(double time) {
	iostat_detect_peak(time, delay);
	event_queue_add(eventq, create_event_node(time + delay, EVENT_STAT_PEAK, 0));
}

static void recon_complete_callback(double time, struct disksim_request *dr, void *ctx) {
	event_queue_add(eventq, create_event_node(time, EVENT_IO_INTERNAL, ctx));
	event_queue_add(eventq, create_event_node(time + 1e-9, EVENT_IO_COMPLETE, dr->reqctx));
}

static void recon_schedule_callback(disksim_interface_callback_t fn, double time, void *ctx) {
	event_queue_add(eventq, create_event_node(time, EVENT_IO_INTERNAL, ctx));
}

static void recon_descheduled_callback(double time, void *ctx) {
}

int main(int argc, char **argv)
{
	int i, p = 1;
	while (p < argc) {
		const char *flag = argv[p++];
		if (!strcmp(flag, "-h") || !strcmp(flag, "--help"))
			return help(argv[0]);
		if (p == argc) {
			fprintf(stderr, "require arguments after \"%s\" flag.\n", flag);
			exit(-1);
		}
		const char *argu = argv[p++];
		if (!strcmp(flag, "-p") || !strcmp(flag, "--parv"))
			parfile = argu;
		else if (!strcmp(flag, "-o") || !strcmp(flag, "--output"))
			outfile = argu;
		else if (!strcmp(flag, "-n") || !strcmp(flag, "--num"))
			disks = atoi(argu);
		else if (!strcmp(flag, "-u") || !strcmp(flag, "--unit"))
			unit = atoi(argu);
        else if (!strcmp(flag, "-a") || !strcmp(flag, "--append"))
        	result = argu;
        else if (!strcmp(flag, "-c") || !strcmp(flag, "--code"))
        	code = get_code_id(argu);
        else if (!strcmp(flag, "-w"))
        	width = atoi(argu);
        else {
        	fprintf(stderr, "unknown flag: %s\n", flag);
        	exit(0);
        }
	}

	interface = disksim_interface_initialize(
			parfile, outfile, recon_complete_callback,
			recon_schedule_callback, recon_descheduled_callback,
			(void*) interface, 0, 0);

	DISKSIM_srand48(1);

	eventq = malloc(sizeof(equeue));
	event_queue_initialize(eventq);

	meta = (metadata*) malloc(sizeof(metadata));
	erasure_initialize(meta, code, disks, unit * 2);

	iostat_initialize(disks);
	distr = malloc(sizeof(int) * disks);

	int currblock = 0, totblocks = meta->dataunits;
	event_queue_add(eventq, create_event_node(0, EVENT_TRACE_FETCH, 0));
	timer_start(0);

	struct disksim_request *tmpreq;
	int stop_simulation = 0;
	double checkpoint = 0;
	while (!stop_simulation) {
		enode *node = event_queue_pop(eventq);
		if (node == NULL)
			break; // stop simulation
		if (currtime != node->time)
			disksim_interface_internal_event(interface, node->time, node->ctx);
		currtime = node->time;
		switch (node->type) {
		case EVENT_STOP_SIM:
			stop_simulation = 1;
			break;
		case EVENT_TRACE_FETCH:
			//printf("time = %f, type = EVENT_TRACE_FETCH\n", node->time);
			if (currblock < totblocks) {
				ioreq *req = (ioreq*) getfromextraq();
				req->time = currtime;
				req->blkno = currblock * unit * 2;
				req->bcount = width * unit * 2;
				req->flag = WRITE;
				req->stat = 1;
				req->curr = NULL;
				req->groups = NULL;
				req->reqno = ++reqno; // auto increment ID
				event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_MAPREQ, req));
				currblock++;
			}
			break;
		case EVENT_TRACE_MAPREQ:
			//printf("time = %f, type = EVENT_TRACE_ITEM\n", node->time);
			ioreq_maprequest(node->time, (ioreq*)node->ctx);
			break;
		case EVENT_IO_INTERNAL:
			//printf("time = %f, type = EVENT_IO_INTERNAL\n", node->time);
			break;
		case EVENT_IO_COMPLETE:
			//printf("time = %f, type = EVENT_IO_COMPLETE\n", node->time);
			ioreq_complete(node->time, (ioreq*)node->ctx);
			break;
		case EVENT_STAT_ADD:
			//printf("time = %f, type = EVENT_STAT_COMPLETE\n", node->time);
			iostat_add((statnode*) node->ctx);
			break;
		case EVENT_STAT_PEAK:
			//printf("time = %f, type = EVENT_STAT_PEAK\n", node->time);
			calculate_peak_throughput(node->time);
			break;
		default:
			printf("time = %f, type = INVALID\n", node->time);
			exit(-1);
		}
		free_event_node(node);
		if ((--limit) == 0) break;
		if (currtime > checkpoint) {
			putchar('*');
			fflush(stdout);
			checkpoint += 60000;
		}
	}
	disksim_interface_shutdown(interface, currtime);
	timer_end(0);
	long long duration = timer_microsecond(0);
	printf("\n");
	printf("===================================================\n");
	printf("Disks = %d\n", disks);
	printf("Unit Size = %d\n", unit);
	printf("Code = %s\n", get_code_name(code));
	printf("Total Simulation Time = %f ms\n", currtime);
	printf("Avg. Response Time = %f ms\n", iostat_avg_response_time());
	printf("Avg. XORs per Write = %f\n", iostat_avg_xors_per_write());
	printf("Avg. I/Os per Request = %f\n", iostat_avg_IOs_per_request());
	printf("Throughput = %f MB/s\n", iostat_throughput());
	printf("Experiment Duration = %.3f s\n", duration / 1000.0);

	FILE *exp = fopen(result, "a");
	if (exp != NULL) {
		fprintf(exp, "===================================================\n");
		fprintf(exp, "Disks = %d\n", disks);
		fprintf(exp, "Unit Size = %d\n", unit);
		fprintf(exp, "Code = %s\n", get_code_name(code));
		fprintf(exp, "Total Simulation Time = %f ms\n", currtime);
		fprintf(exp, "Avg. Response Time = %f ms\n", iostat_avg_response_time());
		fprintf(exp, "Avg. XORs Per Write = %f\n", iostat_avg_xors_per_write());
		fprintf(exp, "Avg. I/Os per Request = %f\n", iostat_avg_IOs_per_request());
		fprintf(exp, "Throughput = %f MB/s\n", iostat_throughput());
		fprintf(exp, "Experiment Duration = %.3f s\n", duration / 1000.0);
		fclose(exp);
	}
	return 0;
}
