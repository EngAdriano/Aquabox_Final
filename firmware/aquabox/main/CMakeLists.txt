idf_component_register(SRCS
                    "main.c"
                    "hal/hardware.c"
                    "drivers/flow_sensor.c"
                    "control/hydraulic_control.c"
                    "ui/buttons.c"
                    "ui/st7735_display.c"
                    "ui/local_ui.c"
                    INCLUDE_DIRS "." "include"
                    REQUIRES esp_driver_gpio esp_driver_spi esp_timer)
