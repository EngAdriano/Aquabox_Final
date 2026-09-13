#include "buttons.h"

#include "aquabox_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"

#define DEBOUNCE_MS 40

static const char *TAG = "buttons";
static const gpio_num_t pins[] = {AQUABOX_PIN_BTN_UP, AQUABOX_PIN_BTN_DOWN,
                                  AQUABOX_PIN_BTN_ENTER, AQUABOX_PIN_BTN_BACK};
static uint8_t stable_state[4];
static int64_t changed_at_us[4];

esp_err_t buttons_init(void) {
  uint64_t mask = 0;
  for (unsigned i = 0; i < 4; ++i)
    mask |= 1ULL << pins[i];
  const gpio_config_t config = {.pin_bit_mask = mask,
                                .mode = GPIO_MODE_INPUT,
                                .pull_up_en = GPIO_PULLUP_DISABLE,
                                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                .intr_type = GPIO_INTR_DISABLE};
  ESP_RETURN_ON_ERROR(gpio_config(&config), TAG,
                      "button GPIO configuration failed");
  for (unsigned i = 0; i < 4; ++i)
    stable_state[i] = gpio_get_level(pins[i]);
  return ESP_OK;
}

button_event_t buttons_poll(void) {
  const int64_t now = esp_timer_get_time();
  for (unsigned i = 0; i < 4; ++i) {
    const uint8_t raw = gpio_get_level(pins[i]); /* Buttons are active high. */
    if (raw != stable_state[i]) {
      if (changed_at_us[i] == 0)
        changed_at_us[i] = now;
      else if (now - changed_at_us[i] >= DEBOUNCE_MS * 1000) {
        stable_state[i] = raw;
        changed_at_us[i] = 0;
        if (raw)
          return (button_event_t)(i + 1); /* press edge only */
      }
    } else {
      changed_at_us[i] = 0;
    }
  }
  return BUTTON_NONE;
}
