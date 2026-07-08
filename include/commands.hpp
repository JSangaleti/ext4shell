#pragma once

#include <iostream>
#include <ctime>

#include "ext4.hpp"
#include "shell.hpp"

using namespace std;

// [READ]:
void info(const ext4_super_block& super_block, const fs_state& state);

void cat(const string path, fstream& iso_file, const ext4_super_block& sb, const fs_state& state);

void attr(const string path, fstream& iso_file, const ext4_super_block& sb, const fs_state& state); 

void cd(const string path, fstream& iso_file, const ext4_super_block& sb, fs_state& state); 

void ls(fstream& iso_file, const ext4_super_block& sb, const fs_state& state);

bool testi(const uint32_t inode_number, fstream& iso_file, const ext4_super_block& sb);

bool testb(const uint64_t block_number, fstream& iso_file, const ext4_super_block& sb);

void command_export(const string source_path, const string target_string, fstream& iso_file, const ext4_super_block& sb, const fs_state& state);

void pwd(const fs_state& state);

// [WRITE]:

void touch(const string file, fstream& iso_file, ext4_super_block& sb, fs_state& state);

void mkdir(const string dir, fstream& iso_file, ext4_super_block& sb, fs_state& state);

void rm(const string file); 

void rmdir(const string dir); 

void rename(const string file, const string new_file_name); 

// [DEBUG]:

void print_superblock(const ext4_super_block& sb);

void print_block(fstream& iso_file, const uint32_t block_number, const uint32_t block_size);

void print_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num);