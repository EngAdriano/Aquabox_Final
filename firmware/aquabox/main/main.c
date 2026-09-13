#include "control/hydraulic_control.h"
#include "drivers/flow_sensor.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/hardware.h"
#include "ui/buttons.h"
#include "ui/local_ui.h"
#include "ui/st7735_display.h"

static const char *TAG = "app";

void app_main(void) {
  ESP_ERROR_CHECK(hardware_init());
  ESP_ERROR_CHECK(flow_sensor_init());
  ESP_ERROR_CHECK(buttons_init());
  ESP_ERROR_CHECK(st7735_display_init());
  if (hydraulic_control_start() != pdPASS) {
    hardware_safe_stop();
    ESP_LOGE(TAG, "Failed to start hydraulic control; outputs remain off");
  }
  if (local_ui_start() != pdPASS) {
    ESP_LOGE(TAG, "Failed to start local user interface");
  }
}
