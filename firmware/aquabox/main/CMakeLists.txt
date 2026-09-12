idf_component_register(SRCS
                    "main.c"
                    "hal/hardware.c"
                    "drivers/flow_sensor.c"
                    "control/hydraulic_control.c"
                    INCLUDE_DIRS "." "include"
                    REQUIRES esp_driver_gpio esp_timer)
