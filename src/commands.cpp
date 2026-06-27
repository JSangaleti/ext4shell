#include "commands.hpp"

// --- READ ---

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

void cd(const string path, fstream& iso_file, const ext4_super_block& sb, fs_state& state) {
    
    // Atalho para voltar direto para a raiz (Inode 2 é o padrão do ext4 para '/')
    if (path == "/") {
        state.current_inode = 2;
        state.path = "/";
        return;
    }
    
    // Se o usuário digitar "cd .", o diretório não muda
    if (path == ".") {
        return;
    }

    auto entries = search_filedir(iso_file, sb, state.current_inode, path);

    if (entries.empty()) {
        cout << "cd: " << path << ": Arquivo ou diretorio nao encontrado" << endl;
        return;
    }
    
    // Pega o primeiro (e único) resultado da busca para acessar os dados
    auto entry = entries[0];
                
    // Confirma se o destino é um diretório (tipo 2 = dir)
    if (entry.file_type == 2) {
        
        // 1. Lógica para atualizar a string do path
        if (path == "..") {
            // Ao subir de nível, removemos o último segmento do caminho
            if (state.path != "/") {
                size_t last_slash = state.path.find_last_of("/");
                if (last_slash == 0) state.path = "/";
                else state.path = state.path.substr(0, last_slash);
            }
        } else {
            // Ao entrar em uma pasta, concatenamos o nome ao caminho atual
            if (state.path == "/") state.path = "/" + path;
            else state.path = state.path + "/" + path;
        }

        // 2. Atualiza o Inode para o novo diretório
        state.current_inode = entry.inode;
        return; 
    } else {
        cout << "cd: " << path << ": Nao e um diretorio" << endl;
        return;
    }
}

void ls(fstream& iso_file, const ext4_super_block& sb, const fs_state& state) {

   auto entries = search_filedir(iso_file, sb, state.current_inode);
         
   for (const auto& entry : entries) {

        // Formatação: Coloca uma barra '/' se for diretório
        if (entry.file_type == 2) {
            cout << entry.name << "/  ";
        } else {
            cout << entry.name << "  ";
        }
    }

    cout << endl;
}

bool testi(const uint32_t inode_number) { 
    cout << "falta implementar" << endl; 
    return false;
}
bool testb(const uint64_t block_number) { 
    cout << "falta implementar" << endl; 
    return false;
}

void command_export(const string source_path, const string target_string) { cout << "falta implementar" << endl; }

// --- WRITE ---

void touch(const string file) { cout << "falta implementar" << endl; }

void mkdir(const string dir) { cout << "falta implementar" << endl; }

void rm(const string file) { cout << "falta implementar" << endl; }

void rmdir(const string dir) { cout << "falta implementar" << endl; }

void rename(const string file, const string new_file_name) { cout << "falta implementar" << endl; }

// --- DEBUG ---

void print_superblock(const ext4_super_block& sb) {
    cout << "\n--- RAW DUMP: SUPERBLOCO ---" << endl;
    cout << "s_inodes_count:      " << sb.s_inodes_count << endl;
    cout << "s_blocks_count_lo:   " << sb.s_blocks_count_lo << endl;
    cout << "s_r_blocks_count_lo: " << sb.s_r_blocks_count_lo << endl;
    cout << "s_free_blocks_count: " << sb.s_free_blocks_count_lo << endl;
    cout << "s_free_inodes_count: " << sb.s_free_inodes_count << endl;
    cout << "s_first_data_block:  " << sb.s_first_data_block << endl;
    cout << "s_log_block_size:    " << sb.s_log_block_size << endl;
    cout << "----------------------------\n" << endl;
}

void print_block(fstream& iso_file, uint32_t block_number, uint32_t block_size) {
    cout << "\n--- RAW DUMP: BLOCO " << block_number << " ---" << endl;
    
    char* buffer = new char[block_size];
    read_block(iso_file, block_number, block_size, buffer);
    
    int limit = (block_size < 64) ? block_size : 64; // Mostra só os primeiros 64 bytes
    
    for(int i = 0; i < limit; i++) {
        printf("%02X ", (unsigned char)buffer[i]);
        if ((i + 1) % 16 == 0) cout << endl; // Quebra a linha a cada 16 bytes
    }
    cout << "\n----------------------------\n" << endl;
    
    delete[] buffer;
}

void print_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num) {
    ext4_inode inode;
    read_inode(iso_file, sb, inode_num, inode);
    
    cout << "\n--- INODE " << inode_num << " ---" << endl;
    cout << "Tamanho: " << inode.i_size_lo << " bytes" << endl;
    cout << "Links:   " << inode.i_links_count << endl;
    cout << "Modo (Hex): 0x" << hex << inode.i_mode << dec << endl; 
    uint64_t bloco_fisico = get_physical_block(inode, 0);
    cout << "Bloco Fisico (Dados): " << bloco_fisico << endl;
}