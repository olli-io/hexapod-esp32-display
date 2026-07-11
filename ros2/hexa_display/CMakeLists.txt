cmake_minimum_required(VERSION 3.16)
project(hexa_display CXX C)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# This package is self-contained: everything it compiles lives under the
# package directory. The shared firmware pieces and the u8g2 C core are
# vendored under vendor/ (see vendor/README.md for provenance and re-sync).

# Vendored u8g2 C core (framework-independent), compiled into its own library
# so --gc-sections can drop the unused device drivers. The 38 MB font-data
# blobs are intentionally not vendored — the face draws no text.
file(GLOB U8G2_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/u8g2/*.c")
add_library(u8g2_core STATIC ${U8G2_SRCS})
target_include_directories(u8g2_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/u8g2")

# Platform-free pieces vendored from the firmware tree:
#   EyeAnim/EyeRaster        - the animation + rasterizer (also host-tested)
#   ExpressionController.cpp - only for expressionName()/gazeName(), the single
#                              source of truth for the on-wire enum names
set(CORE_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/core/EyeAnim.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/core/EyeRaster.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/core/ExpressionController.cpp"
)

add_executable(face_node
  src/face_node.cpp
  src/Sh1122Panel.cpp
  ${CORE_SRCS}
)

target_include_directories(face_node PRIVATE
  src
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/core"
)

target_link_libraries(face_node u8g2_core)
ament_target_dependencies(face_node rclcpp std_msgs)

install(TARGETS face_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_package()
