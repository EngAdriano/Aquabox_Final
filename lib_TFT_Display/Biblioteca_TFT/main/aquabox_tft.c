#include "aquabox_tft.h"

#include <string.h>
#include "esp_rom_sys.h"
#include "esp_log.h"

#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_NORON   0x13
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_RASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_COLMOD  0x3A
#define CMD_INVON   0x21
#define CMD_INVOFF  0x20

/* Fonte ASCII 5x7, caracteres 0x20..0x7f; cada byte representa uma coluna. */
static const uint8_t s_font5x7[][5] = {
 {0,0,0,0,0},{0,0,0x5f,0,0},{0,7,0,7,0},{0x14,0x7f,0x14,0x7f,0x14},{0x24,0x2a,0x7f,0x2a,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},{0,0x1c,0x22,0x41,0},{0,0x41,0x22,0x1c,0},{0x14,8,0x3e,8,0x14},{8,8,0x3e,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,8,4,2},
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},
 {0x32,0x49,0x79,0x41,0x3e},{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
 {0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},{0,0x7f,0x41,0x41,0},{2,4,8,0x10,0x20},{0,0x41,0x41,0x7f,0},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},
 {0,1,2,4,0},{0x20,0x54,0x54,0x54,0x78},{0x7f,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7f},{0x38,0x54,0x54,0x54,0x18},{8,0x7e,9,1,2},{0x0c,0x52,0x52,0x52,0x3e},{0x7f,8,4,4,0x78},{0,0x44,0x7d,0x40,0},{0x20,0x40,0x44,0x3d,0},{0x7f,0x10,0x28,0x44,0},{0,0x41,0x7f,0x40,0},{0x7c,4,0x18,4,0x78},{0x7c,8,4,4,0x78},{0x38,0x44,0x44,0x44,0x38},
 {0x7c,0x14,0x14,0x14,8},{8,0x14,0x14,0x18,0x7c},{0x7c,8,4,4,8},{0x48,0x54,0x54,0x54,0x20},{4,0x3f,0x44,0x40,0x20},{0x3c,0x40,0x40,0x20,0x7c},{0x1c,0x20,0x40,0x20,0x1c},{0x3c,0x40,0x30,0x40,0x3c},{0x44,0x28,0x10,0x28,0x44},{0x0c,0x50,0x50,0x50,0x3c},{0x44,0x64,0x54,0x4c,0x44},{0,8,0x36,0x41,0},{0,0,0x7f,0,0},{0,0x41,0x36,8,0},{0x10,8,8,0x10,8},{0,0,0,0,0}
};

static void write_cmd(aquabox_tft_t *tft, uint8_t command) {
    spi_transaction_t t = { .length = 8, .tx_buffer = &command };
    gpio_set_level(tft->dc_pin, 0); spi_device_polling_transmit(tft->spi, &t);
}
static void write_data(aquabox_tft_t *tft, const void *data, size_t length) {
    spi_transaction_t t = { .length = length * 8, .tx_buffer = data };
    gpio_set_level(tft->dc_pin, 1); spi_device_polling_transmit(tft->spi, &t);
}
static void set_window(aquabox_tft_t *tft, int x, int y, int w, int h) {
    uint8_t d[4];
    write_cmd(tft, CMD_CASET); d[0]=x>>8; d[1]=x; d[2]=(x+w-1)>>8; d[3]=x+w-1; write_data(tft,d,4);
    write_cmd(tft, CMD_RASET); d[0]=y>>8; d[1]=y; d[2]=(y+h-1)>>8; d[3]=y+h-1; write_data(tft,d,4); write_cmd(tft,CMD_RAMWR);
}
static void pixel(aquabox_tft_t *tft, int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= AQUABOX_TFT_WIDTH || y >= AQUABOX_TFT_HEIGHT) return;
    uint8_t d[] = { color >> 8, color }; set_window(tft,x,y,1,1); write_data(tft,d,2);
}
static int scale_of(aquabox_tft_font_t font) { return (int)font; }

esp_err_t aquabox_tft_init(aquabox_tft_t *tft, const aquabox_tft_config_t *cfg) {
    if (!tft || !cfg) return ESP_ERR_INVALID_ARG;
    memset(tft, 0, sizeof(*tft)); tft->dc_pin = cfg->pin_dc; tft->background = AQ_COLOR_BLACK;
    spi_bus_config_t bus = { .mosi_io_num=cfg->pin_mosi, .sclk_io_num=cfg->pin_sclk, .miso_io_num=-1, .max_transfer_sz=4096 };
    esp_err_t err = spi_bus_initialize(cfg->spi_host, &bus, SPI_DMA_CH_AUTO); if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    spi_device_interface_config_t dev = { .clock_speed_hz=(int)(cfg->clock_hz ? cfg->clock_hz : 10000000), .mode=0, .spics_io_num=cfg->pin_cs, .queue_size=1 };
    if ((err=spi_bus_add_device(cfg->spi_host,&dev,&tft->spi)) != ESP_OK) return err;
    gpio_config_t io = { .pin_bit_mask=(1ULL<<cfg->pin_dc)|(1ULL<<cfg->pin_rst), .mode=GPIO_MODE_OUTPUT }; gpio_config(&io);
    gpio_set_level(cfg->pin_rst,0); esp_rom_delay_us(10000); gpio_set_level(cfg->pin_rst,1); esp_rom_delay_us(120000);
    write_cmd(tft,CMD_SWRESET); esp_rom_delay_us(150000); write_cmd(tft,CMD_SLPOUT); esp_rom_delay_us(120000);
    uint8_t color_mode=0x05, madctl=cfg->bgr ? 0xC8 : 0xC0; write_cmd(tft,CMD_COLMOD); write_data(tft,&color_mode,1); write_cmd(tft,CMD_MADCTL); write_data(tft,&madctl,1);
    write_cmd(tft, cfg->invert_colors ? CMD_INVON : CMD_INVOFF); write_cmd(tft,CMD_NORON); write_cmd(tft,CMD_DISPON); esp_rom_delay_us(100000); aquabox_tft_fill(tft,AQ_COLOR_BLACK); return ESP_OK;
}
void aquabox_tft_set_backlight(uint8_t percent) { (void)percent; }
void aquabox_tft_fill(aquabox_tft_t *tft, uint16_t color) { aquabox_tft_rect(tft,0,0,AQUABOX_TFT_WIDTH,AQUABOX_TFT_HEIGHT,color); }
void aquabox_tft_rect(aquabox_tft_t *tft,int x,int y,int w,int h,uint16_t color) {
    if (!tft || w<=0 || h<=0) return; if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;} if(x+w>AQUABOX_TFT_WIDTH)w=AQUABOX_TFT_WIDTH-x; if(y+h>AQUABOX_TFT_HEIGHT)h=AQUABOX_TFT_HEIGHT-y; if(w<=0||h<=0)return;
    uint8_t line[256]; for(int i=0;i<w*2;i+=2){line[i]=color>>8;line[i+1]=color;} set_window(tft,x,y,w,h); for(int row=0;row<h;row++)write_data(tft,line,w*2);
}
void aquabox_tft_rect_outline(aquabox_tft_t *tft,int x,int y,int w,int h,uint16_t c){aquabox_tft_rect(tft,x,y,w,1,c);aquabox_tft_rect(tft,x,y+h-1,w,1,c);aquabox_tft_rect(tft,x,y,1,h,c);aquabox_tft_rect(tft,x+w-1,y,1,h,c);}
void aquabox_tft_text(aquabox_tft_t *tft,int x,int y,const char *text,aquabox_tft_font_t font,uint16_t fg,uint16_t bg) {
    int s=scale_of(font); while(*text){uint8_t ch=(uint8_t)*text++; if(ch<0x20||ch>0x7f)ch='?'; const uint8_t *g=s_font5x7[ch-0x20]; for(int col=0;col<5;col++)for(int row=0;row<7;row++){uint16_t c=(g[col]&(1<<row))?fg:bg; aquabox_tft_rect(tft,x+col*s,y+row*s,s,s,c);} x+=6*s;}
}
void aquabox_tft_icon(aquabox_tft_t *tft,int x,int y,aquabox_tft_icon_t icon,uint16_t c) {
    switch(icon){
    case AQ_ICON_PUMP: aquabox_tft_rect_outline(tft,x+2,y+5,12,9,c); aquabox_tft_rect(tft,x,y+8,3,3,c); aquabox_tft_rect(tft,x+14,y+8,3,3,c); aquabox_tft_rect(tft,x+6,y+1,4,4,c); break;
    case AQ_ICON_VALVE: aquabox_tft_rect_outline(tft,x+3,y+6,10,6,c); for(int i=0;i<4;i++)pixel(tft,x+5+i,y+4-i,c),pixel(tft,x+11-i,y+4-i,c); break;
    case AQ_ICON_TANK: aquabox_tft_rect_outline(tft,x+3,y+1,10,14,c); aquabox_tft_rect(tft,x+4,y+10,8,4,c); break;
    case AQ_ICON_DROP: for(int i=0;i<6;i++)aquabox_tft_rect(tft,x+7-i/2,y+i,1+i,1,c); aquabox_tft_rect(tft,x+4,y+6,7,5,c); break;
    case AQ_ICON_CLOCK: aquabox_tft_rect_outline(tft,x+2,y+2,12,12,c); aquabox_tft_rect(tft,x+8,y+5,1,5,c); aquabox_tft_rect(tft,x+8,y+9,4,1,c); break;
    case AQ_ICON_WARNING: for(int i=0;i<8;i++)aquabox_tft_rect(tft,x+8-i,y+i,1+2*i,1,c); aquabox_tft_rect(tft,x+7,y+6,2,4,AQ_COLOR_BLACK); pixel(tft,x+8,y+12,AQ_COLOR_BLACK); break;
    case AQ_ICON_GEAR: aquabox_tft_rect_outline(tft,x+4,y+4,8,8,c); aquabox_tft_rect(tft,x+7,y,2,4,c); aquabox_tft_rect(tft,x+7,y+12,2,4,c); aquabox_tft_rect(tft,x,y+7,4,2,c); aquabox_tft_rect(tft,x+12,y+7,4,2,c); break;
    case AQ_ICON_BACK: for(int i=0;i<7;i++)pixel(tft,x+i,y+7-i,c),pixel(tft,x+i,y+7+i,c); aquabox_tft_rect(tft,x+5,y+7,10,1,c); break; }
}
static void header(aquabox_tft_t*t,const char*title,uint16_t c){aquabox_tft_rect(t,0,0,128,17,c);aquabox_tft_text(t,4,3,title,AQ_TFT_FONT_NORMAL,AQ_COLOR_BLACK,c);}
void aquabox_ui_boot(aquabox_tft_t*t,const char*v,const char*p){aquabox_tft_fill(t,AQ_COLOR_NAVY);aquabox_tft_icon(t,56,24,AQ_ICON_DROP,AQ_COLOR_CYAN);aquabox_tft_text(t,30,52,"AQUABOX",AQ_TFT_FONT_NORMAL,AQ_COLOR_WHITE,AQ_COLOR_NAVY);aquabox_tft_text(t,30,73,v,AQ_TFT_FONT_SMALL,AQ_COLOR_CYAN,AQ_COLOR_NAVY);aquabox_tft_rect_outline(t,9,105,110,20,AQ_COLOR_CYAN);aquabox_tft_text(t,14,111,p,AQ_TFT_FONT_SMALL,AQ_COLOR_WHITE,AQ_COLOR_NAVY);}
void aquabox_ui_home(aquabox_tft_t*t,const aquabox_ui_status_t*s){char b[18];aquabox_tft_fill(t,AQ_COLOR_BLACK);header(t,s->automatic_mode?"AUTO":"MANUAL",s->automatic_mode?AQ_COLOR_GREEN:AQ_COLOR_ORANGE);aquabox_tft_icon(t,5,22,AQ_ICON_CLOCK,AQ_COLOR_CYAN);aquabox_tft_text(t,27,23,s->time?s->time,"--:--",AQ_TFT_FONT_NORMAL,AQ_COLOR_WHITE,AQ_COLOR_BLACK);aquabox_tft_text(t,80,27,s->date?s->date,"--/--",AQ_TFT_FONT_SMALL,AQ_COLOR_GRAY,AQ_COLOR_BLACK);aquabox_tft_icon(t,5,45,AQ_ICON_PUMP,s->pump_on?AQ_COLOR_GREEN:AQ_COLOR_GRAY);aquabox_tft_text(t,27,49,s->pump_on?"BOMBA ON":"BOMBA OFF",AQ_TFT_FONT_SMALL,s->pump_on?AQ_COLOR_GREEN:AQ_COLOR_GRAY,AQ_COLOR_BLACK);for(int i=0;i<3;i++){int y=68+i*22;uint16_t c=s->valve_on[i]?AQ_COLOR_GREEN:AQ_COLOR_GRAY;aquabox_tft_icon(t,4,y,AQ_ICON_VALVE,c);snprintf(b,sizeof(b),"S%d  %s",i+1,s->valve_on[i]?"ABERTA":"FECHADA");aquabox_tft_text(t,25,y+4,b,AQ_TFT_FONT_SMALL,c,AQ_COLOR_BLACK);aquabox_tft_icon(t,92,y,AQ_ICON_TANK,s->tank_low[i]?AQ_COLOR_YELLOW:(s->tank_high[i]?AQ_COLOR_GREEN:AQ_COLOR_GRAY));}snprintf(b,sizeof(b),"%u L/h",s->flow_lph);aquabox_tft_icon(t,5,137,AQ_ICON_DROP,AQ_COLOR_CYAN);aquabox_tft_text(t,25,141,b,AQ_TFT_FONT_SMALL,AQ_COLOR_CYAN,AQ_COLOR_BLACK);if(s->alarms_active){aquabox_tft_icon(t,103,137,AQ_ICON_WARNING,AQ_COLOR_RED);aquabox_tft_text(t,4,153,"ENTER menu",AQ_TFT_FONT_SMALL,AQ_COLOR_GRAY,AQ_COLOR_BLACK);}}
void aquabox_ui_menu(aquabox_tft_t*t,const char*title,const char*const*items,size_t n,size_t sel){aquabox_tft_fill(t,AQ_COLOR_BLACK);header(t,title,AQ_COLOR_BLUE);for(size_t i=0;i<n&&i<6;i++){int y=23+(int)i*21;uint16_t bg=i==sel?AQ_COLOR_CYAN:AQ_COLOR_BLACK;uint16_t fg=i==sel?AQ_COLOR_BLACK:AQ_COLOR_WHITE;aquabox_tft_rect(t,4,y,120,17,bg);aquabox_tft_text(t,8,y+3,items[i],AQ_TFT_FONT_SMALL,fg,bg);}aquabox_tft_text(t,4,150,"UP/DN ENTER BACK",AQ_TFT_FONT_SMALL,AQ_COLOR_GRAY,AQ_COLOR_BLACK);}
void aquabox_ui_channel_config(aquabox_tft_t*t,uint8_t ch,bool en,bool irr,uint16_t mins){char title[12],v[20];snprintf(title,sizeof(title),"CANAL S%u",ch);aquabox_tft_fill(t,AQ_COLOR_BLACK);header(t,title,AQ_COLOR_BLUE);snprintf(v,sizeof(v),"Habilitado: %s",en?"SIM":"NAO");aquabox_tft_text(t,6,29,v,AQ_TFT_FONT_SMALL,AQ_COLOR_WHITE,AQ_COLOR_BLACK);snprintf(v,sizeof(v),"Modo: %s",irr?"IRRIGACAO":"CAIXA");aquabox_tft_text(t,6,52,v,AQ_TFT_FONT_SMALL,AQ_COLOR_CYAN,AQ_COLOR_BLACK);snprintf(v,sizeof(v),"Limite: %u min",mins);aquabox_tft_text(t,6,75,v,AQ_TFT_FONT_SMALL,AQ_COLOR_YELLOW,AQ_COLOR_BLACK);aquabox_tft_icon(t,55,100,irr?AQ_ICON_CLOCK:AQ_ICON_TANK,irr?AQ_COLOR_CYAN:AQ_COLOR_GREEN);aquabox_tft_text(t,5,145,"ENTER editar",AQ_TFT_FONT_SMALL,AQ_COLOR_GRAY,AQ_COLOR_BLACK);}
void aquabox_ui_alarm(aquabox_tft_t*t,const char*code,const char*msg,bool critical){uint16_t c=critical?AQ_COLOR_RED:AQ_COLOR_YELLOW;aquabox_tft_fill(t,AQ_COLOR_BLACK);header(t,critical?"ALARME CRITICO":"ALERTA",c);aquabox_tft_icon(t,55,28,AQ_ICON_WARNING,c);aquabox_tft_text(t,25,55,code,AQ_TFT_FONT_NORMAL,c,AQ_COLOR_BLACK);aquabox_tft_text(t,8,83,msg,AQ_TFT_FONT_SMALL,AQ_COLOR_WHITE,AQ_COLOR_BLACK);aquabox_tft_text(t,8,140,"BACK: reconhecer",AQ_TFT_FONT_SMALL,AQ_COLOR_GRAY,AQ_COLOR_BLACK);}
void aquabox_ui_demo(aquabox_tft_t*t){const char*menu[]={"Canais","Sistema","Comandos manuais","Alarmes"};aquabox_ui_boot(t,"FW 0.1.0","TFT OK  RTC OK");esp_rom_delay_us(1200000);aquabox_ui_menu(t,"MENU",menu,4,1);}
