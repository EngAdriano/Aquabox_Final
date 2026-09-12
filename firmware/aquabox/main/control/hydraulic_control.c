#include "hydraulic_control.h"
#include "aquabox_config.h"
#include "drivers/flow_sensor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "hal/hardware.h"
#include <inttypes.h>

typedef enum {
  CYCLE_IDLE,
  CYCLE_PREOPEN,
  CYCLE_FILLING,
  CYCLE_IRRIGATING
} cycle_state_t;
typedef struct {
  cycle_state_t state;
  int active_channel;
  int64_t cycle_started_us;
  int64_t pump_started_us;
  int64_t pump_stop_requested_us;
  uint32_t alarms;
  bool manual_mode;
  bool independent_pump;
  bool rtc_valid;
} controller_t;
static const char *TAG = "hydraulic";
static aquabox_channel_config_t channel_config[AQUABOX_CHANNEL_COUNT] = {
    {true, AQUABOX_CHANNEL_TANK, AQUABOX_DEFAULT_FILL_TIMEOUT_MS},
    {true, AQUABOX_CHANNEL_TANK, AQUABOX_DEFAULT_FILL_TIMEOUT_MS},
    {true, AQUABOX_CHANNEL_TANK, AQUABOX_DEFAULT_FILL_TIMEOUT_MS}};
static bool elapsed(int64_t start_us, int64_t duration_ms) {
  return start_us && esp_timer_get_time() - start_us >= duration_ms * 1000;
}
static bool has_blocking_alarm(const controller_t *c) {
  return c->alarms & (AQUABOX_ALARM_NO_FLOW | AQUABOX_ALARM_LEVEL_INCONSISTENT |
                      AQUABOX_ALARM_CONFIG_INVALID);
}
static void stop_safely(controller_t *c, uint32_t alarm) {
  hardware_safe_stop();
  c->state = CYCLE_IDLE;
  c->active_channel = -1;
  c->cycle_started_us = 0;
  c->pump_started_us = 0;
  c->pump_stop_requested_us = 0;
  c->alarms |= alarm;
  ESP_LOGE(TAG, "Safety stop, alarms=0x%02" PRIx32, c->alarms);
}
static void finish_cycle(controller_t *c) {
  if (c->active_channel >= 0)
    hardware_set_solenoid(c->active_channel, false);
  c->state = CYCLE_IDLE;
  c->active_channel = -1;
  c->cycle_started_us = 0;
  c->pump_stop_requested_us = esp_timer_get_time();
}
static void update_pump(controller_t *c) {
  bool cycle_demand = c->state == CYCLE_FILLING || c->state == CYCLE_IRRIGATING;
  if (cycle_demand || (c->independent_pump && !has_blocking_alarm(c))) {
    hardware_set_pump(true);
    if (!c->pump_started_us)
      c->pump_started_us = esp_timer_get_time();
  } else if (elapsed(c->pump_stop_requested_us, AQUABOX_PUMP_OFF_DELAY_MS)) {
    hardware_set_pump(false);
    c->pump_started_us = 0;
  }
}
static void check_flow(controller_t *c) {
  if (!hardware_pump_is_on() || !c->pump_started_us)
    return;
  int64_t last = flow_sensor_last_pulse_us(),
          ref = last ? last : c->pump_started_us,
          timeout = last ? AQUABOX_FLOW_RUNNING_TIMEOUT_MS
                         : AQUABOX_FLOW_START_TIMEOUT_MS;
  if (esp_timer_get_time() - ref > timeout * 1000)
    stop_safely(c, AQUABOX_ALARM_NO_FLOW);
}
static void evaluate_tanks(controller_t *c) {
  for (unsigned ch = 0; ch < AQUABOX_CHANNEL_COUNT; ++ch) {
    if (!channel_config[ch].enabled ||
        channel_config[ch].mode != AQUABOX_CHANNEL_TANK)
      continue;
    bool low = hardware_level_low_active(ch),
         high = hardware_level_high_active(ch);
    if (low && high) {
      if (c->active_channel == (int)ch)
        stop_safely(c, AQUABOX_ALARM_LEVEL_INCONSISTENT);
      else {
        hardware_set_solenoid(ch, false);
        c->alarms |= AQUABOX_ALARM_LEVEL_INCONSISTENT;
      }
      continue;
    }
    if (c->active_channel == (int)ch && c->state == CYCLE_FILLING) {
      if (high)
        finish_cycle(c);
      else if (elapsed(c->cycle_started_us, channel_config[ch].max_fill_ms))
        stop_safely(c, AQUABOX_ALARM_FILL_TIMEOUT);
      return;
    }
  }
  if (c->state != CYCLE_IDLE || c->manual_mode || has_blocking_alarm(c))
    return;
  for (unsigned ch = 0; ch < AQUABOX_CHANNEL_COUNT; ++ch)
    if (channel_config[ch].enabled &&
        channel_config[ch].mode == AQUABOX_CHANNEL_TANK &&
        hardware_level_low_active(ch) && !hardware_level_high_active(ch)) {
      hardware_set_solenoid(ch, true);
      c->active_channel = ch;
      c->cycle_started_us = esp_timer_get_time();
      c->state = CYCLE_PREOPEN;
      ESP_LOGI(TAG, "Tank %u valve opened", ch + 1);
      return;
    }
}
static void hydraulic_control_task(void *arg) {
  (void)arg;
  controller_t c = {
      .state = CYCLE_IDLE, .active_channel = -1, .rtc_valid = false};
  hardware_safe_stop();
  vTaskDelay(pdMS_TO_TICKS(250));
  ESP_LOGI(TAG, "Aquabox %s ready", AQUABOX_VERSION);
  for (;;) {
    c.manual_mode = hardware_manual_mode_active();
    c.independent_pump = hardware_independent_pump_requested();
    if (c.manual_mode && c.state != CYCLE_IDLE)
      finish_cycle(&c);
    if (!c.rtc_valid)
      c.alarms |= AQUABOX_ALARM_RTC_INVALID;
    if (!c.manual_mode)
      evaluate_tanks(&c);
    if (c.state == CYCLE_PREOPEN &&
        elapsed(c.cycle_started_us, AQUABOX_PREOPEN_MS))
      c.state = CYCLE_FILLING;
    update_pump(&c);
    check_flow(&c);
    vTaskDelay(pdMS_TO_TICKS(AQUABOX_CONTROL_PERIOD_MS));
  }
}
BaseType_t hydraulic_control_start(void) {
  return xTaskCreatePinnedToCore(hydraulic_control_task, "hydraulic_ctl", 4096,
                                 NULL, 12, NULL, 1);
}
