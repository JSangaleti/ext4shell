#include "ext4.hpp"

void read_superblock(int pos, fstream& iso_file, ext4_super_block& block_out) {
    iso_file.seekg(pos);
    iso_file.read(reinterpret_cast<char*>(&block_out), sizeof(ext4_super_block));
}