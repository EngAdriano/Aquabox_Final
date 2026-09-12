#include "control/hydraulic_control.h"
#include "drivers/flow_sensor.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/hardware.h"

static const char *TAG = "app";

void app_main(void) {
  ESP_ERROR_CHECK(hardware_init());
  ESP_ERROR_CHECK(flow_sensor_init());
  if (hydraulic_control_start() != pdPASS) {
    hardware_safe_stop();
    ESP_LOGE(TAG, "Failed to start hydraulic control; outputs remain off");
  }
}
