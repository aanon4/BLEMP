/*
 * temperature.h
 *
 *  Created on: Mar 16, 2015
 *      Author: tim
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#define	TEMPERATURE_ADDRESS			 0x90
#define	TEMPERATURE_WAIT_MS			  200

extern const Mesh_Key MESH_KEY_TEMPERATURE;

extern void temperature_init(void);
extern void temperature_timer_handler(void);

#endif /* TEMPERATURE_H_ */
