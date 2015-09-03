/*
 * meshsystem.c
 *
 *  Created on: Mar 23, 2015
 *      Author: tim
 */

#include <nordic_common.h>
#include <ble.h>
#include <ble_gatts.h>
#include <ble_gap.h>
#include <ble_hci.h>
#include <app_error.h>
#include <nrf_soc.h>
#include <app_scheduler.h>

#include "mesh/mesh.h"
#include "mesh/meshsystem.h"

#include "services/gap.h"

#include "uuids.h"
#include "advertising.h"
#include "timer.h"
#include "nrfmesh.h"
#include "secure.h"
#include "meshinfo.h"
#include "meshtime.h"
#include "statistics.h"

static void retry_handler_irq(void* context);

static struct
{
	uint16_t	conn_handle;
	uint16_t	sync_handle;
	uint16_t	attr_handle;
	app_timer_id_t timer;
  uint8_t   rediscover_needed;
} mesh_state =
{
	.conn_handle = BLE_CONN_HANDLE_INVALID,
	.sync_handle = BLE_CONN_HANDLE_INVALID,
	.attr_handle = BLE_CONN_HANDLE_INVALID,
};
Mesh_Node mesh_node;

//#define DEBUG_EVENTS  1

#define MAX_CONCURRENT_CONNECTION_ATTEMPTS  1

#if defined(DEBUG_EVENTS)

#define DEBUG_EVENT_MAX   256
struct debug_event
{
  uint8_t   evt_id;
} debug_events[DEBUG_EVENT_MAX];

#endif

void nrfmesh_init(void)
{
	uint32_t err_code;

  uuids_init();
  meshtimer_init();

	// Add Mesh service
	static const ble_gatts_attr_md_t metadata =
	{
		.read_perm = { 1, 3 },
		.write_perm = { 1, 3 },
		.rd_auth = 1,
		.wr_auth = 1,
		.vlen = 1,
		.vloc = BLE_GATTS_VLOC_STACK
	};
	static const ble_uuid_t uuid =
	{
		.type = UUIDS_BASE_TYPE,
		.uuid = MESH_SYNC_UUID
	};
	static const ble_gatts_attr_t attr =
	{
		.p_uuid = (ble_uuid_t*)&uuid,
		.p_attr_md = (ble_gatts_attr_md_t*)&metadata,
		.init_len = 0,
		.max_len = MESH_BUFFER_SIZE
	};
	static const ble_gatts_char_md_t characteristic =
	{
		.char_props.read = 1,
		.char_props.write = 1,
		.p_char_user_desc = "Mesh",
		.char_user_desc_max_size = 4,
		.char_user_desc_size = 4
	};
	ble_gatts_char_handles_t newhandle;
	err_code = sd_ble_gatts_characteristic_add(primary_service_handle, &characteristic, &attr, &newhandle);
	APP_ERROR_CHECK(err_code);
	mesh_state.sync_handle = newhandle.value_handle;

	ble_gap_addr_t mac;
	err_code = sd_ble_gap_address_get(&mac);
	APP_ERROR_CHECK(err_code);
	Mesh_NodeReset(&mesh_node, (Mesh_NodeAddress*)mac.addr);

	err_code = app_timer_create(&mesh_state.timer, APP_TIMER_MODE_SINGLE_SHOT, &retry_handler_irq);
	APP_ERROR_CHECK(err_code);

#if defined(INCLUDE_STATISTICS)
  statistics_init();
#endif
  secure_init();
  meshinfo_init();
  meshtime_init();
}

void nrfmesh_start(void)
{
  uint32_t err_code;

  static const ble_gap_scan_params_t scan =
  {
    .interval = SCAN_INTERVAL,
    .window = SCAN_WINDOW,
    .timeout = SCAN_TIMEOUT
  };
  STAT_RECORD_INC(scan_start_count);
  err_code = sd_ble_gap_scan_start(&scan);
  APP_ERROR_CHECK(err_code);
}

void nrfmesh_timer_handler(void)
{
  meshtime_timer_handler();

	if (mesh_state.rediscover_needed)
	{
		static const ble_gap_scan_params_t scan =
		{
			.interval = SCAN_INTERVAL,
			.window = SCAN_WINDOW,
			.timeout = SCAN_TIMEOUT
		};
		if (mesh_node.state == MESH_STATE_IDLE && sd_ble_gap_scan_start(&scan) == NRF_SUCCESS)
    {
      STAT_RECORD_INC(scan_start_count);
      mesh_state.rediscover_needed = 0;
    }
    else
    {
      STAT_RECORD_INC(scan_nostart_count);
    }
	}
}

void nrfmesh_meshchange(Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length)
{
  secure_meshchange(id, key, value, length);
}

void nrfmesh_ble_event(ble_evt_t* event)
{
	static const uint8_t advert[] =
	{
		0x02, BLE_GAP_AD_TYPE_FLAGS, BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
		0x1A, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0x4C, 0x00,
		0x02, 0x15,
		REVERSE_UUID(MESH_SERVICE_BASE_UUID)
	};

	uint32_t err_code;

#if defined(DEBUG_EVENTS)
	{
    switch (event->header.evt_id)
    {
    case BLE_GAP_EVT_RSSI_CHANGED:
      break;

    default:
      memmove(debug_events + 1, debug_events, sizeof(struct debug_event) * (DEBUG_EVENT_MAX - 1));
      debug_events[0].evt_id = event->header.evt_id;
      break;
    }
	}
#endif

	secure_ble_event(event);

	switch (event->header.evt_id)
	{
	case BLE_GAP_EVT_ADV_REPORT:
		if (event->evt.gap_evt.params.adv_report.peer_addr.addr_type == BLE_GAP_ADDR_TYPE_RANDOM_STATIC && event->evt.gap_evt.params.adv_report.dlen >= sizeof(advert) && Mesh_System_memcmp(advert, event->evt.gap_evt.params.adv_report.data, sizeof(advert)) == 0)
    {
      Mesh_NodeId id = Mesh_InternNodeId(&mesh_node, (Mesh_NodeAddress*)event->evt.gap_evt.params.adv_report.peer_addr.addr, 1);
      if (id != MESH_NODEID_SELF)
      {
        Mesh_Process(&mesh_node, MESH_EVENT_NEIGHBOR_DISCOVER, id, (uint32_t)(int32_t)event->evt.gap_evt.params.adv_report.rssi);
      }
		}
		break;

	case BLE_GAP_EVT_CONNECTED:
#if defined(INCLUDE_STATISTICS)
		{
			uint32_t count;
			count = 0;
			for (Mesh_NodeId i = 0; i < MESH_MAX_NODES; i++)
			{
				if (mesh_node.ids[i].flag.ping && !mesh_node.ids[i].flag.blacklisted)
				{
					count++;
				}
			}
			STAT_RECORD_SET(node_count, count);
			count = 0;
			for (uint8_t i = 0; i < MESH_MAX_NEIGHBORS; i++)
			{
				if (mesh_node.neighbors.neighbors[i].id)
				{
					count++;
				}
			}
			STAT_RECORD_SET(neighbor_count, count);
			STAT_RECORD_SET(value_count, mesh_node.values.count);
		}
#endif
		if (event->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH)
		{
		  if (Mesh_Process(&mesh_node, MESH_EVENT_INCOMINGCONNECTION, Mesh_InternNodeId(&mesh_node, (Mesh_NodeAddress*)event->evt.gap_evt.params.connected.peer_addr.addr, 1), 0) == MESH_OK)
		  {
        mesh_state.conn_handle = event->evt.gap_evt.conn_handle;
        STAT_RECORD_INC(connections_in_success_count);
        STAT_TIMER_START(connections_in_total_time_ms);
        err_code = app_timer_stop(mesh_state.timer);
        APP_ERROR_CHECK(err_code);
        err_code = sd_ble_gap_rssi_start(event->evt.gap_evt.conn_handle, RSSI_THRESHOLD, RSSI_SKIPCOUNT);
        APP_ERROR_CHECK(err_code);
      }
      else
      {
        // If we cannot accept the incoming connection, we terminate immediately
        err_code = sd_ble_gap_disconnect(event->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
      }
		}
		else
		{
			mesh_state.conn_handle = event->evt.gap_evt.conn_handle;
			// Resolve address to relevant neighbor
			Mesh_NodeId id;
			if (event->evt.gap_evt.params.connected.irk_match)
      {
        id = MESH_NODEID_CLIENT;
        if (mesh_node.state != MESH_STATE_SYNCMASTERCONNECTING)
        {
          APP_ERROR_CHECK(NRF_ERROR_INVALID_STATE);
        }
      }
      else
      {
        id = Mesh_InternNodeId(&mesh_node, (Mesh_NodeAddress*)event->evt.gap_evt.params.connected.peer_addr.addr, 0);
        if (id == MESH_NODEID_INVALID)
        {
			    APP_ERROR_CHECK(NRF_ERROR_INVALID_STATE);
			  }
			}
			if (Mesh_FindNeighbor(&mesh_node, id, &mesh_node.sync.neighbor) != MESH_OK)
			{
			  // Unexpected failure - we should always know the neighbor we're connecting to!
			  APP_ERROR_CHECK(NRF_ERROR_INVALID_STATE);
			}
			else if (mesh_node.sync.neighbor->handle)
			{
			  connected:;
				mesh_state.attr_handle = mesh_node.sync.neighbor->handle;
				STAT_RECORD_INC(connections_out_success_count);
				STAT_TIMER_END(connecting_out_total_time_ms);
				secure_authenticate(mesh_state.conn_handle);
		    err_code = sd_ble_gap_rssi_start(event->evt.gap_evt.conn_handle, RSSI_THRESHOLD, RSSI_SKIPCOUNT);
		    APP_ERROR_CHECK(err_code);
			}
			else
			{
				static const ble_gattc_handle_range_t discover =
				{
					.start_handle = 1,
					.end_handle = BLE_GATTC_HANDLE_END
				};
				STAT_RECORD_INC(discover_count);
				STAT_TIMER_START(discover_total_time_ms);
				err_code = sd_ble_gattc_characteristics_discover(mesh_state.conn_handle, &discover);
				APP_ERROR_CHECK(err_code);
			}
		}
		break;

	case BLE_GATTC_EVT_CHAR_DISC_RSP:
		if (event->evt.gattc_evt.conn_handle == mesh_state.conn_handle)
		{
			if (event->evt.gattc_evt.gatt_status == NRF_SUCCESS)
			{
				for (uint16_t i = 0; i < event->evt.gattc_evt.params.char_disc_rsp.count; i++)
				{
					ble_gattc_char_t* chr = &event->evt.gattc_evt.params.char_disc_rsp.chars[i];
					if (chr->uuid.type == UUIDS_BASE_TYPE && chr->uuid.uuid == MESH_SYNC_UUID)
					{
						mesh_node.sync.neighbor->handle = chr->handle_value;
						STAT_RECORD_INC(discover_success_count);
		        STAT_TIMER_END(discover_total_time_ms);
						goto connected;
					}
				}
				ble_gattc_handle_range_t discover =
				{
					.start_handle = event->evt.gattc_evt.params.char_disc_rsp.chars[event->evt.gattc_evt.params.char_disc_rsp.count - 1].handle_value + 1,
					.end_handle = BLE_GATTC_HANDLE_END
				};
				err_code = sd_ble_gattc_characteristics_discover(mesh_state.conn_handle, &discover);
				APP_ERROR_CHECK(err_code);
			}
			else
			{
				// Mesh service not available
				STAT_RECORD_INC(invalid_node_count);
				STAT_TIMER_END(discover_total_time_ms);
				if (!mesh_node.ids[mesh_node.sync.neighbor->id].flag.client)
				{
				  Mesh_Process(&mesh_node, MESH_EVENT_INVALIDNODE, 0, 0);
				}
			}
		}
		break;

	case BLE_GAP_EVT_DISCONNECTED:
	  if (event->evt.gap_evt.conn_handle == mesh_state.conn_handle || mesh_state.conn_handle == BLE_CONN_HANDLE_INVALID)
	  {
      STAT_RECORD_INC(disconnect_count);
      STAT_TIMER_END(connections_in_total_time_ms);
      STAT_TIMER_END(connections_out_total_time_ms);
      STAT_TIMER_END(disconnecting_total_time_ms);
      mesh_state.conn_handle = BLE_CONN_HANDLE_INVALID;
      // The GATT in clients changes often so we cannot cache it
      if (mesh_node.sync.neighbor && mesh_node.ids[mesh_node.sync.neighbor->id].flag.client)
      {
        mesh_node.sync.neighbor->handle = 0;
      }
      Mesh_Process(&mesh_node, MESH_EVENT_DISCONNECTED, 0, 0);
	  }
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
	    if (event->evt.gap_evt.conn_handle == mesh_state.conn_handle && event->evt.gap_evt.params.auth_status.auth_status == BLE_GAP_SEC_STATUS_SUCCESS)
      {
        Mesh_Process(&mesh_node, MESH_EVENT_CONNECTED, 0, 0);
      }
	    break;

	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		switch (event->evt.gatts_evt.params.authorize_request.type)
		{
		case BLE_GATTS_AUTHORIZE_TYPE_WRITE:
			if (event->evt.gatts_evt.params.authorize_request.request.write.handle == mesh_state.sync_handle)
			{
        int8_t rssi;
        err_code = sd_ble_gap_rssi_get(mesh_state.conn_handle, &rssi);
        APP_ERROR_CHECK(err_code);
        mesh_node.sync.bufferlen = MIN(sizeof(mesh_node.sync.buffer), event->evt.gatts_evt.params.authorize_request.request.write.len);
        Mesh_System_memmove(mesh_node.sync.buffer, event->evt.gatts_evt.params.authorize_request.request.write.data, mesh_node.sync.bufferlen);
        Mesh_Process(&mesh_node, MESH_EVENT_WRITE, 0, rssi);
        static const ble_gatts_rw_authorize_reply_params_t reply =
        {
          .type = BLE_GATTS_AUTHORIZE_TYPE_WRITE,
          .params.write.gatt_status = BLE_GATT_STATUS_SUCCESS
        };
        STAT_RECORD_INC(write_in_count);
        err_code = sd_ble_gatts_rw_authorize_reply(mesh_state.conn_handle, &reply);
        APP_ERROR_CHECK(err_code);
			}
			break;

		case BLE_GATTS_AUTHORIZE_TYPE_READ:
			if (event->evt.gatts_evt.params.authorize_request.request.read.handle == mesh_state.sync_handle)
			{
        int8_t rssi;
        err_code = sd_ble_gap_rssi_get(mesh_state.conn_handle, &rssi);
        APP_ERROR_CHECK(err_code);
        mesh_node.sync.bufferlen = 0;
        Mesh_Process(&mesh_node, MESH_EVENT_READING, 0, rssi);
        ble_gatts_rw_authorize_reply_params_t reply =
        {
          .type = BLE_GATTS_AUTHORIZE_TYPE_READ,
          .params.read =
          {
            .gatt_status = BLE_GATT_STATUS_SUCCESS,
            .update = 1,
            .offset = 0,
            .len = mesh_node.sync.bufferlen,
            .p_data = mesh_node.sync.buffer
          }
        };
        STAT_RECORD_INC(read_in_count);
        err_code = sd_ble_gatts_rw_authorize_reply(mesh_state.conn_handle, &reply);
        APP_ERROR_CHECK(err_code);
			}
			break;

		default:
			break;
		}
		break;

	case BLE_GATTC_EVT_READ_RSP:
		if (event->evt.gattc_evt.params.read_rsp.handle == mesh_state.attr_handle)
		{
			STAT_RECORD_INC(read_out_count);
			STAT_TIMER_END(read_out_total_time_ms);
			int8_t rssi;
			err_code = sd_ble_gap_rssi_get(mesh_state.conn_handle, &rssi);
			APP_ERROR_CHECK(err_code);
			mesh_node.sync.bufferlen = MIN(sizeof(mesh_node.sync.buffer), event->evt.gattc_evt.params.read_rsp.len);
			Mesh_System_memmove(mesh_node.sync.buffer, event->evt.gattc_evt.params.read_rsp.data, mesh_node.sync.bufferlen);
			Mesh_Process(&mesh_node, MESH_EVENT_READ, 0, rssi);
		}
		break;

	case BLE_GATTC_EVT_WRITE_RSP:
		if (event->evt.gattc_evt.params.write_rsp.handle == mesh_state.attr_handle)
		{
			STAT_RECORD_INC(write_out_count);
			STAT_TIMER_END(write_out_total_time_ms);
			int8_t rssi;
			err_code = sd_ble_gap_rssi_get(mesh_state.conn_handle, &rssi);
			APP_ERROR_CHECK(err_code);
			Mesh_Process(&mesh_node, MESH_EVENT_WROTE, 0, rssi);
		}
		break;

	case BLE_GAP_EVT_TIMEOUT:
		switch (event->evt.gap_evt.params.timeout.src)
		{
		case BLE_GAP_TIMEOUT_SRC_SCAN:
			STAT_RECORD_INC(scan_complete_count);
			Mesh_Process(&mesh_node, MESH_EVENT_SYNC, 0, 0);
			break;

		case BLE_GAP_TIMEOUT_SRC_CONN:
		{
		  STAT_RECORD_INC(connection_timeout_count);
		  // Flag each neighbor we failed to connect to
		  uint8_t addr_count = 0;
		  uint8_t total_count = 0;
		  Mesh_ChangeBits remainingbits = mesh_node.sync.remainingbits;
		  for (Mesh_Neighbor* neighbor = &mesh_node.neighbors.neighbors[0]; neighbor < &mesh_node.neighbors.neighbors[MESH_MAX_NEIGHBORS]; neighbor++)
		  {
		    if (neighbor->flag.valid && mesh_node.ids[neighbor->id].flag.client && !neighbor->flag.retry && (remainingbits & MESH_NEIGHBOR_TO_CHANGEBIT(&mesh_node, neighbor)))
		    {
		      neighbor->flag.retry = 1;
		      neighbor->retries++;
		      total_count++;
		    }
		  }
		  if (total_count == 0)
		  {
		    for (Mesh_Neighbor* neighbor = &mesh_node.neighbors.neighbors[0]; total_count < MAX_CONCURRENT_CONNECTION_ATTEMPTS && neighbor < &mesh_node.neighbors.neighbors[MESH_MAX_NEIGHBORS] && addr_count < BLE_GAP_WHITELIST_ADDR_MAX_COUNT; neighbor++)
		    {
		      if (neighbor->flag.valid && !mesh_node.ids[neighbor->id].flag.client && !neighbor->flag.retry && (remainingbits & MESH_NEIGHBOR_TO_CHANGEBIT(&mesh_node, neighbor)))
		      {
		        neighbor->flag.retry = 1;
            neighbor->retries++;
            total_count++;
            addr_count++;
		      }
		    }
		  }
		  Mesh_Process(&mesh_node, MESH_EVENT_CONNECTIONFAILED, 0, 0);
		  break;
		}

		default:
			break;
		}
		break;

	case BLE_GAP_EVT_RSSI_CHANGED:
		 break;

	default:
		break;
	}
}

static void retry_handler(void* dummy, uint16_t size)
{
	if (Mesh_Process(&mesh_node, MESH_EVENT_RETRY, 0, 0) != MESH_OK)
	{
		Mesh_System_Retry(&mesh_node, 1);
	}
}

static void retry_handler_irq(void* dummy)
{
  uint32_t err_code;

	err_code = app_sched_event_put(NULL, 0, retry_handler);
	APP_ERROR_CHECK(err_code);
}

void Mesh_System_MasterMode(Mesh_Node* node)
{
	uint32_t err_code;

	STAT_RECORD_INC(master_count);
	STAT_TIMER_START(master_time_total_ms);
	advertising_stop();
	err_code = app_timer_stop(mesh_state.timer);
	APP_ERROR_CHECK(err_code);
	Mesh_Process(&mesh_node, MESH_EVENT_INMASTERMODE, 0, 0);
}

void Mesh_System_PeripheralMode(Mesh_Node* node)
{
	Mesh_Process(&mesh_node, MESH_EVENT_INPERIPHERALMODE, 0, 0);
	advertising_start();
	STAT_TIMER_END(master_time_total_ms);
}

void Mesh_System_PeripheralDone(Mesh_Node* node)
{
	advertising_start();
}

Mesh_Status Mesh_System_Connect(Mesh_Node* node)
{
	uint32_t err_code;

  static const ble_gap_conn_params_t conn =
  {
    .min_conn_interval = MIN_CONN_INTERVAL,
    .max_conn_interval = MAX_CONN_INTERVAL,
    .slave_latency = SLAVE_LATENCY,
    .conn_sup_timeout = CONN_SUP_TIMEOUT
  };

  STAT_RECORD_INC(connections_out_count);
  STAT_TIMER_START(connections_out_total_time_ms);
  STAT_TIMER_START(connecting_out_total_time_ms);

  // Rather than connect to a specific neighbor, we attempt to connect to many at once. Only one will succeed, which we will then
  // handle in the usual way. By looking for many at once, we will connect to the first we see which should speed up the syncing
  // process and reduce overall power usage.
  ble_gap_addr_t addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
  ble_gap_addr_t* p_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
  ble_gap_irk_t irks[BLE_GAP_WHITELIST_IRK_MAX_COUNT];
  ble_gap_irk_t* p_irks[BLE_GAP_WHITELIST_IRK_MAX_COUNT];
  uint8_t addr_count = 0;
  uint8_t irk_count = 0;
  uint8_t total_count = 0;

  // Prioritize clients
  Mesh_ChangeBits remainingbits = node->sync.remainingbits;
  for (Mesh_Neighbor* neighbor = &node->neighbors.neighbors[0]; neighbor < &node->neighbors.neighbors[MESH_MAX_NEIGHBORS] && addr_count < BLE_GAP_WHITELIST_ADDR_MAX_COUNT; neighbor++)
  {
    if (neighbor->flag.valid && node->ids[neighbor->id].flag.client && !neighbor->flag.retry && (remainingbits & MESH_NEIGHBOR_TO_CHANGEBIT(node, neighbor)))
    {
      irk_count = secure_get_irks(irks);
      for (uint8_t i = irk_count; i--; )
      {
        p_irks[i] = &irks[i];
      }
      total_count += irk_count;
      break;
    }
  }
  if (total_count == 0)
  {
    for (Mesh_Neighbor* neighbor = &node->neighbors.neighbors[0]; total_count < MAX_CONCURRENT_CONNECTION_ATTEMPTS && neighbor < &node->neighbors.neighbors[MESH_MAX_NEIGHBORS] && addr_count < BLE_GAP_WHITELIST_ADDR_MAX_COUNT; neighbor++)
    {
      if (neighbor->flag.valid && !node->ids[neighbor->id].flag.client && !neighbor->flag.retry && (remainingbits & MESH_NEIGHBOR_TO_CHANGEBIT(node, neighbor)))
      {
        addrs[addr_count].addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
        Mesh_System_memmove(&addrs[addr_count].addr, &node->ids[neighbor->id].address, sizeof(Mesh_NodeAddress));
        p_addrs[addr_count] = &addrs[addr_count];
        addr_count++;
        total_count++;
      }
    }
  }

  // If we have nothing to do, return now
  if (total_count == 0)
  {
    return MESH_NOTFOUND;
  }

  // If we have no IRKs this time we force an empty one.
  // We cannot leave this blank otherwise the system will reuse the last one :-(
  if (irk_count == 0)
  {
    static const ble_gap_irk_t empty_irk;
    irks[0] = empty_irk;
    p_irks[0] = &irks[0];
    irk_count++;
  }

  ble_gap_whitelist_t whitelist =
  {
    .pp_addrs = p_addrs,
    .addr_count = addr_count,
    .pp_irks = p_irks,
    .irk_count = irk_count
  };
  ble_gap_scan_params_t scan =
  {
    .interval = CONNECT_SCAN_INTERVAL,
    .window = CONNECT_SCAN_WINDOW,
    .timeout = CONNECT_TIMEOUT,
    .selective = 1,
    .p_whitelist = &whitelist
  };
  err_code = sd_ble_gap_connect(NULL, &scan, &conn);
  APP_ERROR_CHECK(err_code);

  return MESH_OK;
}

void Mesh_System_Disconnect(Mesh_Node* node)
{
	uint32_t err_code;

	STAT_TIMER_START(disconnecting_total_time_ms);
	err_code = sd_ble_gap_disconnect(mesh_state.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	APP_ERROR_CHECK(err_code);
}

void Mesh_System_Write(Mesh_Node* node)
{
	uint32_t err_code;

	ble_gattc_write_params_t write =
	{
		.write_op = BLE_GATT_OP_WRITE_REQ,
		.handle = mesh_state.attr_handle,
		.len = mesh_node.sync.bufferlen,
		.p_value = mesh_node.sync.buffer
	};
	STAT_TIMER_START(write_out_total_time_ms);
	err_code = sd_ble_gattc_write(mesh_state.conn_handle, &write);
	APP_ERROR_CHECK(err_code);
}

void Mesh_System_Read(Mesh_Node* node)
{
	uint32_t err_code;

	STAT_TIMER_START(read_out_total_time_ms);
	err_code = sd_ble_gattc_read(mesh_state.conn_handle, mesh_state.attr_handle, 0);
	APP_ERROR_CHECK(err_code);
}

void Mesh_System_Retry(Mesh_Node* node, unsigned short retrycount)
{
	uint32_t err_code;

	uint16_t random;
	err_code = sd_rand_application_vector_get((uint8_t*)&random, sizeof(random));
	APP_ERROR_CHECK(err_code);
	uint32_t timeout = RETRY_FIXED + ((RETRY_VARIABLE * (uint32_t)random * (uint32_t)retrycount) >> 16);
	STAT_RECORD_ADD(retry_total_time_ms, timeout);
	STAT_RECORD_INC(retry_count);
	STAT_RECORD_ADD(retry_node_count, retrycount);
	err_code = app_timer_start(mesh_state.timer, MS_TO_TICKS(timeout), NULL);
	APP_ERROR_CHECK(err_code);
}

void Mesh_System_ScheduleDiscovery(Mesh_Node* node)
{
  mesh_state.rediscover_needed = 1;
}

Mesh_Tick Mesh_System_Tick(void)
{
	return (Mesh_Tick)(meshtime_tick() / 1000); // Tick in seconds
}

void Mesh_System_RandomNumber(unsigned char* buffer, unsigned char length)
{
  uint32_t err_code;

  while (length)
  {
    uint8_t available = 0;
    err_code = sd_rand_application_bytes_available_get(&available);
    APP_ERROR_CHECK(err_code);
    if (available)
    {
      available = available < length ? available : length;
      err_code = sd_rand_application_vector_get(buffer, available);
      APP_ERROR_CHECK(err_code);
      buffer += available;
      length -= available;
    }
  }
}
