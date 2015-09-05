/*
 * secure.c
 *
 *  Created on: Jun 3, 2015
 *      Author: tim
 */

#include <stdio.h>
#include <string.h>

#include <nordic_common.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_hci.h>
#include <app_error.h>
#include <app_scheduler.h>
#include <nrf_soc.h>

#include "timer.h"
#include "advertising.h"
#include "nrfmesh.h"
#include "secure.h"

typedef enum
{
  SECURE_ADDR_MESH,
  SECURE_ADDR_ENDPOINT
} secure_addr_type;

static const Mesh_Key MESH_KEY_LTK_FIRST = _MESH_KEY_LTK_FIRST;
static const Mesh_Key MESH_KEY_LTK_LAST  = _MESH_KEY_LTK_LAST;

static struct
{
  struct
  {
    ble_gap_enc_key_t enc;
    ble_gap_id_key_t  id;
  } p;
  struct
  {
    ble_gap_enc_key_t enc;
    ble_gap_id_key_t  id;
  } c;
} secure_keys;

static struct
{
  secure_addr_type      addr_type;
  ble_gap_addr_t        mesh_addr;
  ble_gap_addr_t        endpoint_addr;
  app_timer_id_t        addr_timer;
  ble_gap_sec_params_t  periph_auth;
  ble_gap_sec_params_t  mesh_auth;
  ble_gap_sec_keyset_t  keyset;
  uint8_t               role;
  uint8_t               oob[16];
  app_timer_id_t        timer;
  uint8_t               pairing;
} secure_state =
{
    .addr_type = -1,
    .endpoint_addr =
    {
        .addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
        .addr = { SECURE_CLIENT_ADDRESS | 0xC0 }
    },
    .periph_auth =
    {
        .bond = 1,
        .mitm = 1,
        .io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY,
        .oob = 1,
        .min_key_size = 16,
        .max_key_size = 16,
        .kdist_periph = { .enc = 1, .id = 1, .sign = 0 },
        .kdist_central = { .enc = 1, .id = 1, .sign = 0 }
    },
    .mesh_auth =
    {
        .bond = 0,
        .mitm = 1,
        .io_caps = BLE_GAP_IO_CAPS_KEYBOARD_ONLY,
        .oob = 1,
        .min_key_size = 16,
        .max_key_size = 16,
        .kdist_periph = { .enc = 1, .id = 1, .sign = 0 },
        .kdist_central = { .enc = 1, .id = 1, .sign = 0 }
    },
    .keyset =
    {
        .keys_periph = { &secure_keys.p.enc, NULL, NULL },
        .keys_central = { &secure_keys.c.enc, &secure_keys.c.id, NULL }
    },
};

typedef struct
{
  uint8_t ediv[2];
  uint8_t ltk[BLE_GAP_SEC_KEY_LEN];
  uint8_t irk[BLE_GAP_SEC_KEY_LEN];
} secure_keytransfer;


#define SECURE_ADDRESS_SWITCH_TIMEOUT   (15 * 1000) // ms

static const secure_keytransfer emptybuf;

static uint8_t secure_newkey(ble_gap_enc_key_t* enc, ble_gap_id_key_t* id);
static uint8_t secure_selectkey(uint16_t ediv);
static uint8_t secure_check_irk_address(ble_gap_addr_t* address);
static void secure_set_address(secure_addr_type type);

static void secure_handler_irq(void* dummy)
{
  secure_state.pairing = 0;
}

static void secure_handler_addr(void* dummy, uint16_t size)
{
  secure_set_address(SECURE_ADDR_MESH);
  Mesh_Process(&mesh_node, MESH_EVENT_CLIENT_TIMEOUT, 0, 0);
}

static void secure_handler_addr_irq(void* dummy)
{
  uint32_t err_code;

  err_code = app_sched_event_put(NULL, 0, secure_handler_addr);
  APP_ERROR_CHECK(err_code);
}

void secure_init(void)
{
  uint32_t err_code;

  err_code = app_timer_create(&secure_state.timer, APP_TIMER_MODE_SINGLE_SHOT, secure_handler_irq);
  APP_ERROR_CHECK(err_code);

  err_code = app_timer_create(&secure_state.addr_timer, APP_TIMER_MODE_SINGLE_SHOT, secure_handler_addr_irq);
  APP_ERROR_CHECK(err_code);
}

void secure_set_passkey(uint8_t* passkey, int32_t timeout_ms)
{
  uint32_t err_code;

  // 0-key is equivalent to "just work" pairing.
  if (strncmp(passkey, "000000", 6) == 0)
  {
    secure_state.periph_auth.io_caps = BLE_GAP_IO_CAPS_NONE;
  }
  else
  {
    secure_state.periph_auth.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY;
    ble_opt_t kopt =
    {
        .gap_opt.passkey.p_passkey = passkey
    };
    err_code = sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &kopt);
    APP_ERROR_CHECK(err_code);
  }

  secure_state.pairing = 0;

  // Enable pairing for a limited period of time after setting up keys
  if (timeout_ms > 0)
  {
    err_code = app_timer_start(secure_state.timer, MS_TO_TICKS(timeout_ms), NULL);
    APP_ERROR_CHECK(err_code);

    secure_state.pairing = 1;
  }
  else if (timeout_ms < 0)
  {
    secure_state.pairing = 1;
  }

  // Remember our unique, static address
  err_code = sd_ble_gap_address_get(&secure_state.mesh_addr);
  APP_ERROR_CHECK(err_code);
  secure_state.mesh_addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;

  secure_set_address(SECURE_ADDR_MESH);
}

void secure_set_keys(uint8_t* oob)
{
  // Shared key used to secure mesh network
  memcpy(secure_state.oob, oob, BLE_GAP_SEC_KEY_LEN);
}

static void secure_set_address(secure_addr_type type)
{
  uint32_t err_code;

  if (type != secure_state.addr_type)
  {
    switch (type)
    {
    case SECURE_ADDR_MESH:
      advertising_stop();
      err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &secure_state.mesh_addr);
      APP_ERROR_CHECK(err_code);
      advertising_set_0();
      break;

    case SECURE_ADDR_ENDPOINT:
      advertising_stop();
      err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &secure_state.endpoint_addr);
      APP_ERROR_CHECK(err_code);
      advertising_set_1();
      break;

    default:
      APP_ERROR_CHECK(NRF_ERROR_INVALID_PARAM);
      break;
    }

    secure_state.addr_type = type;
    err_code = sd_ble_gap_address_get(&secure_keys.p.id.id_addr_info);
    APP_ERROR_CHECK(err_code);
  }
}

void secure_authenticate(uint16_t handle)
{
  uint32_t err_code;

  if (mesh_node.ids[mesh_node.sync.neighbor->id].flag.client)
  {
    Mesh_Process(&mesh_node, MESH_EVENT_CONNECTED, 0, 0);
  }
  else
  {
    err_code = sd_ble_gap_authenticate(handle, &secure_state.mesh_auth);
    APP_ERROR_CHECK(err_code);
  }
}

void secure_ble_event(ble_evt_t* event)
{
  uint32_t err_code;

  switch (event->header.evt_id)
  {
  case BLE_GAP_EVT_CONNECTED:
    secure_state.role = event->evt.gap_evt.params.connected.role;
    if (event->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH && event->evt.gap_evt.params.connected.peer_addr.addr_type == BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE)
    {
      switch (mesh_node.state)
      {
      case MESH_STATE_IDLE:
        // Switch to client mode
        Mesh_Process(&mesh_node, MESH_EVENT_CLIENT_START, 0, 0);
        err_code = sd_ble_gap_disconnect(event->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        secure_set_address(SECURE_ADDR_ENDPOINT);
        event->header.evt_id = BLE_EVT_INVALID;
        err_code = app_timer_start(secure_state.addr_timer, MS_TO_TICKS(SECURE_ADDRESS_SWITCH_TIMEOUT), NULL);
        APP_ERROR_CHECK(err_code);
        break;

      case MESH_STATE_CLIENT_WAITING:
        err_code = app_timer_stop(secure_state.addr_timer);
        APP_ERROR_CHECK(err_code);
        break;

      case MESH_STATE_CLIENT_STARTING:
      case MESH_STATE_CLIENT_CONNECTED:
      default:
        // Cannot switch to client mode - just disconnect
        err_code = sd_ble_gap_disconnect(event->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        event->header.evt_id = BLE_EVT_INVALID;
        break;
      }
    }
    else
    {
      switch (mesh_node.state)
      {
      case MESH_STATE_CLIENT_STARTING:
      case MESH_STATE_CLIENT_WAITING:
      case MESH_STATE_CLIENT_CONNECTED:
        // In client mode - just disconnect
        err_code = sd_ble_gap_disconnect(event->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        event->header.evt_id = BLE_EVT_INVALID;
        break;

      default:
        break;
      }
    }
    break;

  case BLE_GAP_EVT_DISCONNECTED:
    switch (mesh_node.state)
    {
    case MESH_STATE_CLIENT_CONNECTED:
      secure_set_address(SECURE_ADDR_MESH);
      break;

    case MESH_STATE_CLIENT_STARTING:
    case MESH_STATE_CLIENT_WAITING:
    default:
      break;
    }
    break;

    case BLE_GAP_EVT_SEC_INFO_REQUEST:
      if (secure_selectkey(event->evt.gap_evt.params.sec_info_request.master_id.ediv))
      {
        err_code = sd_ble_gap_sec_info_reply(event->evt.gap_evt.conn_handle, &secure_keys.p.enc.enc_info, NULL, NULL);
        APP_ERROR_CHECK(err_code);
      }
      else
      {
        err_code = sd_ble_gap_sec_info_reply(event->evt.gap_evt.conn_handle, NULL, NULL, NULL);
        APP_ERROR_CHECK(err_code);
      }
      break;

    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
      if (secure_state.role == BLE_GAP_ROLE_CENTRAL)
      {
        err_code = sd_ble_gap_sec_params_reply(event->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, NULL, &secure_state.keyset);
        APP_ERROR_CHECK(err_code);
      }
      else
      {
        if (event->evt.gap_evt.params.sec_params_request.peer_params.oob)
        {
          err_code = sd_ble_gap_sec_params_reply(event->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, &secure_state.mesh_auth, &secure_state.keyset);
          APP_ERROR_CHECK(err_code);
        }
        else
        {
          if (secure_state.pairing)
          {
            err_code = sd_ble_gap_sec_params_reply(event->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, &secure_state.periph_auth, &secure_state.keyset);
            APP_ERROR_CHECK(err_code);
          }
          else
          {
            err_code = sd_ble_gap_sec_params_reply(event->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
          }
        }
      }
      break;

    case BLE_GAP_EVT_CONN_SEC_UPDATE:
      break;

    case BLE_GAP_EVT_AUTH_KEY_REQUEST:
      err_code = sd_ble_gap_auth_key_reply(event->evt.gap_evt.conn_handle, BLE_GAP_AUTH_KEY_TYPE_OOB, secure_state.oob);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GAP_EVT_AUTH_STATUS:
      if (event->evt.gap_evt.params.auth_status.auth_status != BLE_GAP_SEC_STATUS_SUCCESS)
      {
        err_code = sd_ble_gap_disconnect(event->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
      }
      else
      {
        if (event->evt.gap_evt.params.auth_status.bonded)
        {
          secure_newkey(&secure_keys.p.enc, &secure_keys.c.id);
        }
      }
      break;

    default:
      break;
  }
}

static uint8_t secure_newkey(ble_gap_enc_key_t* enc, ble_gap_id_key_t* id)
{
  secure_keytransfer buf;

  Mesh_Key match = {};
  Mesh_Key empty = {};

  for (Mesh_Key key = MESH_KEY_LTK_FIRST; key.key != MESH_KEY_LTK_LAST.key; key.key++)
  {
    uint8_t length = sizeof(buf);
    Mesh_Status status = Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, (uint8_t*)&buf, &length);
    if (status == MESH_NOTFOUND)
    {
      if (!empty.admin)
      {
        empty = key;
      }
    }
    else if (status == MESH_OK && length == sizeof(buf))
    {
      if (memcmp(&buf, &emptybuf, length) == 0)
      {
        if (!empty.admin)
        {
          empty = key;
        }
      }
      else if (memcmp(buf.irk, id->id_info.irk, BLE_GAP_SEC_KEY_LEN) == 0)
      {
        match = key;
        break;
      }
    }
  }
  if (!match.admin)
  {
    if (!empty.admin)
    {
      // No space
      return 0;
    }
    match = empty;
  }

  // Add the MESH_NODEID_CLIENT as a neighbor
  if (!mesh_node.ids[MESH_NODEID_CLIENT].flag.neighbor)
  {
    Mesh_Neighbor* neighbor;
    if (Mesh_AddNeighbor(&mesh_node, MESH_NODEID_CLIENT, &neighbor) != MESH_OK)
    {
      return 0;
    }
    neighbor->flag.valid = 1;
  }

  // ediv - used to identify the ltk for reconnections
  memcpy(buf.ediv, &enc->master_id.ediv, sizeof(uint16_t));
  // ltk - used to secure/auth reconnections
  memcpy(buf.ltk, enc->enc_info.ltk, BLE_GAP_SEC_KEY_LEN);
  // irk - used to resolve address of connecting "phone" when we connect to it
  memcpy(buf.irk, id->id_info.irk, BLE_GAP_SEC_KEY_LEN);

  Mesh_Status status = Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, match, (uint8_t*)&buf, sizeof(buf));
  if (status == MESH_OK)
  {
    Mesh_Sync(&mesh_node);
    return 1;
  }

  return 0;
}

static uint8_t secure_selectkey(uint16_t ediv)
{
  secure_keytransfer buf;

  memset(secure_keys.p.enc.enc_info.ltk, 0, BLE_GAP_SEC_KEY_LEN);

  for (Mesh_Key key = MESH_KEY_LTK_FIRST; key.key != MESH_KEY_LTK_LAST.key; key.key++)
  {
    uint8_t length = sizeof(buf);
    if (Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, (uint8_t*)&buf, &length) == MESH_OK)
    {
      if (memcmp(buf.ediv, &ediv, sizeof(ediv)) == 0)
      {
        memcpy(secure_keys.p.enc.enc_info.ltk, buf.ltk, BLE_GAP_SEC_KEY_LEN);
        secure_keys.p.enc.enc_info.auth = 1;
        return 1;
      }
    }
  }

  return 0;
}

void secure_meshchange(Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length)
{
  if (id == MESH_NODEID_GLOBAL)
  {
    for (Mesh_Key search = MESH_KEY_LTK_FIRST; key.key != MESH_KEY_LTK_LAST.key; search.key++)
    {
      if (memcmp(&search, &key, sizeof(Mesh_Key)))
      {
        // If we set a LTK, we add a CLIENT neighbor so we'll attempt to sync changes to clients
        if (!mesh_node.ids[MESH_NODEID_CLIENT].flag.neighbor)
        {
          Mesh_Neighbor* neighbor;
          if (Mesh_AddNeighbor(&mesh_node, MESH_NODEID_CLIENT, &neighbor) == MESH_OK)
          {
            neighbor->flag.valid = 1;
          }
        }
        break;
      }
    }
  }
}

void secure_reset_bonds(void)
{
  for (Mesh_Key key = MESH_KEY_LTK_FIRST; key.key != MESH_KEY_LTK_LAST.key; key.key++)
  {
    uint8_t length = 0;
    if (Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, NULL, &length) == MESH_OK)
    {
      Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, key, (uint8_t*)&emptybuf, sizeof(emptybuf));
    }
  }
}

uint8_t secure_get_irks(ble_gap_irk_t irks[BLE_GAP_WHITELIST_IRK_MAX_COUNT])
{
  secure_keytransfer buf;
  uint8_t count = 0;
  for (Mesh_Key key = MESH_KEY_LTK_FIRST; key.key != MESH_KEY_LTK_LAST.key && count < BLE_GAP_WHITELIST_IRK_MAX_COUNT; key.key++)
  {
    uint8_t length = sizeof(buf);
    if (Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, (uint8_t*)&buf, &length) == MESH_OK)
    {
      memcpy(irks[count++].irk, buf.irk, BLE_GAP_SEC_KEY_LEN);
    }
  }
  return count;
}

#if 0

static uint8_t secure_check_irk_address(ble_gap_addr_t* address)
{
  uint32_t err_code;

  if (address->addr_type == BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE)
  {
    secure_keytransfer buf;
    for (Mesh_Key key = MESH_KEY_LTK_FIRST; key.key != MESH_KEY_LTK_LAST.key; key.key++)
    {
      uint8_t length = sizeof(buf);
      if (Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, buf.buf, &length) == MESH_OK)
      {
        uint8_t* irk = buf.buf + sizeof(uint16_t) + BLE_GAP_SEC_KEY_LEN;
        nrf_ecb_hal_data_t edata =
        {
            .key =
            {
                irk[15], irk[14], irk[13], irk[12], irk[11], irk[10], irk[9], irk[8],
                irk[7],  irk[6],  irk[5],  irk[4],  irk[3],  irk[2],  irk[1], irk[0]
            },
            .cleartext =
            {
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, address->addr[5], address->addr[4], address->addr[3]
            }
        };
        err_code = sd_ecb_block_encrypt(&edata);
        APP_ERROR_CHECK(err_code);

        if (edata.ciphertext[15] == address->addr[0] && edata.ciphertext[14] == address->addr[1] && edata.ciphertext[13] == address->addr[2])
        {
          return 1;
        }
      }
    }
  }
  return 0;
}

#endif
