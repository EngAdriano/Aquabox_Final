#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_BLACK 0x0000
#define DISPLAY_WHITE 0xFFFF
#define DISPLAY_BLUE 0x001F
#define DISPLAY_GREEN 0x07E0
#define DISPLAY_RED 0xF800
#define DISPLAY_YELLOW 0xFFE0
#define DISPLAY_CYAN 0x07FF

typedef enum {
  DISPLAY_FONT_SMALL = 2,
  DISPLAY_FONT_MEDIUM = 3,
  DISPLAY_FONT_LARGE = 4,
} display_font_size_t;

typedef enum {
  DISPLAY_ICON_TANK,
  DISPLAY_ICON_PUMP,
  DISPLAY_ICON_VALVE,
  DISPLAY_ICON_FLOW,
  DISPLAY_ICON_ALERT,
} display_icon_t;

esp_err_t st7735_display_init(void);
void st7735_display_clear(uint16_t color);
void st7735_display_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                              uint16_t height, uint16_t color);
void st7735_display_draw_rect(uint16_t x, uint16_t y, uint16_t width,
                              uint16_t height, uint16_t color);
void st7735_display_draw_line(uint16_t x0, uint16_t y0, uint16_t x1,
                              uint16_t y1, uint16_t color);
void st7735_display_text_font(uint16_t x, uint16_t y, const char *text,
                              uint16_t color, display_font_size_t size);
void st7735_display_text(uint16_t x, uint16_t y, const char *text,
                         uint16_t color, uint8_t scale);
void st7735_display_icon(uint16_t x, uint16_t y, display_icon_t icon,
                         uint16_t color);
