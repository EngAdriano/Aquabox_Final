#include "hardware.h"
#include "aquabox_config.h"
#include "driver/gpio.h"
#include "esp_check.h"

static const char *TAG = "hardware";
static const gpio_num_t solenoid_pins[AQUABOX_CHANNEL_COUNT] = {
    AQUABOX_PIN_SOL1, AQUABOX_PIN_SOL2, AQUABOX_PIN_SOL3};
static const gpio_num_t level_low_pins[AQUABOX_CHANNEL_COUNT] = {
    AQUABOX_PIN_S1_LOW, AQUABOX_PIN_S2_LOW, AQUABOX_PIN_S3_LOW};
static const gpio_num_t level_high_pins[AQUABOX_CHANNEL_COUNT] = {
    AQUABOX_PIN_S1_HIGH, AQUABOX_PIN_S2_HIGH, AQUABOX_PIN_S3_HIGH};

void hardware_safe_stop(void) {
  gpio_set_level(AQUABOX_PIN_PUMP, 0);
  for (unsigned i = 0; i < AQUABOX_CHANNEL_COUNT; ++i)
    gpio_set_level(solenoid_pins[i], 0);
}
esp_err_t hardware_init(void) {
  const uint64_t outputs =
      (1ULL << AQUABOX_PIN_PUMP) | (1ULL << AQUABOX_PIN_SOL1) |
      (1ULL << AQUABOX_PIN_SOL2) | (1ULL << AQUABOX_PIN_SOL3);
  const uint64_t inputs =
      (1ULL << AQUABOX_PIN_FLOW) | (1ULL << AQUABOX_PIN_S1_LOW) |
      (1ULL << AQUABOX_PIN_S1_HIGH) | (1ULL << AQUABOX_PIN_S2_LOW) |
      (1ULL << AQUABOX_PIN_S2_HIGH) | (1ULL << AQUABOX_PIN_S3_LOW) |
      (1ULL << AQUABOX_PIN_S3_HIGH) | (1ULL << AQUABOX_PIN_PUMP_SWITCH) |
      (1ULL << AQUABOX_PIN_MANUAL_SWITCH);
  const gpio_config_t out = {.pin_bit_mask = outputs,
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
  const gpio_config_t in = {.pin_bit_mask = inputs,
                            .mode = GPIO_MODE_INPUT,
                            .pull_up_en = GPIO_PULLUP_DISABLE,
                            .pull_down_en = GPIO_PULLDOWN_DISABLE,
                            .intr_type = GPIO_INTR_DISABLE};
  ESP_RETURN_ON_ERROR(gpio_config(&out), TAG,
                      "output GPIO configuration failed");
  ESP_RETURN_ON_ERROR(gpio_config(&in), TAG, "input GPIO configuration failed");
  hardware_safe_stop();
  return ESP_OK;
}
void hardware_set_solenoid(unsigned channel, bool open) {
  if (channel < AQUABOX_CHANNEL_COUNT)
    gpio_set_level(solenoid_pins[channel], open);
}
void hardware_set_pump(bool on) { gpio_set_level(AQUABOX_PIN_PUMP, on); }
bool hardware_pump_is_on(void) { return gpio_get_level(AQUABOX_PIN_PUMP) != 0; }
bool hardware_level_low_active(unsigned channel) {
  return channel < AQUABOX_CHANNEL_COUNT &&
         gpio_get_level(level_low_pins[channel]) == 0;
}
bool hardware_level_high_active(unsigned channel) {
  return channel < AQUABOX_CHANNEL_COUNT &&
         gpio_get_level(level_high_pins[channel]) == 0;
}
bool hardware_manual_mode_active(void) {
  return gpio_get_level(AQUABOX_PIN_MANUAL_SWITCH) != 0;
}
bool hardware_independent_pump_requested(void) {
  return gpio_get_level(AQUABOX_PIN_PUMP_SWITCH) != 0;
}
