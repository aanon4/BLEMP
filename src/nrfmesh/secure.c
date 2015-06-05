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

static void secure_handler_irq(void* dummy)
{
  secure_state.pairing = 0;
}

void secure_set_keys(uint8_t* passkey, uint32_t timeout_ms, uint8_t* oob, uint8_t* irk)
{
  uint32_t err_code;

  memcpy(secure_state.oob, oob, BLE_GAP_SEC_KEY_LEN);
  memcpy(secure_keys.p.id.id_info.irk, irk, BLE_GAP_SEC_KEY_LEN);

  // Setup a resolvable private address based on the IRK but which doesn't change over time
  ble_opt_t opt =
  {
      .gap_opt.privacy = { &secure_keys.p.id.id_info, 0 }
  };
  err_code = sd_ble_opt_set(BLE_GAP_OPT_PRIVACY, &opt);
  APP_ERROR_CHECK(err_code);

  secure_keys.p.id.id_addr_info.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE;
  err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_AUTO, &secure_keys.p.id.id_addr_info);
  APP_ERROR_CHECK(err_code);

  err_code = sd_ble_gap_address_get(&secure_keys.p.id.id_addr_info);
  APP_ERROR_CHECK(err_code);

  ble_opt_t kopt =
  {
      .gap_opt.passkey.p_passkey = passkey
  };
  err_code = sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &kopt);
  APP_ERROR_CHECK(err_code);

  // Enable pairing for a limited period of time after setting up keys
  if (timeout_ms > 0)
  {
    err_code = app_timer_create(&secure_state.timer, APP_TIMER_MODE_SINGLE_SHOT, secure_handler_irq);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_start(secure_state.timer, MS_TO_TICKS(timeout_ms), NULL);
    APP_ERROR_CHECK(err_code);
    secure_state.pairing = 1;
  }
}

uint8_t secure_authenticate(uint16_t handle)
{
  uint32_t err_code;

  err_code = sd_ble_gap_authenticate(handle, &secure_state.mesh_auth);
  APP_ERROR_CHECK(err_code);

  return 1;
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
        // NOTE: LTK is 16 byte. The current maximum mesh value can only be 15.
        Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_LTK0, secure_keys.p.enc.enc_info.ltk, BLE_GAP_SEC_KEY_LEN);
        Mesh_Sync(&mesh_node);
      }
    }
    break;

  default:
    break;
  }
}

void secure_mesh_valuechanged(Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length)
{
  if (key == MESH_KEY_LTK0)
  {
    memcpy(secure_keys.p.enc.enc_info.ltk, value, BLE_GAP_SEC_KEY_LEN);
  }
}
