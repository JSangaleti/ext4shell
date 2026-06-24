#pragma once

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string>
#include <sstream>

#include "ext4.hpp"

using namespace std;

struct fs_state
    {
        string path = "/";
        uint32_t current_inode = 2;
        uint32_t block_size;
    };

int start_shell(fstream& iso_file);

extern bool DEBUG_MODE; 

#define DEBUG_PRINT(msg) if(DEBUG_MODE) { std::cout << "[DEBUG] " << msg << std::endl; }