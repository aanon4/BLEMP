/*
 * meshtime.h
 *
 *  Created on: Aug 21, 2015
 *      Author: tim
 */

#ifndef NRFMESH_MESHTIME_H_
#define NRFMESH_MESHTIME_H_

extern void meshtime_init(void);
extern void meshtime_timer_handler(void);
extern uint64_t meshtime_tick(void);
extern uint32_t meshtime_currenttime(void);

#endif /* NRFMESH_MESHTIME_H_ */
