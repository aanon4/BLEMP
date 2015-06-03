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

static struct
{
  ble_gap_sec_params_t  auth;
  ble_gap_sec_keyset_t  keyset;
  uint8_t               role;
  uint8_t               oob[16];
  uint8_t               using_oob;
} secure_state =
{
    .auth =
    {
        .bond = 0,
        .mitm = 1,
        .io_caps = BLE_GAP_IO_CAPS_NONE,
        .oob = 1,
        .min_key_size = 16,
        .max_key_size = 16,
        .kdist_periph = { .enc = 1, .id = 1, .sign = 0 },
        .kdist_central = { .enc = 1, .id = 1, .sign = 0 }
    },
    .keyset =
    {
        .keys_periph = { NULL, NULL, NULL },
        .keys_central = { NULL, NULL, NULL }
    }
};


void secure_set_keys(uint8_t* oob)
{
  memcpy(secure_state.oob, oob, sizeof(secure_state.oob));
}

uint8_t secure_authenticate(uint16_t handle)
{
  uint32_t err_code;

  secure_state.auth.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_ONLY;
  err_code = sd_ble_gap_authenticate(handle, &secure_state.auth);
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
    secure_state.using_oob = 0;
    break;

  case BLE_GAP_EVT_DISCONNECTED:
    secure_state.using_oob = 0;
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
        secure_state.auth.io_caps = BLE_GAP_IO_CAPS_KEYBOARD_ONLY;
      }
      else
      {
        secure_state.auth.io_caps = BLE_GAP_IO_CAPS_DISPLAY_ONLY;
      }
      err_code = sd_ble_gap_sec_params_reply(event->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, &secure_state.auth, &secure_state.keyset);
      APP_ERROR_CHECK(err_code);
    }
    break;

  case BLE_GAP_EVT_AUTH_KEY_REQUEST:
    secure_state.using_oob = 1;
    err_code = sd_ble_gap_auth_key_reply(event->evt.gap_evt.conn_handle, BLE_GAP_AUTH_KEY_TYPE_OOB, secure_state.oob);
    APP_ERROR_CHECK(err_code);
    break;

  case BLE_GAP_EVT_CONN_SEC_UPDATE:
    break;

  case BLE_GAP_EVT_PASSKEY_DISPLAY:
    break;

  case BLE_GAP_EVT_AUTH_STATUS:
    if (event->evt.gap_evt.params.auth_status.auth_status != BLE_GAP_SEC_STATUS_SUCCESS)
    {
      err_code = sd_ble_gap_disconnect(event->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      APP_ERROR_CHECK(err_code);
    }
    break;

  default:
    break;
  }
}

uint8_t secure_check_authorization(ble_evt_t* event)
{
  uint32_t err_code;

  if (secure_state.using_oob)
  {
    return 1;
  }
  else
  {
    ble_gatts_rw_authorize_reply_params_t reply =
    {
      .type = event->evt.gatts_evt.params.authorize_request.type,
      .params.write.gatt_status = (event->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE ? BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED : BLE_GATT_STATUS_ATTERR_READ_NOT_PERMITTED),
    };
    err_code = sd_ble_gatts_rw_authorize_reply(event->evt.common_evt.conn_handle, &reply);
    APP_ERROR_CHECK(err_code);
    return 0;
  }
}
