#pragma once

#include "esp_err.h"

typedef enum {
  BUTTON_NONE,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_ENTER,
  BUTTON_BACK,
} button_event_t;

esp_err_t buttons_init(void);
button_event_t buttons_poll(void);
