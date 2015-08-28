/*
 * sensortimer.c
 *
 *  Created on: Apr 2, 2015
 *      Author: tim
 */

#include <app_timer.h>
#include <app_scheduler.h>
#include <ble.h>

#include "timer.h"
#include "oneminutetimer.h"
#include "nrfmesh.h"


static app_timer_id_t timer;
uint32_t oneminutetimer_value;

static void handler(void* dummy, uint16_t size)
{
  nrfmesh_timer_handler();
  oneminutetimer_other_timer_handlers();
}

static void handler_irq(void* dummy)
{
  uint32_t err_code;

  oneminutetimer_value++;
	err_code = app_sched_event_put(NULL, 0, handler);
  APP_ERROR_CHECK(err_code);
}

void oneminutetimer_init(void)
{
	uint32_t err_code;

	// Setup timer to read temperature every so often
	err_code = app_timer_create(&timer, APP_TIMER_MODE_REPEATED, handler_irq);
	APP_ERROR_CHECK(err_code);
	err_code = app_timer_start(timer, MS_TO_TICKS(ONEMINUTE_TIMER_MS), NULL);
	APP_ERROR_CHECK(err_code);
}
