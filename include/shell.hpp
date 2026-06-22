#pragma once

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string>
#include <sstream>

using namespace std;

int start_shell(fstream& iso_file);

extern bool DEBUG_MODE; 

#define DEBUG_PRINT(msg) if(DEBUG_MODE) { std::cout << "[DEBUG] " << msg << std::endl; }