#include "st7735_display.h"

#include "aquabox_config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define ST_CMD_SWRESET 0x01
#define ST_CMD_SLPOUT 0x11
#define ST_CMD_COLMOD 0x3A
#define ST_CMD_MADCTL 0x36
#define ST_CMD_CASET 0x2A
#define ST_CMD_RASET 0x2B
#define ST_CMD_RAMWR 0x2C
#define ST_CMD_DISPON 0x29

static const char *TAG = "display";
static spi_device_handle_t display;

static void send_command(uint8_t command) {
  gpio_set_level(AQUABOX_PIN_TFT_DC, 0);
  spi_transaction_t tx = {.length = 8, .tx_buffer = &command};
  spi_device_polling_transmit(display, &tx);
}
static void send_data(const void *data, size_t bytes) {
  if (!bytes)
    return;
  gpio_set_level(AQUABOX_PIN_TFT_DC, 1);
  spi_transaction_t tx = {.length = bytes * 8, .tx_buffer = data};
  spi_device_polling_transmit(display, &tx);
}
static void command_data(uint8_t command, const uint8_t *data, size_t bytes) {
  send_command(command);
  send_data(data, bytes);
}
static void set_window(uint16_t x, uint16_t y, uint16_t width,
                       uint16_t height) {
  const uint8_t columns[] = {x >> 8, x, (uint16_t)(x + width - 1) >> 8,
                             (uint8_t)(x + width - 1)};
  const uint8_t rows[] = {y >> 8, y, (uint16_t)(y + height - 1) >> 8,
                          (uint8_t)(y + height - 1)};
  command_data(ST_CMD_CASET, columns, sizeof(columns));
  command_data(ST_CMD_RASET, rows, sizeof(rows));
  send_command(ST_CMD_RAMWR);
}
static const uint8_t font[37][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}, {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6},
    {7, 4, 4, 4, 7}, {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4},
    {7, 4, 5, 5, 7}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7}, {1, 1, 1, 5, 7},
    {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5},
    {7, 5, 5, 5, 7}, {7, 5, 7, 4, 4}, {7, 5, 5, 7, 1}, {7, 5, 7, 6, 5},
    {7, 4, 7, 1, 7}, {7, 2, 2, 2, 2}, {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2},
    {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5}, {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7},
    {0, 0, 0, 0, 0}};
static const uint8_t punctuation[][5] = {{0, 2, 0, 2, 0},
                                         {0, 0, 7, 0, 0},
                                         {0, 0, 0, 0, 2},
                                         {0, 0, 0, 0, 0}}; /* : - . space */
static const uint8_t *glyph(char c) {
  if (c >= '0' && c <= '9')
    return font[c - '0'];
  if (c >= 'A' && c <= 'Z')
    return font[10 + c - 'A'];
  if (c == ':')
    return punctuation[0];
  if (c == '-')
    return punctuation[1];
  if (c == '.')
    return punctuation[2];
  return punctuation[3];
}

esp_err_t st7735_display_init(void) {
  const spi_bus_config_t bus = {.mosi_io_num = AQUABOX_PIN_TFT_MOSI,
                                .miso_io_num = -1,
                                .sclk_io_num = AQUABOX_PIN_TFT_SCLK,
                                .quadwp_io_num = -1,
                                .quadhd_io_num = -1,
                                .max_transfer_sz = AQUABOX_TFT_WIDTH * 2};
  const spi_device_interface_config_t device = {
      .clock_speed_hz = AQUABOX_TFT_SPI_CLOCK_HZ,
      .mode = 0,
      .spics_io_num = AQUABOX_PIN_TFT_CS,
      .queue_size = 1};
  gpio_config_t output = {.pin_bit_mask = (1ULL << AQUABOX_PIN_TFT_DC) |
                                          (1ULL << AQUABOX_PIN_TFT_RST),
                          .mode = GPIO_MODE_OUTPUT,
                          .pull_up_en = GPIO_PULLUP_DISABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                          .intr_type = GPIO_INTR_DISABLE};
  ESP_RETURN_ON_ERROR(gpio_config(&output), TAG, "display control GPIO failed");
  gpio_set_level(AQUABOX_PIN_TFT_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(AQUABOX_PIN_TFT_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(120));
  ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO), TAG,
                      "display SPI bus failed");
  ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI2_HOST, &device, &display), TAG,
                      "display SPI device failed");
  send_command(ST_CMD_SWRESET);
  vTaskDelay(pdMS_TO_TICKS(150));
  send_command(ST_CMD_SLPOUT);
  vTaskDelay(pdMS_TO_TICKS(120));
  /* MV troca os eixos; A8 seleciona a orientação horizontal com ordem BGR. */
  const uint8_t color_mode = 0x05, orientation = 0xA8;
  command_data(ST_CMD_COLMOD, &color_mode, 1);
  command_data(ST_CMD_MADCTL, &orientation, 1);
  send_command(ST_CMD_DISPON);
  vTaskDelay(pdMS_TO_TICKS(100));
  st7735_display_clear(DISPLAY_BLACK);
  return ESP_OK;
}
void st7735_display_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                              uint16_t height, uint16_t color) {
  if (x >= AQUABOX_TFT_WIDTH || y >= AQUABOX_TFT_HEIGHT)
    return;
  if (x + width > AQUABOX_TFT_WIDTH)
    width = AQUABOX_TFT_WIDTH - x;
  if (y + height > AQUABOX_TFT_HEIGHT)
    height = AQUABOX_TFT_HEIGHT - y;
  uint8_t row[AQUABOX_TFT_WIDTH * 2];
  for (unsigned i = 0; i < width; i++) {
    row[2 * i] = color >> 8;
    row[2 * i + 1] = color;
  }
  set_window(x, y, width, height);
  for (unsigned i = 0; i < height; i++)
    send_data(row, width * 2);
}
void st7735_display_clear(uint16_t color) {
  st7735_display_fill_rect(0, 0, AQUABOX_TFT_WIDTH, AQUABOX_TFT_HEIGHT, color);
}
void st7735_display_draw_rect(uint16_t x, uint16_t y, uint16_t width,
                              uint16_t height, uint16_t color) {
  if (width == 0 || height == 0)
    return;
  st7735_display_fill_rect(x, y, width, 1, color);
  st7735_display_fill_rect(x, y + height - 1, width, 1, color);
  st7735_display_fill_rect(x, y, 1, height, color);
  st7735_display_fill_rect(x + width - 1, y, 1, height, color);
}
void st7735_display_draw_line(uint16_t x0, uint16_t y0, uint16_t x1,
                              uint16_t y1, uint16_t color) {
  int x = x0, y = y0, target_x = x1, target_y = y1;
  const int delta_x = x < target_x ? target_x - x : x - target_x;
  const int step_x = x < target_x ? 1 : -1;
  const int delta_y = y < target_y ? -(target_y - y) : -(y - target_y);
  const int step_y = y < target_y ? 1 : -1;
  int error = delta_x + delta_y;
  while (true) {
    if (x >= 0 && y >= 0)
      st7735_display_fill_rect((uint16_t)x, (uint16_t)y, 1, 1, color);
    if (x == target_x && y == target_y)
      break;
    const int twice_error = 2 * error;
    if (twice_error >= delta_y) {
      error += delta_y;
      x += step_x;
    }
    if (twice_error <= delta_x) {
      error += delta_x;
      y += step_y;
    }
  }
}
void st7735_display_text(uint16_t x, uint16_t y, const char *text,
                         uint16_t color, uint8_t scale) {
  for (; *text; ++text, x += 4 * scale) {
    const uint8_t *letter = glyph(*text);
    for (unsigned row = 0; row < 5; row++)
      for (unsigned col = 0; col < 3; col++)
        if (letter[row] & (1 << (2 - col)))
          st7735_display_fill_rect(x + col * scale, y + row * scale, scale,
                                   scale, color);
  }
}
void st7735_display_text_font(uint16_t x, uint16_t y, const char *text,
                              uint16_t color, display_font_size_t size) {
  st7735_display_text(x, y, text, color, (uint8_t)size);
}
void st7735_display_icon(uint16_t x, uint16_t y, display_icon_t icon,
                         uint16_t color) {
  switch (icon) {
  case DISPLAY_ICON_TANK:
    st7735_display_draw_rect(x + 2, y, 12, 16, color);
    st7735_display_fill_rect(x + 4, y + 10, 8, 4, color);
    break;
  case DISPLAY_ICON_PUMP:
    st7735_display_draw_rect(x + 3, y + 4, 10, 9, color);
    st7735_display_fill_rect(x, y + 7, 3, 3, color);
    st7735_display_fill_rect(x + 13, y + 7, 3, 3, color);
    st7735_display_fill_rect(x + 6, y + 7, 4, 3, color);
    break;
  case DISPLAY_ICON_VALVE:
    st7735_display_draw_line(x, y + 8, x + 16, y + 8, color);
    st7735_display_draw_line(x + 4, y + 3, x + 8, y + 8, color);
    st7735_display_draw_line(x + 8, y + 8, x + 4, y + 13, color);
    st7735_display_draw_line(x + 12, y + 3, x + 8, y + 8, color);
    st7735_display_draw_line(x + 8, y + 8, x + 12, y + 13, color);
    break;
  case DISPLAY_ICON_FLOW:
    st7735_display_draw_line(x, y + 8, x + 13, y + 8, color);
    st7735_display_draw_line(x + 13, y + 8, x + 9, y + 4, color);
    st7735_display_draw_line(x + 13, y + 8, x + 9, y + 12, color);
    break;
  case DISPLAY_ICON_ALERT:
    st7735_display_draw_line(x + 8, y, x, y + 15, color);
    st7735_display_draw_line(x, y + 15, x + 16, y + 15, color);
    st7735_display_draw_line(x + 16, y + 15, x + 8, y, color);
    st7735_display_fill_rect(x + 7, y + 5, 2, 5, color);
    st7735_display_fill_rect(x + 7, y + 12, 2, 2, color);
    break;
  }
}
