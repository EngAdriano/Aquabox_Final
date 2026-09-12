#pragma once
#include "esp_err.h"
#include <stdint.h>
esp_err_t flow_sensor_init(void);
uint32_t flow_sensor_pulse_count(void);
int64_t flow_sensor_last_pulse_us(void);
