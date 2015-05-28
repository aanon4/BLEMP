/*
 * advertising.h
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#ifndef ADVERTISING_H_
#define ADVERTISING_H_

#define APP_ADV_INTERVAL                   1000 // The advertising interval in ms
#define APP_ADV_TIMEOUT_IN_SECONDS            0 // No advertising timeout

#define	APP_ADV_UUID						0x21, 0xAB, 0xBA, 0x78, 0x15, 0x75, 0x4D, 0xBB, 0x9C, 0x8B, 0xCB, 0x68, 0x08, 0xB2, 0x30, 0xED
//#define	APP_ADV_UUID						0x22, 0xAB, 0xBA, 0x78, 0x15, 0x75, 0x4D, 0xBB, 0x9C, 0x8B, 0xCB, 0x68, 0x08, 0xB2, 0x30, 0xED

extern void advertising_init(void);
extern void advertising_start(void);
extern void advertising_stop(void);

#endif /* ADVERTISING_H_ */
