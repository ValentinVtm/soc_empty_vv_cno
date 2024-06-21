/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "sl_sensor_rht.h"
#include "temperature.h"
#include "gatt_db.h"
#include "sl_sleeptimer.h"
#include "sl_bt_api.h"
#include "sl_simple_led_instances.h"

#define TEMPERATURE_TIMER_SIGNAL (1<<0)

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static sl_sleeptimer_timer_handle_t my_timer_handle;
static timer_data_t my_timer_data = { .step_count = 0 };
static uint8_t connection_handle = 0xFF; //BLE connection handler
extern sl_simple_led_context_t simple_led0_context;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  sl_simple_led_init_instances();
  app_log_info("%s\n", __FUNCTION__);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

void temperature_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data) {
  (void)handle;
  (void)data;
  sl_bt_external_signal(TEMPERATURE_TIMER_SIGNAL);
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
          advertising_set_handle,
          160, // min. adv. interval (milliseconds * 1.6)
          160, // max. adv. interval (milliseconds * 1.6)
          0,   // adv. duration
          0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

      // -------------------------------
      // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("%s: connection_opened!!!\n", __FUNCTION__);
      // Init the sensor
      sc = sl_sensor_rht_init();
      //      uint8_t status_flags = evt->data.evt_gatt_server_characteristic_status.status_flags;
      //      app_log_info("Status flags before notify: 0x%02x\n", status_flags);
      connection_handle = evt->data.evt_connection_opened.connection;
      break;

      // -------------------------------
      // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("%s: connection_closed!!!\n", __FUNCTION__);
      //Deinit the sensor
      sl_sensor_rht_deinit();

      sl_sleeptimer_stop_timer(&my_timer_handle);
      my_timer_data.step_count = 0; // Remise à 0 compteur si connexion OFF
      connection_handle = 0xFF; // Réinit handle connexion

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

      ///////////////////////////////////////////////////////////////////////////
      // Add additional event handlers here as your application requires!      //
      ///////////////////////////////////////////////////////////////////////////

    case sl_bt_evt_gatt_server_user_read_request_id:

      if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_temperature){
          app_log_info("%s: Temperature resquested!\n", __FUNCTION__);

          uint8_t connection = evt->data.evt_gatt_server_user_read_request.connection;
          uint16_t characteristic = evt->data.evt_gatt_server_user_read_request.characteristic;

          uint8_t temperature_value[2];
          uint16_t sent_len;

          sl_status_t status = read_and_convert_temperature(temperature_value);

          if (status == SL_STATUS_OK) {
              sc = sl_bt_gatt_server_send_user_read_response(connection,
                                                             characteristic,
                                                             0,
                                                             sizeof(temperature_value),
                                                             temperature_value,
                                                             &sent_len);
              app_log_info("BLE Temperature sent: %d . %d C\n", temperature_value[1], temperature_value[0]);
          } else {
              app_log_info("Failed to read temperature\n");
          }
      }

      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:
      if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_temperature){
          uint8_t status_flags = evt->data.evt_gatt_server_characteristic_status.status_flags;
          uint16_t client_config_flags = evt->data.evt_gatt_server_characteristic_status.client_config_flags;

          if (status_flags == sl_bt_gatt_server_client_config) {
              app_log_info("%s: Temperature notify requested!\n", __FUNCTION__);
              app_log_info("Status flags after notify: 0x%02x\n", status_flags);
              app_log_info("Client config flags: 0x%04x\n", client_config_flags);

              sl_status_t status = sl_sleeptimer_start_periodic_timer_ms(
                  &my_timer_handle,           // Pointeur vers timer structure
                  1000,                       // Période (ms) 
                  temperature_timer_callback, // Callback
                  &my_timer_data,             // Donées entrée callback
                  0,                          // Priorité
                  0                           // Options supplémentaires
              );

              if (status != SL_STATUS_OK) {
                  app_log_info("Failed to start timer\n");
              } else {
                  app_log_info("Timer started successfully\n");
              }
          }
      }
      break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & TEMPERATURE_TIMER_SIGNAL) {
          uint8_t temperature_value[2];
          sl_status_t status = read_and_convert_temperature(temperature_value);
          if (status == SL_STATUS_OK) {
              sc = sl_bt_gatt_server_send_notification(
                  connection_handle,
                  gattdb_temperature,
                  sizeof(temperature_value),
                  temperature_value
              );
              if (sc != SL_STATUS_OK) {
                  app_log_info("Failed to send temperature notification\n");
              } else {
                  app_log_info("Temperature notification sent: %d . %d C\n", temperature_value[1], temperature_value[0]);
              }
          } else {
              app_log_info("Failed to read temperature\n");
          }
      }
      break;

    case sl_bt_evt_gatt_server_user_write_request_id:
      uint8_t att_opcode = evt->data.evt_gatt_server_user_write_request.att_opcode;
      if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_aio_digital_out) {
          app_log_info("%s: Write request received\n", __FUNCTION__);

          uint8array *write_value = &evt->data.evt_gatt_server_user_write_request.value;
          if (write_value->len > 0) {
              uint8_t value = write_value->data[0];
              app_log_info("Value written: %d\n", value);

              if (value == 48) {
                  sl_simple_led_turn_off(&simple_led0_context);
              } else if (value == 49) {
                  sl_simple_led_turn_on(&simple_led0_context);
              }
          }
          if (att_opcode == 0x12){
              // réponse écriture SUCCESSFUL
              sc = sl_bt_gatt_server_send_user_write_response(evt->data.evt_gatt_server_user_write_request.connection,
                                                              evt->data.evt_gatt_server_user_write_request.characteristic,
                                                              0); // 0 = no error
          }
      }
      break;

      // -------------------------------
      // Default event handler.
    default:
      break;
  }
}
