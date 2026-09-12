#include "flow_sensor.h"
#include "aquabox_config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_timer.h"

static const char *TAG = "flow";
static volatile uint32_t pulse_count;
static volatile int64_t last_pulse_us;
static void IRAM_ATTR flow_pulse_isr(void *arg) {
  (void)arg;
  ++pulse_count;
  last_pulse_us = esp_timer_get_time();
}
esp_err_t flow_sensor_init(void) {
  ESP_RETURN_ON_ERROR(gpio_set_intr_type(AQUABOX_PIN_FLOW, GPIO_INTR_POSEDGE),
                      TAG, "could not configure flow interrupt");
  const esp_err_t result = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
    return result;
  return gpio_isr_handler_add(AQUABOX_PIN_FLOW, flow_pulse_isr, NULL);
}
uint32_t flow_sensor_pulse_count(void) { return pulse_count; }
int64_t flow_sensor_last_pulse_us(void) { return last_pulse_us; }
