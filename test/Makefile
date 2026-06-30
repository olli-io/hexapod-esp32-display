# Host-side unit tests (replaces the old PlatformIO `native` env).
# Run from this directory: make            (build + run all suites)
#                          make build      (build only)
#                          make clean

CC       ?= gcc
CXX      ?= g++
CXXFLAGS ?= -std=gnu++17 -O1 -g -Wall -Wextra
INCLUDES := -I../components/protocol/include -Iunity

BUILD := build

# u8g2 C core, needed by the face panel test. Compiled once into objects.
U8G2_DIR  := ../components/u8g2/u8g2/csrc
U8G2_SRCS := $(wildcard $(U8G2_DIR)/*.c)
U8G2_OBJS := $(patsubst $(U8G2_DIR)/%.c,$(BUILD)/u8g2/%.o,$(U8G2_SRCS))

FACE_INC := -I../ros2/face/src -I$(U8G2_DIR) \
            -I../components/renderer/include -I../components/expression/include \
            -I../components/config/include

SUITES := $(BUILD)/test_codec $(BUILD)/test_crc $(BUILD)/test_eyes $(BUILD)/test_face

.PHONY: all build clean sim
all: build
	@set -e; for t in $(SUITES); do echo "== $$t"; ./$$t; done

build: $(SUITES)

$(BUILD)/unity.o: unity/unity.c unity/unity.h unity/unity_internals.h
	@mkdir -p $(BUILD)
	$(CC) -Iunity -O1 -g -c $< -o $@

$(BUILD)/test_codec: test_codec/test_codec.cpp ../components/protocol/ProtocolCodec.cpp $(BUILD)/unity.o
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

$(BUILD)/test_crc: test_crc/test_crc.cpp $(BUILD)/unity.o
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

$(BUILD)/test_eyes: test_eyes/test_eyes.cpp ../components/renderer/EyeRaster.cpp $(BUILD)/unity.o
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
	    -I../components/renderer/include -I../components/expression/include \
	    $^ -o $@

$(BUILD)/u8g2/%.o: $(U8G2_DIR)/%.c
	@mkdir -p $(BUILD)/u8g2
	$(CC) -I$(U8G2_DIR) -O1 -c $< -o $@

# Pi face: name parsing + dirty-flush + reused EyeAnim/EyeRaster, headless.
$(BUILD)/test_face: test_face/test_face.cpp \
        ../ros2/face/src/Sh1122Panel.cpp \
        ../components/renderer/EyeAnim.cpp ../components/renderer/EyeRaster.cpp \
        ../components/expression/ExpressionController.cpp \
        $(U8G2_OBJS) $(BUILD)/unity.o
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Iunity $(FACE_INC) $^ -o $@

# Interactive terminal eye simulator (not a test suite): make sim
sim: $(BUILD)/eyesim
	./$(BUILD)/eyesim

$(BUILD)/eyesim: sim/eyesim.cpp ../components/renderer/EyeRaster.cpp ../components/renderer/EyeAnim.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) \
	    -I../components/renderer/include -I../components/expression/include \
	    $^ -o $@

clean:
	rm -rf $(BUILD)
