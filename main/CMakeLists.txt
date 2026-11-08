idf_component_register(
    SRCS "main.c" "mic.c" "mpu6500.c" "ota.c" "ws2812.c"
         "core/audio.c" "core/hat.c" "core/color.c" "core/fluid.c" "core/motion.c" "core/mpu6500_decode.c"
         "core/tap.c"
    INCLUDE_DIRS "." "core"
    REQUIRES esp_driver_spi esp_driver_i2c esp_driver_i2s esp_driver_gpio esp_timer log freertos
             app_update esp_app_format esp_event esp_http_server esp_netif esp_wifi nvs_flash
)
# The physics dominates frame time; optimise the core for speed.
set_source_files_properties(
    core/fluid.c core/motion.c
    PROPERTIES COMPILE_OPTIONS "-O2")
