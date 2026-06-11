idf_component_register(
    SRCS "main.cpp"
    PRIV_REQUIRES config display expression protocol renderer uart_transport
                  esp_timer)
