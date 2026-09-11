/* Aquabox firmware base: apenas a tarefa hidráulica pode acionar as saídas. */
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AQUABOX_VERSION "0.1.0"
#define N_CHANNELS 3
#define CONTROL_PERIOD_MS 100
#define PREOPEN_MS 1000
#define PUMP_OFF_DELAY_MS 1000
#define FLOW_START_TIMEOUT_MS 5000
#define FLOW_RUNNING_TIMEOUT_MS 3000
#define DEFAULT_FILL_TIMEOUT_MS (10 * 60 * 1000)

#define PIN_FLOW GPIO_NUM_14
#define PIN_PUMP GPIO_NUM_16
#define PIN_SOL1 GPIO_NUM_17
#define PIN_SOL2 GPIO_NUM_18
#define PIN_SOL3 GPIO_NUM_21
#define PIN_S1_LOW GPIO_NUM_39
#define PIN_S1_HIGH GPIO_NUM_40
#define PIN_S2_LOW GPIO_NUM_41
#define PIN_S2_HIGH GPIO_NUM_42
#define PIN_S3_LOW GPIO_NUM_43
#define PIN_S3_HIGH GPIO_NUM_44
#define PIN_PUMP_SWITCH GPIO_NUM_38
#define PIN_MANUAL_SWITCH GPIO_NUM_47

static const char *TAG = "aquabox";
static const gpio_num_t sol_pin[N_CHANNELS] = {PIN_SOL1, PIN_SOL2, PIN_SOL3};
static const gpio_num_t low_pin[N_CHANNELS] = {PIN_S1_LOW, PIN_S2_LOW, PIN_S3_LOW};
static const gpio_num_t high_pin[N_CHANNELS] = {PIN_S1_HIGH, PIN_S2_HIGH, PIN_S3_HIGH};

typedef enum { CHANNEL_DISABLED, CHANNEL_TANK, CHANNEL_IRRIGATION } channel_mode_t;
typedef enum { CYCLE_IDLE, CYCLE_PREOPEN, CYCLE_FILLING, CYCLE_IRRIGATING } cycle_state_t;
enum { ALARM_NO_FLOW = 1U << 0, ALARM_FILL_TIMEOUT = 1U << 1,
       ALARM_LEVEL_INCONSISTENT = 1U << 2, ALARM_RTC_INVALID = 1U << 3,
       ALARM_CONFIG_INVALID = 1U << 4 };

typedef struct { bool enabled; channel_mode_t mode; uint32_t max_fill_ms; } channel_config_t;
typedef struct {
    cycle_state_t state; int active_channel; int64_t cycle_started_us;
    int64_t pump_started_us; int64_t pump_stop_requested_us; uint32_t alarms;
    bool manual_mode; bool independent_pump; bool rtc_valid;
} controller_t;

/* Uma configuração futura da NVS substitui esses padrões de fábrica seguros e utilizáveis. */
static channel_config_t config[N_CHANNELS] = {
    {true, CHANNEL_TANK, DEFAULT_FILL_TIMEOUT_MS},
    {true, CHANNEL_TANK, DEFAULT_FILL_TIMEOUT_MS},
    {true, CHANNEL_TANK, DEFAULT_FILL_TIMEOUT_MS},
};
static volatile uint32_t flow_pulses;
static volatile int64_t last_flow_pulse_us;

static bool elapsed(int64_t start_us, int64_t ms) {
    return start_us && esp_timer_get_time() - start_us >= ms * 1000;
}
static void IRAM_ATTR flow_isr(void *unused) {
    (void)unused; flow_pulses++; last_flow_pulse_us = esp_timer_get_time();
}
static void solenoids_off(void) {
    for (int i = 0; i < N_CHANNELS; ++i) gpio_set_level(sol_pin[i], 0);
}
static bool blocking_alarm(const controller_t *c) {
    return c->alarms & (ALARM_NO_FLOW | ALARM_LEVEL_INCONSISTENT | ALARM_CONFIG_INVALID);
}
static void safe_stop(controller_t *c, uint32_t alarm) {
    gpio_set_level(PIN_PUMP, 0); solenoids_off();
    c->state = CYCLE_IDLE; c->active_channel = -1; c->cycle_started_us = 0;
    c->pump_started_us = 0; c->pump_stop_requested_us = 0; c->alarms |= alarm;
    ESP_LOGE(TAG, "Safety stop, alarms=0x%02" PRIx32, c->alarms);
}
static void end_cycle(controller_t *c) {
    if (c->active_channel >= 0) gpio_set_level(sol_pin[c->active_channel], 0);
    c->state = CYCLE_IDLE; c->active_channel = -1; c->cycle_started_us = 0;
    c->pump_stop_requested_us = esp_timer_get_time();
}
static void update_pump(controller_t *c) {
    bool cycle_demand = c->state == CYCLE_FILLING || c->state == CYCLE_IRRIGATING;
    bool demand = cycle_demand || (c->independent_pump && !blocking_alarm(c));
    if (demand) {
        gpio_set_level(PIN_PUMP, 1);
        if (!c->pump_started_us) c->pump_started_us = esp_timer_get_time();
    } else if (elapsed(c->pump_stop_requested_us, PUMP_OFF_DELAY_MS)) {
        gpio_set_level(PIN_PUMP, 0); c->pump_started_us = 0;
    }
}
static void check_flow(controller_t *c) {
    if (!gpio_get_level(PIN_PUMP) || !c->pump_started_us) return;
    int64_t ref = last_flow_pulse_us ? last_flow_pulse_us : c->pump_started_us;
    int64_t timeout = last_flow_pulse_us ? FLOW_RUNNING_TIMEOUT_MS : FLOW_START_TIMEOUT_MS;
    if (esp_timer_get_time() - ref > timeout * 1000) safe_stop(c, ALARM_NO_FLOW);
}
static void evaluate_tanks(controller_t *c) {
    for (int i = 0; i < N_CHANNELS; ++i) {
        if (!config[i].enabled || config[i].mode != CHANNEL_TANK) continue;
        bool low = !gpio_get_level(low_pin[i]), high = !gpio_get_level(high_pin[i]); /* active-low */
        if (low && high) {
            if (c->active_channel == i) safe_stop(c, ALARM_LEVEL_INCONSISTENT);
            else { gpio_set_level(sol_pin[i], 0); c->alarms |= ALARM_LEVEL_INCONSISTENT; }
            continue;
        }
        if (c->active_channel == i && c->state == CYCLE_FILLING) {
            if (high) end_cycle(c);
            else if (elapsed(c->cycle_started_us, config[i].max_fill_ms)) safe_stop(c, ALARM_FILL_TIMEOUT);
            return;
        }
    }
    if (c->state != CYCLE_IDLE || c->manual_mode || blocking_alarm(c)) return;
    /* A prioridade é S1, S2, S3. Um único caminho ativo torna esta linha de base segura. */
    for (int i = 0; i < N_CHANNELS; ++i) {
        if (config[i].enabled && config[i].mode == CHANNEL_TANK &&
            !gpio_get_level(low_pin[i]) && gpio_get_level(high_pin[i])) {
            gpio_set_level(sol_pin[i], 1); c->active_channel = i;
            c->cycle_started_us = esp_timer_get_time(); c->state = CYCLE_PREOPEN;
            ESP_LOGI(TAG, "Tank %d valve opened", i + 1); return;
        }
    }
}
static void hydraulic_control_task(void *arg) {
    (void)arg; controller_t c = {.state = CYCLE_IDLE, .active_channel = -1, .rtc_valid = false};
    solenoids_off(); gpio_set_level(PIN_PUMP, 0); vTaskDelay(pdMS_TO_TICKS(250));
    ESP_LOGI(TAG, "Aquabox %s ready", AQUABOX_VERSION);
    for (;;) {
        c.manual_mode = gpio_get_level(PIN_MANUAL_SWITCH) != 0;
        c.independent_pump = gpio_get_level(PIN_PUMP_SWITCH) != 0;
        if (c.manual_mode && c.state != CYCLE_IDLE) end_cycle(&c);
        if (!c.rtc_valid) c.alarms |= ALARM_RTC_INVALID; /* bloqueia apenas a irrigação */
        if (!c.manual_mode) evaluate_tanks(&c);
        if (c.state == CYCLE_PREOPEN && elapsed(c.cycle_started_us, PREOPEN_MS)) c.state = CYCLE_FILLING;
        update_pump(&c); check_flow(&c);
        vTaskDelay(pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}
static esp_err_t configure_gpio(void) {
    uint64_t out_mask = (1ULL << PIN_PUMP) | (1ULL << PIN_SOL1) | (1ULL << PIN_SOL2) | (1ULL << PIN_SOL3);
    uint64_t in_mask = (1ULL << PIN_FLOW) | (1ULL << PIN_S1_LOW) | (1ULL << PIN_S1_HIGH) |
        (1ULL << PIN_S2_LOW) | (1ULL << PIN_S2_HIGH) | (1ULL << PIN_S3_LOW) | (1ULL << PIN_S3_HIGH) |
        (1ULL << PIN_PUMP_SWITCH) | (1ULL << PIN_MANUAL_SWITCH);
    gpio_config_t out = {.pin_bit_mask=out_mask,.mode=GPIO_MODE_OUTPUT,.pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    gpio_config_t in = {.pin_bit_mask=in_mask,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    ESP_RETURN_ON_ERROR(gpio_config(&out), TAG, "output GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_config(&in), TAG, "input GPIO failed");
    solenoids_off(); gpio_set_level(PIN_PUMP, 0);
    ESP_RETURN_ON_ERROR(gpio_set_intr_type(PIN_FLOW, GPIO_INTR_POSEDGE), TAG, "flow ISR failed");
    ESP_RETURN_ON_ERROR(gpio_install_isr_service(ESP_INTR_FLAG_IRAM), TAG, "ISR service failed");
    return gpio_isr_handler_add(PIN_FLOW, flow_isr, NULL);
}
void app_main(void) {
    ESP_ERROR_CHECK(configure_gpio()); /* RF-01: As saídas são desativadas antes que as entradas sejam avaliadas. */
    BaseType_t ok = xTaskCreatePinnedToCore(hydraulic_control_task, "hydraulic_ctl", 4096, NULL, 12, NULL, 1);
    if (ok != pdPASS) { gpio_set_level(PIN_PUMP, 0); solenoids_off(); ESP_LOGE(TAG, "Control task not created"); }
}
