/*
 * temperature.c
 *
 *  Created on: 21 juin 2024
 *      Author: valentin
 */


#include "temperature.h"
#include "sl_sensor_rht.h"
#include "app_assert.h"

/*/
 * Fonction permettant de lire et convertir la temperature
 */
sl_status_t read_and_convert_temperature(uint8_t *buffer) {
    uint32_t rh; 
    int32_t temperature;
    sl_status_t status = sl_sensor_rht_get(&rh, &temperature);

    if (status != SL_STATUS_OK) {
        return status;
    }

    // Conversion en degrés Celsius 
    int32_t temp_ble = temperature * 0.1;

    buffer[0] = (uint8_t)(temp_ble & 0xFF);
    buffer[1] = (uint8_t)((temp_ble >> 8) & 0xFF);

    return SL_STATUS_OK;
}


