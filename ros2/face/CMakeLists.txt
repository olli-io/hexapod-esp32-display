cmake_minimum_required(VERSION 3.16)
project(hexapod_face CXX C)

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

# Root of the firmware repo: this package lives at <repo>/ros2/face.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Vendored u8g2 C core (framework-independent; same source the firmware uses),
# compiled into its own library so --gc-sections can drop the unused fonts.
file(GLOB U8G2_SRCS "${REPO_ROOT}/components/u8g2/u8g2/csrc/*.c")
add_library(u8g2_core STATIC ${U8G2_SRCS})
target_include_directories(u8g2_core PUBLIC "${REPO_ROOT}/components/u8g2/u8g2/csrc")

# Platform-free pieces reused verbatim from the firmware tree:
#   EyeAnim/EyeRaster  - the animation + rasterizer (also host-tested)
#   ExpressionController.cpp - only for expressionName()/gazeName(), the single
#                              source of truth for the on-wire enum names
set(CORE_SRCS
  "${REPO_ROOT}/components/renderer/EyeAnim.cpp"
  "${REPO_ROOT}/components/renderer/EyeRaster.cpp"
  "${REPO_ROOT}/components/expression/ExpressionController.cpp"
)

add_executable(face_node
  src/face_node.cpp
  src/Sh1122Panel.cpp
  ${CORE_SRCS}
)

target_include_directories(face_node PRIVATE
  src
  "${REPO_ROOT}/components/renderer/include"
  "${REPO_ROOT}/components/expression/include"
  "${REPO_ROOT}/components/config/include"
)

target_link_libraries(face_node u8g2_core)
ament_target_dependencies(face_node rclcpp std_msgs)

install(TARGETS face_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_package()
