# U8g2's C core (csrc/) is framework-independent; only the Arduino C++
# wrapper is skipped. Unused fonts are dropped at link time by --gc-sections.
file(GLOB U8G2_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/u8g2/csrc/*.c")

idf_component_register(
    SRCS ${U8G2_SRCS} "u8g2_esp32_hal.c"
    INCLUDE_DIRS "u8g2/csrc" "include"
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi)
