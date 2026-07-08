#include "commands.hpp"

static constexpr uint32_t EXT4_EXTENTS_FL = 0x00080000;

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

void touch(const string file, fstream& iso_file, ext4_super_block& sb, fs_state& state) {
    // 0. Verifica se o arquivo já existe no diretório atual
    auto existing = search_filedir(iso_file, sb, state.current_inode, file);
    if (!existing.empty()) {
        cout << "touch: Impossivel criar '" << file << "': Arquivo ou diretorio ja existe" << endl;
        return;
    }

    // 1. Aloca um inode livre
    uint32_t new_inode_num = allocate_inode(iso_file, sb);
    if (new_inode_num == 0) { 
        cout << "touch: Erro - Disco cheio ou sem inodes livres." << endl; 
        return; 
    }

    // 2. Monta a estrutura do novo arquivo (vazio)
    ext4_inode new_inode{};
    new_inode.i_mode = 0x81A4; // 0x8000 (Arquivo Regular) + 0644 (Permissões rw-r--r--)
    new_inode.i_size_lo = 0;
    new_inode.i_links_count = 1;
    new_inode.i_flags = EXT4_EXTENTS_FL;
    new_inode.i_ctime = new_inode.i_atime = new_inode.i_mtime = time(nullptr);
    
    // Arquivo vazio: possui cabecalho de extents valido, mas sem extents alocados.
    ext4_extent_header eh{};
    eh.eh_magic = 0xF30A;
    eh.eh_entries = 0;
    eh.eh_max = 4;
    eh.eh_depth = 0;
    eh.eh_generation = 0;
    memcpy(new_inode.i_block, &eh, sizeof(eh));

    // 3. Escreve o Inode no disco
    write_inode(iso_file, sb, new_inode_num, new_inode);

    // 4. Ligar ao diretório atual
    // 4. Ligar ao diretório atual (1 = Arquivo Regular)
    bool success = add_dir_entry(iso_file, sb, state.current_inode, new_inode_num, file, 1);
    
    if (success) {
        cout << "Arquivo criado com sucesso: " << file << " (Inode: " << new_inode_num << ")" << endl;
    }
}

void mkdir(const string dir, fstream& iso_file, ext4_super_block& sb, fs_state& state) {
    // 0. Verifica se o nome já existe
    auto existing = search_filedir(iso_file, sb, state.current_inode, dir);
    if (!existing.empty()) {
        cout << "mkdir: Impossivel criar o diretorio '" << dir << "': Arquivo ou diretorio ja existe" << endl;
        return;
    }

    uint32_t block_size = 1024 << sb.s_log_block_size;

    // 1. Aloca um inode e um bloco de dados
    uint32_t new_inode_num = allocate_inode(iso_file, sb);
    uint64_t new_block_num = allocate_block(iso_file, sb);

    if (new_inode_num == 0 || new_block_num == 0) {
        cout << "mkdir: Erro - Disco cheio (inode ou block)." << endl;
        return;
    }

    // 2. Monta o novo Inode do diretório
    ext4_inode new_inode{};
    new_inode.i_mode = 0x41ED; // 0x4000 (Diretório) + 0755 (Permissões rwxr-xr-x)
    new_inode.i_size_lo = block_size; // Um diretório começa ocupando 1 bloco
    new_inode.i_links_count = 2; // '.' e a si mesmo
    new_inode.i_blocks_lo = block_size / 512; // Número de setores (blocos de 512b)
    new_inode.i_flags = EXT4_EXTENTS_FL;
    new_inode.i_ctime = new_inode.i_atime = new_inode.i_mtime = time(nullptr);
    
    // Criamos as structs locais e depois copiamos para o array i_block
    struct ext4_extent_header {
        uint16_t eh_magic; uint16_t eh_entries; uint16_t eh_max;
        uint16_t eh_depth; uint32_t eh_generation;
    } eh;
    eh.eh_magic = 0xF30A;
    eh.eh_entries = 1;
    eh.eh_max = 4;
    eh.eh_depth = 0;
    eh.eh_generation = 0;
    memcpy(&new_inode.i_block[0], &eh, sizeof(ext4_extent_header));

    struct ext4_extent {
        uint32_t ee_block; uint16_t ee_len;
        uint16_t ee_start_hi; uint32_t ee_start_lo;
    } ex;
    ex.ee_block = 0; 
    ex.ee_len = 1;
    ex.ee_start_hi = (new_block_num >> 32) & 0xFFFF;
    ex.ee_start_lo = new_block_num & 0xFFFFFFFF;
    memcpy(&new_inode.i_block[3], &ex, sizeof(ext4_extent));

    // 3. Inicializa o novo bloco com '.' e '..'
    vector<char> dir_buffer(block_size, 0);
    uint32_t dir_tail_size = ext4_has_metadata_csum(sb) ? 12 : 0;
    
    // Entrada '.'
    ext4_dir_entry_2* dot = reinterpret_cast<ext4_dir_entry_2*>(dir_buffer.data());
    dot->inode = new_inode_num;
    dot->rec_len = 12; // 8 bytes fixos + 1 char (arredondado pra 4)
    dot->name_len = 1;
    dot->file_type = 2; // Diretório
    dot->name[0] = '.';

    // Entrada '..' (usa todo o resto do bloco)
    ext4_dir_entry_2* dotdot = reinterpret_cast<ext4_dir_entry_2*>(dir_buffer.data() + dot->rec_len);
    dotdot->inode = state.current_inode;
    dotdot->rec_len = block_size - dot->rec_len - dir_tail_size;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.'; dotdot->name[1] = '.';

    update_dir_block_checksum(iso_file, sb, new_inode_num, new_inode, dir_buffer.data(), block_size);

    // Grava o bloco do novo diretório no disco
    iso_file.seekp(new_block_num * block_size);
    iso_file.write(dir_buffer.data(), block_size);

    // 4. Salva o Inode no disco e adiciona no diretório atual
    write_inode(iso_file, sb, new_inode_num, new_inode);
    bool success = add_dir_entry(iso_file, sb, state.current_inode, new_inode_num, dir, 2); // 2 = Directory
    
    if (success) {
        update_group_used_dirs_count(iso_file, sb, new_inode_num, 1);

        // Incrementa o contador de links do diretório pai (por causa do '..')
        ext4_inode parent_inode;
        read_inode(iso_file, sb, state.current_inode, parent_inode);
        parent_inode.i_links_count++;
        write_inode(iso_file, sb, state.current_inode, parent_inode);

        cout << "Diretorio criado com sucesso: " << dir << endl;
    }
}

void rm(const string file, fstream& iso_file, ext4_super_block& sb, fs_state& state) {
    auto entries = search_filedir(iso_file, sb, state.current_inode, file);
    if (entries.empty()) {
        cout << "rm: Impossivel remover '" << file << "': Arquivo nao encontrado" << endl;
        return;
    }

    if (entries[0].file_type == 2) {
        cout << "rm: Impossivel remover '" << file << "': E um diretorio" << endl;
        return;
    }

    // Libera o inode
    free_inode(iso_file, sb, entries[0].inode);

    // Remove do diretório pai
    if (remove_dir_entry(iso_file, sb, state.current_inode, file)) {
        cout << "Arquivo removido: " << file << endl;
    }
}

void rmdir(const string dir, fstream& iso_file, ext4_super_block& sb, fs_state& state) {
    // Proíbe tentar apagar atalhos de navegação
    if (dir == "." || dir == "..") {
        cout << "rmdir: Impossivel remover '" << dir << "': Argumento invalido" << endl;
        return;
    }

    auto entries = search_filedir(iso_file, sb, state.current_inode, dir);
    if (entries.empty()) {
        cout << "rmdir: Impossivel remover '" << dir << "': Arquivo ou diretorio nao encontrado" << endl;
        return;
    }

    if (entries[0].file_type != 2) {
        cout << "rmdir: Impossivel remover '" << dir << "': Nao e um diretorio" << endl;
        return;
    }

    uint32_t target_inode_num = entries[0].inode;

    // Só remove se o diretório estiver vazio
    if (!is_dir_empty(iso_file, sb, target_inode_num)) {
        cout << "rmdir: Impossivel remover '" << dir << "': O diretorio nao esta vazio" << endl;
        return;
    }

    // 1. Encontra e libera todos os blocos de dados ocupados pelo diretório
    ext4_inode target_inode;
    read_inode(iso_file, sb, target_inode_num, target_inode);
    uint32_t logical_block = 0;
    while (true) {
        uint64_t phys_block = get_physical_block(target_inode, logical_block);
        if (phys_block == 0) break; // Acabaram os blocos
        free_block(iso_file, sb, phys_block);
        logical_block++;
    }

    // 2. Libera o Inode do diretório
    free_inode(iso_file, sb, target_inode_num);
    update_group_used_dirs_count(iso_file, sb, target_inode_num, -1);

    // 3. Remove a entrada do diretório pai
    if (remove_dir_entry(iso_file, sb, state.current_inode, dir)) {
        
        // 4. Subtrai o link count do pai (pois o diretório filho continha um '..' apontando pra cá)
        ext4_inode parent_inode;
        read_inode(iso_file, sb, state.current_inode, parent_inode);
        if (parent_inode.i_links_count > 0) {
            parent_inode.i_links_count--;
        }
        write_inode(iso_file, sb, state.current_inode, parent_inode);

        cout << "Diretorio removido: " << dir << endl;
    }
}

void rename(const string file, const string new_file_name, fstream& iso_file, ext4_super_block& sb, fs_state& state) {
    if (file == "." || file == "..") {
        cout << "rename: Impossivel renomear atalhos de sistema." << endl;
        return;
    }

    // 1. Verifica se o arquivo de origem existe
    auto src_entries = search_filedir(iso_file, sb, state.current_inode, file);
    if (src_entries.empty()) {
        cout << "rename: '" << file << "' nao encontrado." << endl;
        return;
    }

    // 2. Verifica se o novo nome já está em uso
    auto dest_entries = search_filedir(iso_file, sb, state.current_inode, new_file_name);
    if (!dest_entries.empty()) {
        cout << "rename: Impossivel renomear. O nome '" << new_file_name << "' ja existe." << endl;
        return;
    }

    uint32_t target_inode = src_entries[0].inode;
    uint8_t target_type = src_entries[0].file_type;

    // 3. Apaga a entrada antiga da pasta
    if (remove_dir_entry(iso_file, sb, state.current_inode, file)) {
        
        // 4. Cria a nova entrada apontando para os mesmos dados
        if (add_dir_entry(iso_file, sb, state.current_inode, target_inode, new_file_name, target_type)) {
            cout << "Sucesso: '" << file << "' renomeado para '" << new_file_name << "'" << endl;
        } else {
            cout << "Erro ao alocar novo nome no diretorio." << endl;
        }
    } else {
        cout << "rename: Erro ao remover a entrada antiga." << endl;
    }
}

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
