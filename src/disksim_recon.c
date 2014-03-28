/*
 * disksim_recon.c
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disksim_event_queue.h"
#include "disksim_interface.h"
#include "disksim_erasure.h"
#include "disksim_iostat.h"
#include "disksim_global.h"
#include "disksim_rand48.h"

#define RECON_DELAY			1

static equeue *eventq;
static metadata *meta; // erasure code metadata
static struct disksim_interface *interface;
static double currtime = 0;
static double scale = 1.0;
static int reqno = 0;
static int method = 0;
static int disks = 12;
static int *distr;
static int stripeno = 0, totalstripes = 0; // test
static int delay = 100;

int help(const char *main) {
	printf("usage: %s [options]:\n", main);
	printf("  options are:\n");
	printf("\t-n, --num     [number]\t number of disks\n");
	printf("\t-u, --unit    [number]\t stripe unit size (KB)\n");
	printf("\t-m, --method  [number]\t methods in [0..8]\n");
	printf("\t-s, --stop    [number]\t stop after reconstruct how many stripes\n");
	printf("\t-d, --delay   [number]\t detect interval\n");
	printf("\t-i, --input   [string]\t input trace file\n");
	printf("\t-o, --output  [string]\t disksim output file\n");
	printf("\t-c, --code    [string]\t erasure code [rdp, evenodd, hcode, xcode, ...]\n");
	printf("\t-p, --parv    [string]\t parv file\n");
	printf("\t-h, --help            \t help information\n");
	return 0;
}

void trace_add_next(FILE *f) {
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
	req->reqtype = TYPE_NORMAL;
	event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_MAPREQ, req));
	event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_FETCH, f));
}

void trace_add_recon(double time) {
	ioreq *req = (ioreq*) getfromextraq();
	req->time = time;
	req->stat = 0;
	req->curr = NULL;
	req->groups = NULL;
	req->reqno = ++reqno; // auto increment ID
	req->reqtype = TYPE_RECON;
	event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_MAPREQ, req));
}

void ioreq_maprequest(double time, ioreq *req) {
	struct disksim_request *tmpreq;
	//if (!(req->reqno & 0xFF)) { printf("reqno = %d %d\n", req->reqno, iostat_get_num_in_table()); fflush(stdout); }
	switch (req->reqtype) {
	case TYPE_NORMAL:
		erasure_maprequest(meta, method, req);
		break;
	case TYPE_RECON:
		iostat_distribution(time, delay, distr);
		erasure_rebuild(meta, distr, method, req);
		break;
	}
	if (req->groups == 0) {
		if (req->reqtype == TYPE_NORMAL) {
			fprintf(stderr, "map_request failed.\n");
			exit(-1);
		} else { // reconstruction complete
			event_queue_add(eventq, create_event_node(req->time, EVENT_STOP_SIM, NULL));
			return;
		}
	}
	if (req->reqtype == TYPE_RECON && (--stripeno) == 0)
		event_queue_add(eventq, create_event_node(req->time, EVENT_STOP_SIM, NULL));
	iostat_ioreq_start(time, req);
	req->curr = req->groups;
	req->curr->cnt = req->curr->numreqs;
	for (tmpreq = req->curr->reqs; tmpreq != NULL; tmpreq = tmpreq->next)
		disksim_interface_request_arrive(interface, time, tmpreq);
}

void ioreq_complete(double time, ioreq *req) {
	struct disksim_request *tmpreq, *nq;
	iogroup *group, *ng;
	req->curr->cnt--;
	if (req->curr->cnt == 0) {
		if (req->curr->next != NULL) {
			req->curr = req->curr->next;
			req->curr->cnt = req->curr->numreqs;
			for (tmpreq = req->curr->reqs; tmpreq != NULL; tmpreq = tmpreq->next)
				disksim_interface_request_arrive(interface, time, tmpreq);
		} else { // request complete
			iostat_ioreq_complete(time, req);
			if (req->reqtype == TYPE_RECON)
				event_queue_add(eventq, create_event_node(time + RECON_DELAY
						, EVENT_TRACE_RECON, 0));
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

void calculate_peak_throughput(double time) {
	iostat_detect_peak(time, delay);
	event_queue_add(eventq, create_event_node(time + 50, EVENT_STAT_PEAK, 0));
}

void recon_complete_callback(double time, struct disksim_request *dr, void *ctx) {
	event_queue_add(eventq, create_event_node(time, EVENT_STAT_ADD,
			iostat_create_node(time, dr->devno, dr->bytecount / 512)));
	event_queue_add(eventq, create_event_node(time, EVENT_IO_COMPLETE, dr->reqctx));
}

void recon_schedule_callback(disksim_interface_callback_t fn, double time, void *ctx) {
	event_queue_add(eventq, create_event_node(time, EVENT_IO_INTERNAL, ctx));
}

void recon_descheduled_callback(double time, void *ctx) {
}

int main(int argc, char **argv) {

	const char *parfile = "../valid/12disks.parv";
	const char *outfile = "temp.outv";
	const char *inpfile = "financial.trace";
	long long limit = 0;
	int code = CODE_RDP;
	int unit = 16; // size in KB
	int i;

	int p = 1;
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
		else if (!strcmp(flag, "-i") || !strcmp(flag, "--input"))
			inpfile = argu;
		else if (!strcmp(flag, "-o") || !strcmp(flag, "--output"))
			outfile = argu;
		else if (!strcmp(flag, "-n") || !strcmp(flag, "--num"))
			disks = atoi(argu);
		else if (!strcmp(flag, "-u") || !strcmp(flag, "--unit"))
			unit = atoi(argu);
        else if (!strcmp(flag, "-m") || !strcmp(flag, "--method"))
            method = atoi(argu);
        else if (!strcmp(flag, "-s") || !strcmp(flag, "--step"))
        	totalstripes = stripeno = atoi(argu);
        else if (!strcmp(flag, "-d") || !strcmp(flag, "--delay"))
        	delay = atoi(argu);
        else if (!strcmp(flag, "-l") || !strcmp(flag, "--limit"))
        	limit = atol(argu);
        else if (!strcmp(flag, "-c") || !strcmp(flag, "--code")) {
			if (!strcmp(argu, "rdp"))
				code = CODE_RDP;
			if (!strcmp(argu, "evenodd"))
				code = CODE_EVENODD;
			if (!strcmp(argu, "hcode"))
				code = CODE_HCODE;
			if (!strcmp(argu, "xcode"))
				code = CODE_XCODE;
        }
	}

	interface = disksim_interface_initialize(
			parfile, outfile, recon_complete_callback,
			recon_schedule_callback, recon_descheduled_callback,
			(void*) interface, 0, 0);

	DISKSIM_srand48(1);

	eventq = malloc(sizeof(equeue));
	event_queue_initialize(eventq);
	event_queue_add(eventq, create_event_node(0, EVENT_TRACE_FETCH, fopen(inpfile, "r")));
	event_queue_add(eventq, create_event_node(0, EVENT_STAT_PEAK, 0));
	event_queue_add(eventq, create_event_node(0, EVENT_TRACE_RECON, 0));

	meta = (metadata*) malloc(sizeof(metadata));
	erasure_initialize(meta, code, disks, unit * 2);
	erasure_failure(meta, 0);

	iostat_initialize(disks);
	distr = malloc(sizeof(int) * disks);

	struct disksim_request *tmpreq;
	int stop_simulation = 0;
	while (!stop_simulation) {
		enode *node = event_queue_pop(eventq);
		if (node == NULL)
			break; // stop simulation
		currtime = node->time;
		//if (node->type != EVENT_IO_INTERNAL) { printf("time = %f, type = %d\n", currtime, node->type); fflush(stdout); }
		switch (node->type) {
		case EVENT_STOP_SIM:
			stop_simulation = 1;
			break;
		case EVENT_TRACE_FETCH:
			//printf("time = %f, type = EVENT_TRACE_FETCH\n", node->time);
			trace_add_next((FILE*)node->ctx);
			break;
		case EVENT_TRACE_RECON:
			//printf("time = %f, type = EVENT_TRACE_RECON\n", node->time);
			trace_add_recon(node->time);
			break;
		case EVENT_TRACE_MAPREQ:
			//printf("time = %f, type = EVENT_TRACE_ITEM\n", node->time);
			ioreq_maprequest(node->time, (ioreq*)node->ctx);
			break;
		case EVENT_IO_INTERNAL:
			//printf("time = %f, type = EVENT_IO_INTERNAL\n", node->time);
			disksim_interface_internal_event(interface, node->time, node->ctx);
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
	}

	printf("===================================================\n");
	printf("Method = %s, Code = %s\n", get_method_name(method), get_code_name(code));
	printf("Total Simulation Time = %f ms\n", currtime);
	printf("Total Stripes Reconstructed = %d\n", totalstripes - stripeno);

	disksim_interface_shutdown(interface, currtime);
	iostat_print();
	return 0;
}

