#include "commands.hpp"

void info(const ext4_super_block& super_block, const fs_state& state){
    cout << "--- INFO DO SISTEMA DE ARQUIVOS ---" << endl;
    cout << "Tamanho do Bloco: " << state.block_size << " bytes" << endl;
    cout << "Total de Inodes:  " << super_block.s_inodes_count << endl;
    cout << "Total de Blocos:  " << super_block.s_blocks_count_lo << endl;
    
    cout << "--- ESTADO DO SHELL ---" << endl;
    cout << "Caminho Atual:    " << state.path << endl;
    cout << "Inode Atual:      " << state.current_inode << endl;
}

void pwd(const fs_state& state) {
    cout << state.path << endl;
}

void cat(const string file) { cout << "falta implementar" << endl; }
void attr(const string file_dir) { cout << "falta implementar" << endl; }
void cd(const string path, fs_state& state) { cout << "falta implementar" << endl; }
void ls() { cout << "falta implementar" << endl; }

bool testi(const uint32_t inode_number) { 
    cout << "falta implementar" << endl; 
    return false;
}
bool testb(const uint64_t block_number) { 
    cout << "falta implementar" << endl; 
    return false;
}

void command_export(const string source_path, const string target_string) { cout << "falta implementar" << endl; }
void touch(const string file) { cout << "falta implementar" << endl; }
void mkdir(const string dir) { cout << "falta implementar" << endl; }
void rm(const string file) { cout << "falta implementar" << endl; }
void rmdir(const string dir) { cout << "falta implementar" << endl; }
void rename(const string file, const string new_file_name) { cout << "falta implementar" << endl; }

void print_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num) {
    ext4_inode inode;
    read_inode(iso_file, sb, inode_num, inode);
    
    cout << "\n--- INODE " << inode_num << " ---" << endl;
    cout << "Tamanho: " << inode.i_size_lo << " bytes" << endl;
    cout << "Links:   " << inode.i_links_count << endl;
    cout << "Modo (Hex): 0x" << hex << inode.i_mode << dec << endl; 
    cout << "Bloco 0 (Dados): " << inode.i_block[0] << endl;
}