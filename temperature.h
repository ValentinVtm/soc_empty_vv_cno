/*
 * temperature.h
 *
 *  Created on: 21 juin 2024
 *      Author: valentin
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "sl_sleeptimer.h"
#include "stddef.h"

sl_status_t read_and_convert_temperature(uint8_t *buffer);
void temperature_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);

// Structure de données qui stocke les données du callback
typedef struct {
    uint32_t step_count;
} timer_data_t;




#endif /* TEMPERATURE_H_ */
