#pragma once

#include <iostream>

#include "ext4.hpp"
#include "shell.hpp"

using namespace std;

// [READ]:
void info(const ext4_super_block& super_block, const fs_state& state);

void cat(const string file);

void attr(const string file_dir); 

void cd(const string path, fs_state& state); 

void ls(); 

bool testi(const uint32_t inode_number); 

bool testb(const uint64_t block_number); 

void command_export(const string source_path, const string target_string); 

void pwd(const fs_state& state);

// [WRITE]:

void touch(const string file); 

void mkdir(const string dir); 

void rm(const string file); 

void rmdir(const string dir); 

void rename(const string file, const string new_file_name); 

// [DEBUG]:

void print_superblock(const ext4_super_block& sb);

void print_block(fstream& iso_file, const uint32_t block_number, const uint32_t block_size);