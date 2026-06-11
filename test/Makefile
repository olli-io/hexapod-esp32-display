# Host-side unit tests (replaces the old PlatformIO `native` env).
# Run from this directory: make            (build + run all suites)
#                          make build      (build only)
#                          make clean

CC       ?= gcc
CXX      ?= g++
CXXFLAGS ?= -std=gnu++17 -O1 -g -Wall -Wextra
INCLUDES := -I../components/protocol/include -Iunity

BUILD := build

SUITES := $(BUILD)/test_codec $(BUILD)/test_crc $(BUILD)/test_eyes

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
