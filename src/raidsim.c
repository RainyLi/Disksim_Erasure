/*
 * disksim_recon.c
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "disksim_erasure.h"
#include "disksim_event_queue.h"
#include "disksim_global.h"
#include "disksim_interface.h"
#include "disksim_malloc.h"
#include "disksim_rand48.h"
#include "disksim_req.h"
#include "disksim_timer.h"

#define EVENT_STOP_SIM			0
#define EVENT_TRACE_FETCH		1
#define EVENT_TRACE_RECON		2
#define EVENT_TRACE_MAPREQ		3
#define EVENT_IO_COMPLETE		5
#define EVENT_IO_INTERNAL		6

struct disksim_interface *interface;

static event_queue_t *eventq;
static metadata_t *meta; // erasure code metadata
static double currtime = 0;
static double scale = 1;
static int reqno = 0;
static int disks = 12;
static double stop = -1;
static int unit = 16; // size in KB
static long long limit = 0; // maximum operations
static const char *code = "rdp";
static const char *result = "";
static const char *fail = "";
static const char *parfile = "../valid/16disks.parv";
static const char *outfile = "t.outv";
static const char *inpfile = "";

extern int dr_idx, en_idx, rq_idx;

int usage(const char *main)
{
	printf("usage: %s [options]:\n", main);
	printf("  options are:\n");
	printf("\t-n, --num     [number]\t number of disks\n");
	printf("\t-u, --unit    [number]\t stripe unit size (KB)\n");
	printf("\t-s, --stop    [number]\t stop simulation after some seconds\n");
	printf("\t-i, --input   [string]\t input trace file\n");
	printf("\t-o, --output  [string]\t DiskSim output file\n");
	printf("\t-c, --code    [string]\t erasure code [rdp, evenodd, hcode, xcode, liberation, star, shcode]\n");
	printf("\t-p, --parv    [string]\t parv file\n");
	printf("\t-a, --append  [string]\t append results to file\n");
	printf("\t-f, --failure [string]\t set failed disks separated by ','\n");
	printf("\t-h, --help            \t help information\n");
	return 0;
}

void trace_add_next(FILE *f)
{
	static char line[201];
	if (fgets(line, 200, f) == NULL) {
		event_queue_add(eventq, create_event(currtime + 3000, EVENT_STOP_SIM, NULL));
		return;
	}
	ioreq_t *req = create_ioreq();
	memset(req, 0, sizeof(ioreq_t));
	if (sscanf(line, "%lf%*d%ld%d%d", &req->time, &req->blkno, &req->bcount, &req->flag) != 4) {
		fprintf(stderr, "Wrong number of arguments for I/O trace event type\n");
		fprintf(stderr, "line: %s\n", line);
		exit(-1);
	}
	if (stop > 0 && req->time > stop) return;
	req->reqtype = REQ_TYPE_NORMAL;
	req->time *= scale;
	req->reqno = ++reqno; // auto increment ID
	event_queue_add(eventq, create_event(req->time, EVENT_TRACE_MAPREQ, req));
	event_queue_add(eventq, create_event(req->time, EVENT_TRACE_FETCH, f));
}

static double sum = 0;
static int numreqs = 0;

double avg_response_time()
{
	return sum / numreqs;
}

void erasure_req_complete(double time, ioreq_t *req)
{
	sum += (time - req->time);
	numreqs += 1;
	disksim_free(rq_idx, req);
}

void complete_callback(double time, struct disksim_request *dr, void *ctx)
{
	event_queue_add(eventq, create_event(time, EVENT_IO_COMPLETE, (void*)dr));
}

void schedule_callback(disksim_interface_callback_t fn, double time, void *ctx)
{
	event_queue_add(eventq, create_event(time, EVENT_IO_INTERNAL, ctx));
}

void descheduled_callback(double time, void *ctx)
{
}

int main(int argc, char **argv)
{
	int i, p = 1;
	while (p < argc) {
		const char *flag = argv[p++];
		if (!strcmp(flag, "-h") || !strcmp(flag, "--help"))
			return usage(argv[0]);
		if (p == argc) {
			fprintf(stderr, "require arguments after \"%s\" flag.\n", flag);
			exit(-1);
		}
		const char *argu = argv[p++];
		if (!strcmp(flag, "-p") || !strcmp(flag, "--parv"))
			parfile = argu;
		else if (!strcmp(flag, "-i") || !strcmp(flag, "--input"))
			inpfile = argu;
		else if (!strcmp(flag, "-o") || !strcmp(flag, "--output"))
			outfile = argu;
		else if (!strcmp(flag, "-n") || !strcmp(flag, "--num"))
			disks = atoi(argu);
		else if (!strcmp(flag, "-u") || !strcmp(flag, "--unit"))
			unit = atoi(argu);
        else if (!strcmp(flag, "-s") || !strcmp(flag, "--stop"))
        	stop = atof(argu) * 1000;
        else if (!strcmp(flag, "-l") || !strcmp(flag, "--limit"))
        	limit = atol(argu);
        else if (!strcmp(flag, "-a") || !strcmp(flag, "--append"))
        	result = argu;
        else if (!strcmp(flag, "-f") || !strcmp(flag, "--fail"))
        	fail = argu;
        else if (!strcmp(flag, "-c") || !strcmp(flag, "--code"))
        	code = argu;
        else if (!strcmp(flag, "--scale"))
        	scale = atof(argu);
        else {
        	fprintf(stderr, "unknown flag: %s\n", flag);
        	exit(0);
        }
	}

	interface = disksim_interface_initialize(
			parfile, outfile, complete_callback,
			schedule_callback, descheduled_callback,
			(void*) interface, 0, 0);

	DISKSIM_srand48(1);

	malloc_initialize();
	erasure_initialize();

	//struct dm_disk_if *dm = disksim_getdiskmodel(interface, 0);
	//printf("sectors = %d\n", ((int*)dm)[2]);
	//printf("size = %.2fG\n", ((int*)dm)[2] * 512. / 1e9);

	eventq = (event_queue_t*) malloc(sizeof(event_queue_t));
	event_queue_init(eventq);

	meta = (metadata_t*) malloc(sizeof(metadata_t));
	erasure_code_init(meta, get_code_id(code), disks, unit * 2, erasure_req_complete);

	FILE *inp = fopen(inpfile, "r");
	if (inp != NULL)
		event_queue_add(eventq, create_event(0, EVENT_TRACE_FETCH, inp));
	//if (stop > 0)
	//	event_queue_add(eventq, create_event_node(stop, EVENT_STOP_SIM, NULL));

	int tm = timer_index();
	timer_start(tm);
	int cur = 0, numreqs = 0;

	int stop_simulation = 0;
	double checkpoint = 0;
	while (!stop_simulation) {
		event_node_t *node = event_queue_pop(eventq);
		if (node == NULL)
			break; // stop simulation
		currtime = node->time;
		//printf("time = %f, type = %d\n", node->time, node->type);
		switch (node->type) {
		case EVENT_STOP_SIM:
			stop_simulation = 1;
			break;
		case EVENT_TRACE_FETCH:
			//printf("time = %f, type = EVENT_TRACE_FETCH\n", node->time);
			trace_add_next((FILE*)node->ctx);
			if (cur < numreqs) {
				ioreq_t *req = create_ioreq();
				req->time = cur * 30 * scale;
				req->flag = DISKSIM_WRITE;
				req->blkno = cur * (unit * 2);
				req->bcount = unit * 2;
				req->reqno = ++reqno;
				req->reqtype = REQ_TYPE_NORMAL;
				event_queue_add(eventq, create_event(req->time, EVENT_TRACE_MAPREQ, req));
				event_queue_add(eventq, create_event(req->time, EVENT_TRACE_FETCH, node->ctx));
				cur += 1;
			}
			break;
		case EVENT_TRACE_MAPREQ:
			//printf("time = %f, type = EVENT_TRACE_ITEM\n", node->time);
			erasure_handle_request(node->time, meta, (ioreq_t*)node->ctx);
			break;
		case EVENT_IO_INTERNAL:
			//printf("time = %f, type = EVENT_IO_INTERNAL\n", node->time);
			disksim_interface_internal_event(interface, node->time, node->ctx);
			break;
		case EVENT_IO_COMPLETE:
			//printf("time = %f, type = EVENT_IO_COMPLETE\n", node->time);
			sh_request_complete(node->time, (struct disksim_request*)node->ctx);
			disksim_free(dr_idx, node->ctx);
			break;
		default:
			printf("time = %f, type = INVALID\n", node->time);
			exit(-1);
		}
		disksim_free(en_idx, node);
		if ((--limit) == 0) break;
		if (currtime > checkpoint) {
			putchar('*');
			fflush(stdout);
			checkpoint += 60000;
		}
	}
	disksim_interface_shutdown(interface, currtime);

	timer_stop(tm);
	long long duration = timer_microsecond(tm);
	printf("\n");
	printf("===================================================\n");
	printf("Trace File = %s\n", inpfile);
	printf("Disks = %d\n", disks);
	printf("Prime Number = %d\n", meta->pr);
	printf("Unit Size = %d\n", unit);
	printf("Avg. Response Time = %f ms\n", avg_response_time());
	printf("Code = %s\n", get_code_name(get_code_id(code)));
	printf("Total Simulation Time = %.3f s\n", currtime / 1000.0);
	printf("Experiment Duration = %.3f s\n", duration / 1000.0);

	FILE *exp = fopen(result, "a");
	if (exp != NULL) {
		fprintf(exp, "===================================================\n");
		fprintf(exp, "Trace File = %s\n", inpfile);
		fprintf(exp, "Disks = %d\n", disks);
		fprintf(exp, "Prime Number = %d\n", meta->pr);
		fprintf(exp, "Unit Size = %d\n", unit);
		fprintf(exp, "Avg. Response Time = %f ms\n", avg_response_time());
		fprintf(exp, "Code = %s\n", get_code_name(get_code_id(code)));
		fprintf(exp, "Total Simulation Time = %.3f s\n", currtime / 1000.0);
		fprintf(exp, "Experiment Duration = %.3f s\n", duration / 1000.0);
		fclose(exp);
	}
	return 0;
}

