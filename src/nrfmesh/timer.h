/*
 * timer.h
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#ifndef TIMER_H_
#define TIMER_H_

#include <app_timer.h>

extern uint32_t oneminutetimer_value;

#define APP_TIMER_PRESCALER            32 // Value of the RTC1 PRESCALER register. Tick = 32768/PRESCALER
#define APP_TIMER_MAX_TIMERS            7 // Maximum number of simultaneously created timers.
#define APP_TIMER_OP_QUEUE_SIZE         4 // Size of timer operation queues.

#define	MS_TO_TICKS(MS)					      (APP_TIMER_TICKS(MS, APP_TIMER_PRESCALER))

#define TIMER_N_MINUTES(MINS)         (oneminutetimer_value % (MINS) == 0)

#endif /* TIMER_H_ */
