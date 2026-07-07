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

void cat(const string path, fstream& iso_file, const ext4_super_block& sb, const fs_state& state) {
    // Busca o arquivo no diretório atual
    auto entries = search_filedir(iso_file, sb, state.current_inode, path);
    if (entries.empty()) {
        cout << "cat: " << path << ": Arquivo nao encontrado\n";
        return;
    }

    auto entry = entries[0];
    if (entry.file_type != 1) { // Garante que é um arquivo regular (1) e não diretório (2)
        cout << "cat: " << path << ": E um diretorio\n";
        return;
    }

    // Lê o Inode do arquivo para saber o tamanho exato
    ext4_inode file_inode;
    read_inode(iso_file, sb, entry.inode, file_inode);

    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t remaining_bytes = file_inode.i_size_lo;
    uint32_t logical_block = 0;

    // Buffer para carregar os blocos de texto
    vector<char> buffer(block_size);

    // Lê bloco por bloco até acabar o tamanho do arquivo
    while (remaining_bytes > 0) {
        uint64_t phys_block = get_physical_block(file_inode, logical_block);
        
        // Lê apenas o que falta (evita imprimir lixo da memória no último bloco)
        uint32_t bytes_to_read = (remaining_bytes < block_size) ? remaining_bytes : block_size;

        if (phys_block != 0) {
            iso_file.seekg(phys_block * block_size);
            iso_file.read(buffer.data(), bytes_to_read);
            cout.write(buffer.data(), bytes_to_read);
        }

        remaining_bytes -= bytes_to_read;
        logical_block++;
    }
    cout << endl;
}

void attr(const string path, fstream& iso_file, const ext4_super_block& sb, const fs_state& state) {
    // 1. Busca o arquivo
    auto entries = search_filedir(iso_file, sb, state.current_inode, path);
    if (entries.empty()) {
        cout << "attr: " << path << ": Arquivo ou diretorio nao encontrado" << endl;
        return;
    }

    // 2. Lê o Inode
    ext4_inode inode;
    read_inode(iso_file, sb, entries[0].inode, inode);

    // 3. Exibe os atributos
    cout << "--- ATRIBUTOS DE: " << path << " ---" << endl;
    cout << "Inode:      " << entries[0].inode << endl;
    cout << "Tamanho:    " << inode.i_size_lo << " bytes" << endl;
    cout << "Modo:       0" << oct << inode.i_mode << dec << endl; // Exibe em octal
    cout << "UID:        " << inode.i_uid << endl;
    cout << "GID:        " << inode.i_gid << endl;
    cout << "Links:      " << inode.i_links_count << endl;
    
    // Converte os tempos
    time_t mtime = inode.i_mtime;
    cout << "Modificado: " << ctime(&mtime);
    
    cout << "-------------------------------" << endl;
}

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

bool testi(uint32_t inode_number, fstream& iso_file, const ext4_super_block& sb) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t group_num = (inode_number - 1) / sb.s_inodes_per_group;
    uint32_t local_idx = (inode_number - 1) % sb.s_inodes_per_group;

    // Busca o descritor do grupo
    ext4_group_desc bgd;
    uint32_t bgd_block = sb.s_first_data_block + 1;
    uint32_t desc_size = (sb.s_feature_incompat & 0x80) ? 64 : 32;
    iso_file.seekg((uint64_t)bgd_block * block_size + (group_num * desc_size));
    iso_file.read(reinterpret_cast<char*>(&bgd), sizeof(ext4_group_desc));

    // Lê o bitmap de inodes
    vector<char> bitmap(block_size);
    iso_file.seekg((uint64_t)bgd.bg_inode_bitmap_lo * block_size);
    iso_file.read(bitmap.data(), block_size);

    return check_bit(bitmap.data(), local_idx);
}

bool testb(uint64_t block_number, fstream& iso_file, const ext4_super_block& sb) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t group_num = block_number / sb.s_blocks_per_group;
    uint32_t local_idx = block_number % sb.s_blocks_per_group;

    ext4_group_desc bgd;
    uint32_t bgd_block = sb.s_first_data_block + 1;
    uint32_t desc_size = (sb.s_feature_incompat & 0x80) ? 64 : 32;
    iso_file.seekg((uint64_t)bgd_block * block_size + (group_num * desc_size));
    iso_file.read(reinterpret_cast<char*>(&bgd), sizeof(ext4_group_desc));

    vector<char> bitmap(block_size);
    iso_file.seekg((uint64_t)bgd.bg_block_bitmap_lo * block_size);
    iso_file.read(bitmap.data(), block_size);

    return check_bit(bitmap.data(), local_idx);
}

void command_export(const string source_path, const string target_path, fstream& iso_file, const ext4_super_block& sb, const fs_state& state) {
    // 1. Busca o arquivo dentro da imagem
    auto entries = search_filedir(iso_file, sb, state.current_inode, source_path);
    if (entries.empty()) {
        cout << "export: " << source_path << ": Arquivo nao encontrado" << endl;
        return;
    }

    ext4_inode file_inode;
    read_inode(iso_file, sb, entries[0].inode, file_inode);

    // 2. Abre o arquivo de destino
    ofstream target_file(target_path, ios::binary);
    if (!target_file.is_open()) {
        cout << "export: Erro ao criar arquivo de destino: " << target_path << endl;
        return;
    }

    // 3. Lê bloco por bloco da imagem e escreve no arquivo real
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t remaining_bytes = file_inode.i_size_lo;
    uint32_t logical_block = 0;
    vector<char> buffer(block_size);

    while (remaining_bytes > 0) {
        uint64_t phys_block = get_physical_block(file_inode, logical_block);
        uint32_t bytes_to_read = (remaining_bytes < block_size) ? remaining_bytes : block_size;

        if (phys_block != 0) {
            iso_file.seekg(phys_block * block_size);
            iso_file.read(buffer.data(), bytes_to_read);
            target_file.write(buffer.data(), bytes_to_read);
        }

        remaining_bytes -= bytes_to_read;
        logical_block++;
    }

    cout << "Sucesso: Arquivo exportado para " << target_path << endl;
}

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
    iso_file.clear();
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
    iso_file.clear();
    ext4_inode inode;
    read_inode(iso_file, sb, inode_num, inode);
    
    cout << "\n--- INODE " << inode_num << " ---" << endl;
    cout << "Tamanho: " << inode.i_size_lo << " bytes" << endl;
    cout << "Links:   " << inode.i_links_count << endl;
    cout << "Modo (Hex): 0x" << hex << inode.i_mode << dec << endl; 
    uint64_t bloco_fisico = get_physical_block(inode, 0);
    cout << "Bloco Fisico (Dados): " << bloco_fisico << endl;
}