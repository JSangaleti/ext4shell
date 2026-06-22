#pragma once

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string>

using namespace std;

struct ext4_super_block {
    uint32_t s_inodes_count;
    uint32_t s_log_block_size;
    // terminar de colocar o resto
} __attribute__((packed));

void read_superblock(int pos, fstream& iso_file, ext4_super_block& block_out);
