/*
 * DiskSim Storage Subsystem Simulation Environment (Version 4.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001-2008.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */



/*
 * DiskSim Storage Subsystem Simulation Environment (Version 2.0)
 * Revision Authors: Greg Ganger
 * Contributors: Ross Cohen, John Griffin, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 1999.
 *
 * Permission to reproduce, use, and prepare derivative works of
 * this software for internal use is granted provided the copyright
 * and "No Warranty" statements are included with all reproductions
 * and derivative works. This software may also be redistributed
 * without charge provided that the copyright and "No Warranty"
 * statements are included in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 */


#include "disksim_global.h"
#include "disksim_malloc.h"

/* GROK -- temporary, while this is in progress */
#include <stdlib.h>

void *DISKSIM_malloc (int size)
{
	/*     void *addr; */

	/*     if (disksim == NULL) { */
	/*        fprintf (stderr, "DISKSIM_malloc: disksim structure not yet initialized\n"); */
	/*        exit(1); */
	/*     } */

	/*     addr = disksim->startaddr + disksim->curroffset; */

	/*     if ((disksim->totallength - disksim->curroffset) >= size) { */
	/*        disksim->curroffset += rounduptomult(size,8); */
	/*     }  */
	/*     else { */
	/*       fprintf (stderr, "*** error: DISKSIM_malloc(): allocated space for disksim run has run out (%d)\n", disksim->totallength); */
	/*        exit(1); */
	/*     } */

	/*     return (addr); */
	return malloc(size);

}

#include "disksim_container.h"
#include "disksim_erasure.h"
#include "disksim_event_queue.h"
#include "disksim_interface.h"
#include "disksim_req.h"

int ht_idx; // hash_table_t
int hn_idx; // hash_node_t
int en_idx; // event_node_t
int sh_idx; // stripe_head_t
int wt_idx; // wait_req_t
int dr_idx; // disksim_request
int el_idx; // element_t
int sb_idx; // sub_ioreq

#define INDEX(type) malloc_index(sizeof(struct type))

static int malloc_index(unsigned x)
{
    int n = 0;
    if (x >> 8) {x >>= 8; n += 8;}
    if (x >> 4) {x >>= 4; n += 4;}
    if (x >> 2) {x >>= 2; n += 2;}
    if (x >> 1) {x >>= 1; n += 1;}
    return n;
}

void malloc_initialize()
{
	ht_idx = INDEX(hash_table);
	hn_idx = INDEX(hash_node);
	en_idx = INDEX(event_node);
	sh_idx = INDEX(stripe_head);
	wt_idx = INDEX(wait_request);
	dr_idx = INDEX(disksim_request);
	el_idx = INDEX(element);
	sb_idx = INDEX(sub_ioreq);
}

static void* space[32];

void* disksim_malloc(int index)
{
	int *ptr = space[index], size = (1 << index);
	if (ptr == NULL) {
		int *tmp = (int*) malloc(size << 4); // 16 elements
		int *addr = tmp, *end = tmp + (size << 4);
		while (addr < end) {
			*addr = (int) ptr;
			ptr = addr;
			addr += size;
		}
	}
	void *ret = ptr;
	ptr = (int*) (*ptr);
	return ret;
}

void disksim_free(int index, void *item)
{
	void *ptr = space[index];
	*(int*)item = (int) ptr;
	ptr = item;
}
