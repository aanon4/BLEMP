/*
 * keepalive.h
 *
 *  Created on: Mar 25, 2015
 *      Author: tim
 */

#ifndef KEEPALIVE_H_
#define KEEPALIVE_H_

extern void meshkeepalive_init(void);
extern void meshkeepalive_timer_handler(void);

#if defined(TESTING_KEEPALIVE)
extern void meshkeepalive_ble_event(ble_evt_t* event);
#endif

#endif /* KEEPALIVE_H_ */
