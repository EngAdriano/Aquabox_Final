#include "local_ui.h"

#include "aquabox_config.h"
#include "buttons.h"
#include "drivers/flow_sensor.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "hal/hardware.h"
#include "st7735_display.h"
#include <stdio.h>

typedef enum {
  SCREEN_HOME,
  SCREEN_STATUS,
  SCREEN_SENSORS,
  SCREEN_FLOW,
  SCREEN_ABOUT
} screen_t;

static const char *TAG = "local_ui";

static void text_line(unsigned row, const char *text, uint16_t color) {
  /* Fonte 3x5 com escala 3: oito linhas cabem nas 128 linhas do painel. */
  st7735_display_text_font(4, 4 + row * 15, text, color, DISPLAY_FONT_MEDIUM);
}
static void render(screen_t screen) {
  char line[18];
  st7735_display_clear(DISPLAY_BLACK);
  switch (screen) {
  case SCREEN_HOME:
    st7735_display_text_font(4, 4, "AQUABOX", DISPLAY_CYAN, DISPLAY_FONT_LARGE);
    st7735_display_icon(132, 6, DISPLAY_ICON_TANK, DISPLAY_CYAN);
    st7735_display_text_font(4, 30, "OFFLINE", DISPLAY_YELLOW,
                             DISPLAY_FONT_SMALL);
    st7735_display_text_font(
        4, 53, hardware_manual_mode_active() ? "MODE MANUAL" : "MODE AUTO",
        DISPLAY_WHITE, DISPLAY_FONT_MEDIUM);
    st7735_display_icon(4, 77, DISPLAY_ICON_VALVE, DISPLAY_GREEN);
    st7735_display_text_font(26, 79, "ENTER MENU", DISPLAY_GREEN,
                             DISPLAY_FONT_MEDIUM);
    break;
  case SCREEN_STATUS:
    text_line(0, "STATUS", DISPLAY_CYAN);
    st7735_display_icon(132, 4, DISPLAY_ICON_PUMP,
                        hardware_pump_is_on() ? DISPLAY_GREEN : DISPLAY_WHITE);
    text_line(2, hardware_pump_is_on() ? "PUMP ON" : "PUMP OFF",
              hardware_pump_is_on() ? DISPLAY_GREEN : DISPLAY_WHITE);
    text_line(4, hardware_manual_mode_active() ? "MANUAL" : "AUTOMATIC",
              DISPLAY_WHITE);
    text_line(7, "BACK RETURN", DISPLAY_YELLOW);
    break;
  case SCREEN_SENSORS:
    text_line(0, "SENSORS", DISPLAY_CYAN);
    st7735_display_icon(132, 4, DISPLAY_ICON_TANK, DISPLAY_CYAN);
    for (unsigned i = 0; i < AQUABOX_CHANNEL_COUNT; ++i) {
      snprintf(line, sizeof(line), "S%u L%c H%c", i + 1,
               hardware_level_low_active(i) ? '1' : '0',
               hardware_level_high_active(i) ? '1' : '0');
      text_line(i + 2, line, DISPLAY_WHITE);
    }
    text_line(7, "BACK RETURN", DISPLAY_YELLOW);
    break;
  case SCREEN_FLOW:
    text_line(0, "FLOW", DISPLAY_CYAN);
    st7735_display_icon(132, 4, DISPLAY_ICON_FLOW, DISPLAY_CYAN);
    text_line(2, "PULSE COUNT", DISPLAY_WHITE);
    snprintf(line, sizeof(line), "%lu",
             (unsigned long)flow_sensor_pulse_count());
    text_line(4, line, DISPLAY_GREEN);
    text_line(7, "BACK RETURN", DISPLAY_YELLOW);
    break;
  case SCREEN_ABOUT:
    text_line(0, "ABOUT", DISPLAY_CYAN);
    text_line(2, "AQUABOX", DISPLAY_WHITE);
    text_line(4, "VERSION 010", DISPLAY_GREEN);
    text_line(7, "BACK RETURN", DISPLAY_YELLOW);
    break;
  }
}
static void local_ui_task(void *arg) {
  (void)arg;
  screen_t screen = SCREEN_HOME;
  bool refresh = true;
  TickType_t last_refresh = xTaskGetTickCount();
  for (;;) {
    const button_event_t event = buttons_poll();
    if (event == BUTTON_ENTER && screen == SCREEN_HOME) {
      screen = SCREEN_STATUS;
      refresh = true;
    } else if (event == BUTTON_BACK && screen != SCREEN_HOME) {
      screen = SCREEN_HOME;
      refresh = true;
    } else if (event == BUTTON_UP && screen > SCREEN_STATUS) {
      screen--;
      refresh = true;
    } else if (event == BUTTON_DOWN && screen >= SCREEN_STATUS &&
               screen < SCREEN_ABOUT) {
      screen++;
      refresh = true;
    }
    if (xTaskGetTickCount() - last_refresh >= pdMS_TO_TICKS(500)) {
      refresh = true;
      last_refresh = xTaskGetTickCount();
    }
    if (refresh) {
      render(screen);
      refresh = false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
BaseType_t local_ui_start(void) {
  ESP_LOGI(TAG, "Starting offline local interface");
  return xTaskCreatePinnedToCore(local_ui_task, "local_ui", 4096, NULL, 5, NULL,
                                 1);
}
