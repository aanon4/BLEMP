/*
 * statistics.h
 *
 *  Created on: Apr 25, 2015
 *      Author: tim
 */

#ifndef STATISTICS_H_
#define STATISTICS_H_

#if defined(INCLUDE_STATISTICS)

struct statistics
{
	uint32_t	node_count;
	uint32_t	neighbor_count;
	uint32_t	value_count;
	uint32_t	retry_node_count;
	uint32_t	retry_count;
	uint32_t	retry_total_time_ms;
	uint32_t	connections_in_success_count;
	uint32_t	connections_in_total_time_ms;
	uint32_t	connections_out_count;
	uint32_t	connections_out_success_count;
	uint32_t	connections_out_total_time_ms;
	uint32_t	connecting_out_total_time_ms;
	uint32_t	disconnect_count;
	uint32_t 	disconnecting_total_time_ms;
	uint32_t	discover_count;
	uint32_t	discover_total_time_ms;
	uint32_t	scan_start_count;
	uint32_t	scan_nostart_count;
	uint32_t	scan_complete_count;
	uint32_t	invalid_node_count;
	uint32_t	connection_timeout_count;
	uint32_t	master_count;
	uint32_t	master_time_total_ms;
	uint32_t	read_in_count;
	uint32_t	read_out_count;
	uint32_t	read_out_total_time_ms;
	uint32_t	write_in_count;
	uint32_t	write_out_count;
	uint32_t	write_out_total_time_ms;
};

struct statistics_timer
{
	uint32_t	connections_in_total_time_ms;
	uint32_t	connections_out_total_time_ms;
	uint32_t	connecting_out_total_time_ms;
	uint32_t	discover_total_time_ms;
	uint32_t 	disconnecting_total_time_ms;
	uint32_t	master_time_total_ms;
	uint32_t	read_out_total_time_ms;
	uint32_t	write_out_total_time_ms;
};

extern struct statistics stats;
extern struct statistics_timer stats_timer;

extern void statistics_init(void);
extern uint32_t statistics_get_time(void);

#define	STAT_RECORD_SET(FIELD, VALUE)	stats.FIELD = (VALUE)
#define	STAT_RECORD_ADD(FIELD, VALUE)	stats.FIELD += (VALUE)
#define	STAT_RECORD_INC(FIELD)			stats.FIELD++
#define	STAT_TIMER_START(FIELD)			stats_timer.FIELD = statistics_get_time()
#define	STAT_TIMER_END(FIELD)			do { if (stats_timer.FIELD) stats.FIELD += statistics_get_time() - stats_timer.FIELD; stats_timer.FIELD = 0; } while(0)

#else

#define	STAT_RECORD_SET(FIELD, VALUE)
#define	STAT_RECORD_ADD(FIELD, VALUE)
#define	STAT_RECORD_INC(FIELD)

#endif

#endif /* STATISTICS_H_ */
