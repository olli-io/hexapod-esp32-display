idf_component_register(
    SRCS "StubRenderer.cpp" "EyeRenderer.cpp" "EyeRaster.cpp" "EyeAnim.cpp"
    INCLUDE_DIRS "include"
    REQUIRES expression
    PRIV_REQUIRES config display u8g2 esp_timer esp_hw_support)
