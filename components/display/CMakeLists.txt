idf_component_register(
    SRCS "Display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES u8g2
    PRIV_REQUIRES config esp_driver_gpio)
