#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t hardware_init(void);
void hardware_safe_stop(void);
void hardware_set_solenoid(unsigned channel, bool open);
void hardware_set_pump(bool on);
bool hardware_pump_is_on(void);
bool hardware_level_low_active(unsigned channel);
bool hardware_level_high_active(unsigned channel);
bool hardware_manual_mode_active(void);
bool hardware_independent_pump_requested(void);
