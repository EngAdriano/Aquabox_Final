/**
 * @file aquabox_tft.h
 * @brief Interface grafica para TFT ST7735 1.8" (128 x 160) do Aquabox.
 *
 * A biblioteca nao comanda bomba ou valvulas. A tarefa de interface entrega
 * periodicamente um aquabox_ui_status_t, produzido pela camada de controle.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AQUABOX_TFT_WIDTH   128
#define AQUABOX_TFT_HEIGHT  160

/* Cores RGB565. */
#define AQ_COLOR_BLACK   0x0000
#define AQ_COLOR_WHITE   0xFFFF
#define AQ_COLOR_NAVY    0x000F
#define AQ_COLOR_BLUE    0x001F
#define AQ_COLOR_CYAN    0x07FF
#define AQ_COLOR_GREEN   0x07E0
#define AQ_COLOR_YELLOW  0xFFE0
#define AQ_COLOR_ORANGE  0xFD20
#define AQ_COLOR_RED     0xF800
#define AQ_COLOR_GRAY    0x8410
#define AQ_COLOR_DARK    0x2104

typedef enum {
    AQ_TFT_FONT_SMALL = 1,  /**< 5x7 px */
    AQ_TFT_FONT_NORMAL = 2, /**< 10x14 px */
    AQ_TFT_FONT_LARGE = 3,  /**< 15x21 px */
} aquabox_tft_font_t;

typedef enum {
    AQ_ICON_PUMP,
    AQ_ICON_VALVE,
    AQ_ICON_TANK,
    AQ_ICON_DROP,
    AQ_ICON_CLOCK,
    AQ_ICON_WARNING,
    AQ_ICON_GEAR,
    AQ_ICON_BACK,
} aquabox_tft_icon_t;

typedef struct {
    spi_host_device_t spi_host;
    gpio_num_t pin_mosi;
    gpio_num_t pin_sclk;
    gpio_num_t pin_cs;
    gpio_num_t pin_dc;
    gpio_num_t pin_rst;
    uint32_t clock_hz;          /**< 10 MHz e um ponto de partida seguro. */
    bool bgr;                   /**< true para modulos ST7735 com ordem BGR. */
    bool invert_colors;
} aquabox_tft_config_t;

typedef struct {
    bool automatic_mode;
    bool pump_on;
    bool valve_on[3];
    bool tank_low[3];
    bool tank_high[3];
    bool alarms_active;
    uint16_t flow_lph;
    const char *date;           /**< "11/09/26" */
    const char *time;           /**< "14:35" */
} aquabox_ui_status_t;

typedef struct {
    spi_device_handle_t spi;
    gpio_num_t dc_pin;
    uint16_t background;
} aquabox_tft_t;

esp_err_t aquabox_tft_init(aquabox_tft_t *tft, const aquabox_tft_config_t *config);
void aquabox_tft_set_backlight(uint8_t percent); /* Gancho para PWM externo, se instalado. */
void aquabox_tft_fill(aquabox_tft_t *tft, uint16_t color);
void aquabox_tft_rect(aquabox_tft_t *tft, int x, int y, int w, int h, uint16_t color);
void aquabox_tft_rect_outline(aquabox_tft_t *tft, int x, int y, int w, int h, uint16_t color);
void aquabox_tft_text(aquabox_tft_t *tft, int x, int y, const char *text,
                      aquabox_tft_font_t font, uint16_t foreground, uint16_t background);
void aquabox_tft_icon(aquabox_tft_t *tft, int x, int y, aquabox_tft_icon_t icon, uint16_t color);

/* Telas diretamente alinhadas ao item 5 do planejamento. */
void aquabox_ui_boot(aquabox_tft_t *tft, const char *firmware_version, const char *peripheral_status);
void aquabox_ui_home(aquabox_tft_t *tft, const aquabox_ui_status_t *status);
void aquabox_ui_menu(aquabox_tft_t *tft, const char *title, const char *const *items,
                     size_t item_count, size_t selected);
void aquabox_ui_channel_config(aquabox_tft_t *tft, uint8_t channel, bool enabled,
                               bool irrigation, uint16_t max_minutes);
void aquabox_ui_alarm(aquabox_tft_t *tft, const char *code, const char *message, bool critical);
void aquabox_ui_demo(aquabox_tft_t *tft); /* Exemplo de todos os recursos visuais. */

#ifdef __cplusplus
}
#endif
