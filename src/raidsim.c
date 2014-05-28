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
static int delay = 1000;
static double stop = -1;
static int unit = 16; // size in KB
static long long limit = 0; // maximum operations
static int code = CODE_RDP;
static const char *result = "";
static const char *fail = "";
static const char *parfile = "../valid/12disks.parv";
static const char *outfile = "t.outv";
static const char *inpfile = "";

int help(const char *main) {
	printf("usage: %s [options]:\n", main);
	printf("  options are:\n");
	printf("\t-n, --num     [number]\t number of disks\n");
	printf("\t-u, --unit    [number]\t stripe unit size (KB)\n");
	printf("\t-d, --delay   [number]\t io_stat sampling interval\n");
	printf("\t-s, --stop    [number]\t stop simulation after some seconds");
	printf("\t-i, --input   [string]\t input trace file\n");
	printf("\t-o, --output  [string]\t DiskSim output file\n");
	printf("\t-c, --code    [string]\t erasure code [rdp, evenodd, hcode, xcode, liberation, star, shcode]\n");
	printf("\t-p, --parv    [string]\t parv file\n");
	printf("\t-a, --append  [string]\t append results to file\n");
	printf("\t-f, --failure [string]\t set failed disks separated by ','\n");
	printf("\t-h, --help            \t help information\n");
	return 0;
}

void trace_add_next(FILE *f) {
	ioreq *req = (ioreq*) getfromextraq();
	static char line[201];
	if (fgets(line, 200, f) == NULL) return;
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
	if (stop > 0 && req->time > stop) return;
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
	event_queue_add(eventq, create_event_node(req->time, EVENT_TRACE_MAPREQ, req));
}

void ioreq_maprequest(double time, ioreq *req) {
	struct disksim_request *tmpreq;
	erasure_maprequest(meta, req);
	if (req->groups == 0) {
		fprintf(stderr, "map_request failed.\n");
		exit(-1);
	}
	iostat_ioreq_start(time, req);
	req->curr = req->groups;
	req->curr->cnt = req->curr->numreqs;
	for (tmpreq = req->curr->reqs; tmpreq != NULL; tmpreq = tmpreq->next)
		disksim_interface_request_arrive(interface, time, tmpreq);
}

void ioreq_complete(double time, struct disksim_request *tmpreq) {
	ioreq *req = tmpreq->reqctx;
	iogroup *group, *ng;
	event_queue_add(eventq, create_event_node(time, EVENT_STAT_ADD,
			iostat_create_node(time, tmpreq->devno, tmpreq->bytecount / 512)));
	req->curr->cnt--;
	if (req->curr->cnt == 0) {
		if (req->curr->next != NULL) {
			req->curr = req->curr->next;
			req->curr->cnt = req->curr->numreqs;
			for (tmpreq = req->curr->reqs; tmpreq != NULL; tmpreq = tmpreq->next)
				disksim_interface_request_arrive(interface, time, tmpreq);
		} else { // request complete
			iostat_ioreq_complete(time, req);
			struct disksim_request *nq;
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

void initialize_disk_failure(metadata *meta, const char *s) {
	int i, j, l = strlen(fail), fails = 0;
	for (i = 0; i < l; i++)
		if (isdigit(s[i])) {
			int c = 0;
			for (j = i; j < l && isdigit(s[j]); j++)
				c = (c << 3) + (c << 1) + (s[j] - '0');
			erasure_disk_failure(meta, c);
			fails += 1;
			i = j;
		}
}

void calculate_peak_throughput(double time) {
	iostat_detect_peak(time, delay);
	if (stop < 0 || time < stop)
		event_queue_add(eventq, create_event_node(time + delay, EVENT_STAT_PEAK, 0));
}

void recon_complete_callback(double time, struct disksim_request *dr, void *ctx) {
	event_queue_add(eventq, create_event_node(time, EVENT_IO_INTERNAL, ctx));
	event_queue_add(eventq, create_event_node(time + 1e-9, EVENT_IO_COMPLETE, dr));
}

void recon_schedule_callback(disksim_interface_callback_t fn, double time, void *ctx) {
	event_queue_add(eventq, create_event_node(time, EVENT_IO_INTERNAL, ctx));
}

void recon_descheduled_callback(double time, void *ctx) {
}

int main(int argc, char **argv) {

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
		else if (!strcmp(flag, "-i") || !strcmp(flag, "--input"))
			inpfile = argu;
		else if (!strcmp(flag, "-o") || !strcmp(flag, "--output"))
			outfile = argu;
		else if (!strcmp(flag, "-n") || !strcmp(flag, "--num"))
			disks = atoi(argu);
		else if (!strcmp(flag, "-u") || !strcmp(flag, "--unit"))
			unit = atoi(argu);
        else if (!strcmp(flag, "-d") || !strcmp(flag, "--delay"))
        	delay = atoi(argu);
        else if (!strcmp(flag, "-s") || !strcmp(flag, "--stop"))
        	stop = atof(argu) * 1000;
        else if (!strcmp(flag, "-l") || !strcmp(flag, "--limit"))
        	limit = atol(argu);
        else if (!strcmp(flag, "-a") || !strcmp(flag, "--append"))
        	result = argu;
        else if (!strcmp(flag, "-f") || !strcmp(flag, "--fail"))
        	fail = argu;
        else if (!strcmp(flag, "-c") || !strcmp(flag, "--code"))
        	code = get_code_id(argu);
        else {
        	fprintf(stderr, "unknown flag: %s\n", flag);
        	exit(0);
        }
	}

	interface = disksim_interface_initialize(
			parfile, outfile, recon_complete_callback,
			recon_schedule_callback, recon_descheduled_callback,
			(void*) interface, 0, 0);

	//struct dm_disk_if *dm = disksim_getdiskmodel(interface, 0);
	//printf("sectors = %d\n", ((int*)dm)[2]);
	//printf("size = %.2fG\n", ((int*)dm)[2] * 512. / 1e9);

	DISKSIM_srand48(1);

	eventq = malloc(sizeof(equeue));
	event_queue_initialize(eventq);

	FILE *inp = fopen(inpfile, "r");
	if (inp != NULL)
		event_queue_add(eventq, create_event_node(0, EVENT_TRACE_FETCH, inp));
	//if (stop > 0)
	//	event_queue_add(eventq, create_event_node(stop, EVENT_STOP_SIM, NULL));
	if (delay > 0)
		event_queue_add(eventq, create_event_node(0, EVENT_STAT_PEAK, 0));

	meta = (metadata*) malloc(sizeof(metadata));
	erasure_initialize(meta, code, disks, unit * 2);
	initialize_disk_failure(meta, fail);

	iostat_initialize(disks);
	distr = malloc(sizeof(int) * disks);
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
		//printf("time = %f, type = %d\n", node->time, node->type);
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
			break;
		case EVENT_IO_COMPLETE:
			//printf("time = %f, type = EVENT_IO_COMPLETE\n", node->time);
			ioreq_complete(node->time, (struct disksim_request*)node->ctx);
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
	printf("Trace File = %s\n", inpfile);
	printf("Disks = %d\n", disks);
	printf("Prime Number = %d\n", meta->prime);
	printf("Unit Size = %d\n", unit);
	printf("Code = %s\n", get_code_name(code));
	printf("Total Simulation Time = %f ms\n", currtime);
	printf("Avg. Response Time = %f ms\n", iostat_avg_response_time());
	printf("Avg. XORs per Write = %f\n", iostat_avg_xors_per_write());
	printf("Avg. I/Os per Request = %f\n", iostat_avg_IOs_per_request());
	printf("Throughput = %f MB/s\n", iostat_throughput());
	printf("Peak Throughput = %f MB/s\n", iostat_peak_throughput());
	printf("Experiment Duration = %.3f s\n", duration / 1000.0);

	FILE *exp = fopen(result, "a");
	if (exp != NULL) {
		fprintf(exp, "===================================================\n");
		fprintf(exp, "Trace File = %s\n", inpfile);
		fprintf(exp, "Disks = %d\n", disks);
		fprintf(exp, "Prime Number = %d\n", meta->prime);
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

