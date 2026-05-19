CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude -Ithird_party/checksum
LDFLAGS = -lcryptopp

SRC = src/main.cpp src/shell.cpp src/image.cpp src/ext4.cpp src/commands.cpp third_party/checksum/ext4checksum.cc
TARGET = build/ext4shell

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf build