/*
 * advertising.h
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#ifndef ADVERTISING_H_
#define ADVERTISING_H_

#define APP_ADV_INTERVAL                   2000 // The advertising interval in ms
#define APP_ADV_TIMEOUT_IN_SECONDS            0 // No advertising timeout

#define _REVERSE_UUID(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) P,O,N,M,L,K,J,I,H,G,F,E,D,C,B,A
#define REVERSE_UUID(U) _REVERSE_UUID(U)

extern void advertising_init(void);
extern void advertising_set_0(void);
extern void advertising_set_1(void);
extern void advertising_start(void);
extern void advertising_stop(void);

#endif /* ADVERTISING_H_ */
