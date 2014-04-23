/*
 * disksim_timer.h
 *
 *  Created on: Apr 21, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_TIMER_H_
#define DISKSIM_TIMER_H_

void timer_start(int handle);
void timer_end(int handle);
void timer_reset(int handle);
void timer_reset_all();
long long timer_millisecond(int handle);
long long timer_microsecond(int handle);

#endif /* DISKSIM_TIMER_H_ */
