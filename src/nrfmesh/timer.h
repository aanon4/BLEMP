/*
 * timer.h
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#ifndef TIMER_H_
#define TIMER_H_

#include <app_timer.h>

extern uint32_t meshtimer_value;

#define APP_TIMER_PRESCALER            32 // Value of the RTC1 PRESCALER register. Tick = 32768/PRESCALER
#define APP_TIMER_MAX_TIMERS            7 // Maximum number of simultaneously created timers.
#define APP_TIMER_OP_QUEUE_SIZE         4 // Size of timer operation queues.

#define	MS_TO_TICKS(MS)					      (APP_TIMER_TICKS(MS, APP_TIMER_PRESCALER))

#define TIMER_MINUTES(MINS)           (meshtimer_value == (MINS))
#define TIMER_N_MINUTES(MINS)         (meshtimer_value % (MINS) == 0)

// The one minute timer is use to hang many period things from. This lets us do many
// things when we wake the device up rather than having lots of different timers
// continually waking the device. The timer is 1 minute and SHOULD NOT BE CHANGED
// without checking everyone who uses it.
#define ONEMINUTE_TIMER_MS    (60 * 1000) // 1 minute

extern void meshtimer_init(void);
extern void meshtimer_other_timer_handlers(void);

#endif /* TIMER_H_ */
