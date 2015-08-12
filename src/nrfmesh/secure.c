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

#include "services/timer.h"

#include "nrfmesh.h"
#include "secure.h"

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
  ble_gap_sec_params_t  periph_auth;
  ble_gap_sec_params_t  mesh_auth;
  ble_gap_sec_keyset_t  keyset;
  uint8_t               role;
  uint8_t               oob[16];
  app_timer_id_t        timer;
  uint8_t               pairing;
} secure_state =
{
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
  uint8_t buf[sizeof(uint16_t) + BLE_GAP_SEC_KEY_LEN + BLE_GAP_SEC_KEY_LEN];
} secure_keytransfer;

static const secure_keytransfer emptybuf;

static uint8_t secure_havekeyspace(void);
static uint8_t secure_newkey(ble_gap_enc_key_t* enc, ble_gap_id_key_t* id);
static uint8_t secure_selectkey(uint16_t ediv);

static void secure_handler_irq(void* dummy)
{
  secure_state.pairing = 0;
}

void secure_init(void)
{
  uint32_t err_code;

  err_code = app_timer_create(&secure_state.timer, APP_TIMER_MODE_SINGLE_SHOT, secure_handler_irq);
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
}

void secure_set_keys(uint8_t* oob, uint8_t* irk)
{
  uint32_t err_code;

  memcpy(secure_state.oob, oob, BLE_GAP_SEC_KEY_LEN);
  if (irk != NULL)
  {
    // Setup a resolvable private address based on the specified IRK
    memcpy(secure_keys.p.id.id_info.irk, irk, BLE_GAP_SEC_KEY_LEN);

    ble_opt_t opt =
    {
        .gap_opt.privacy = { &secure_keys.p.id.id_info, 0 }
    };
    err_code = sd_ble_opt_set(BLE_GAP_OPT_PRIVACY, &opt);
    APP_ERROR_CHECK(err_code);

    // Create new private address. This doesn't change
    secure_keys.p.id.id_addr_info.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE;
    err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_AUTO, &secure_keys.p.id.id_addr_info);
    APP_ERROR_CHECK(err_code);
  }

  err_code = sd_ble_gap_address_get(&secure_keys.p.id.id_addr_info);
  APP_ERROR_CHECK(err_code);
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
    break;

  case BLE_GAP_EVT_SEC_INFO_REQUEST:
    secure_selectkey(event->evt.gap_evt.params.sec_info_request.master_id.ediv);
    secure_keys.p.enc.enc_info.auth = 1;
    err_code = sd_ble_gap_sec_info_reply(event->evt.gap_evt.conn_handle, &secure_keys.p.enc.enc_info, NULL, NULL);
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
        if (secure_state.pairing && secure_havekeyspace())
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

static uint8_t secure_havekeyspace(void)
{
  secure_keytransfer buf;

  for (Mesh_Key key = MESH_KEY_LTK_FIRST; memcmp(&key, &MESH_KEY_LTK_LAST, sizeof(Mesh_Key)) != 0; key.key++)
  {
    uint8_t length = sizeof(buf);
    Mesh_Status status = Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, buf.buf, &length);
    if (status == MESH_NOTFOUND || (status == MESH_OK && length == sizeof(buf) && memcmp(buf.buf, emptybuf.buf, length) == 0))
    {
      return 1;
    }
  }
  return 0;
}

static uint8_t secure_newkey(ble_gap_enc_key_t* enc, ble_gap_id_key_t* id)
{
  secure_keytransfer buf;

  for (Mesh_Key key = MESH_KEY_LTK_FIRST; memcmp(&key, &MESH_KEY_LTK_LAST, sizeof(Mesh_Key)) != 0; key.key++)
  {
    uint8_t length = sizeof(buf);
    Mesh_Status status = Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, buf.buf, &length);
    if (status == MESH_NOTFOUND || (status == MESH_OK && length == sizeof(buf) && memcmp(buf.buf, emptybuf.buf, length) == 0))
    {
#if MESH_ENABLE_CLIENT_SUPPORT
      // Add the MESH_NODEID_CLIENT as a neighbor
      Mesh_Neighbor* neighbor;
      if (Mesh_AddNeighbor(&mesh_node, MESH_NODEID_CLIENT, &neighbor) != MESH_OK)
      {
        return 0;
      }
      neighbor->flag.valid = 1;
#endif

      // ediv - used to identify the ltk for reconnections
      memcpy(buf.buf, &enc->master_id.ediv, sizeof(uint16_t));
      // ltk - used to secure/auth reconnections
      memcpy(buf.buf + sizeof(uint16_t), enc->enc_info.ltk, BLE_GAP_SEC_KEY_LEN);
      // irk - used to resolve address of connecting "phone" when we connect to it
      memcpy(buf.buf + sizeof(uint16_t) + BLE_GAP_SEC_KEY_LEN, id->id_info.irk, BLE_GAP_SEC_KEY_LEN);
      if (Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, key, buf.buf, sizeof(buf.buf)) == MESH_OK)
      {
        Mesh_Sync(&mesh_node);
        return 1;
      }
      else
      {
        return 0;
      }
    }
  }

  return 0;
}

static uint8_t secure_selectkey(uint16_t ediv)
{
  secure_keytransfer buf;

  memset(secure_keys.p.enc.enc_info.ltk, 0, BLE_GAP_SEC_KEY_LEN);

  for (Mesh_Key key = MESH_KEY_LTK_FIRST; memcmp(&key, &MESH_KEY_LTK_LAST, sizeof(Mesh_Key)); key.key++)
  {
    uint8_t length = sizeof(buf);
    if (Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, buf.buf, &length) == MESH_OK)
    {
      if (memcmp(buf.buf, &ediv, sizeof(ediv)) == 0)
      {
        memcpy(secure_keys.p.enc.enc_info.ltk, buf.buf + sizeof(uint16_t), BLE_GAP_SEC_KEY_LEN);
        return 1;
      }
    }
  }

  return 0;
}

void secure_meshchange(Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length)
{
#if MESH_ENABLE_CLIENT_SUPPORT
  if (id == MESH_NODEID_GLOBAL)
  {
    for (Mesh_Key search = MESH_KEY_LTK_FIRST; memcmp(&search, &MESH_KEY_LTK_LAST, sizeof(Mesh_Key)); search.key++)
    {
      if (memcmp(&search, &key, sizeof(Mesh_Key)))
      {
        // If we set a LTK, we add a CLIENT neighbor so we'll attempt to sync changes to clients
        Mesh_Neighbor* neighbor;
        if (Mesh_AddNeighbor(&mesh_node, MESH_NODEID_CLIENT, &neighbor) == MESH_OK)
        {
          neighbor->flag.valid = 1;
        }
        break;
      }
    }
  }
#endif
}

void secure_reset_bonds(void)
{
  for (Mesh_Key key = MESH_KEY_LTK_FIRST; memcmp(&key, &MESH_KEY_LTK_LAST, sizeof(Mesh_Key)) != 0; key.key++)
  {
    uint8_t length = 0;
    if (Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, NULL, &length) == MESH_OK)
    {
      Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, key, (uint8_t*)emptybuf.buf, sizeof(emptybuf.buf));
    }
  }
}

uint8_t secure_get_irks(ble_gap_irk_t irks[BLE_GAP_WHITELIST_IRK_MAX_COUNT])
{
  secure_keytransfer buf;
  uint8_t count = 0;
  for (Mesh_Key key = MESH_KEY_LTK_FIRST; memcmp(&key, &MESH_KEY_LTK_LAST, sizeof(Mesh_Key)) && count < BLE_GAP_WHITELIST_IRK_MAX_COUNT; key.key++)
  {
    uint8_t length = sizeof(buf);
    if (Mesh_GetValue(&mesh_node, MESH_NODEID_GLOBAL, key, buf.buf, &length) == MESH_OK)
    {
      memcpy(irks[count++].irk, buf.buf + sizeof(uint16_t) + BLE_GAP_SEC_KEY_LEN, BLE_GAP_SEC_KEY_LEN);
    }
  }
  return count;
}

uint8_t secure_address_type(void)
{
  return secure_keys.p.id.id_addr_info.addr_type;
}
