# Compilador e flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude -Ithird-party/checksum
LDFLAGS = -lcryptopp

SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
PROF_SRC = third-party/checksum/ext4checksum.cc

OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
PROF_OBJ = $(BUILD_DIR)/ext4checksum.o

TARGET = ext4shell

all: $(TARGET)

$(TARGET): $(OBJS) $(PROF_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/ext4checksum.o: $(PROF_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean