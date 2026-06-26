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

void cd(const string path, fstream& iso_file, const ext4_super_block& sb, fs_state& state) {
    iso_file.clear(); // Garante que o stream esteja limpo para novas leituras
    
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

    // Lê os metadados do diretório atual para saber onde seus blocos de dados estão
    ext4_inode dir_inode;
    read_inode(iso_file, sb, state.current_inode, dir_inode);

    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t num_blocks = dir_inode.i_size_lo / block_size;
    if (dir_inode.i_size_lo % block_size != 0) { 
        num_blocks++;
    }

    // Varre os blocos físicos do diretório atual para encontrar a entrada do diretório destino
    for (uint32_t logical_block = 0; logical_block < num_blocks; ++logical_block) {
        uint64_t phys_block = get_physical_block(dir_inode, logical_block);
        if (phys_block == 0) continue;

        uint64_t offset = phys_block * block_size;
        uint32_t bytes_read = 0;

        // O bloco de diretório é uma lista sequencial de structs 'ext4_dir_entry_2'.
        // Iteramos sobre o bloco usando a variável 'bytes_read' até chegar ao seu fim.
        while (bytes_read < block_size) {
            ext4_dir_entry_2 entry;
            iso_file.seekg(offset + bytes_read);
            iso_file.read(reinterpret_cast<char*>(&entry), sizeof(ext4_dir_entry_2));

            // Se o tamanho da entrada for 0, chegamos ao final do bloco
            if (entry.rec_len == 0) break;

            // Se o número do Inode for != 0, o registro contém um arquivo/diretório válido
            if (entry.inode != 0) {
                char name[256] = {0};
                iso_file.read(name, entry.name_len); // O nome está logo após a struct no disco
                
                // Comparação do nome da entrada com o caminho solicitado
                if (string(name) == path) {
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
            }
            
            // Pula para o próximo registro
            bytes_read += entry.rec_len;
        }
    }
    
    // Se percorreu todos os blocos e não achou a entrada, o diretório não existe
    cout << "cd: " << path << ": Arquivo ou diretorio nao encontrado" << endl;
}

void ls(fstream& iso_file, const ext4_super_block& sb, const fs_state& state) {
    iso_file.clear(); // Garante que o stream esteja limpo para novas leituras

    // Obtém o Inode do diretório atual para acessar sua árvore de blocos
    ext4_inode dir_inode;
    read_inode(iso_file, sb, state.current_inode, dir_inode);

    uint32_t block_size = 1024 << sb.s_log_block_size;
    
    // Calcula quantos blocos lógicos esse diretório usa
    uint32_t num_blocks = dir_inode.i_size_lo / block_size;
    if (dir_inode.i_size_lo % block_size != 0) num_blocks++;

    // Varre todos os blocos de dados do diretório
    for (uint32_t logical_block = 0; logical_block < num_blocks; ++logical_block) {
        // Traduz o bloco lógico para o endereço físico no disco usando a árvore de extents
        uint64_t phys_block = get_physical_block(dir_inode, logical_block);
        if (phys_block == 0) continue;

        uint64_t offset = phys_block * block_size;
        uint32_t bytes_read = 0;

        // Varre os registros dentro do bloco
        while (bytes_read < block_size) {
            ext4_dir_entry_2 entry;
            iso_file.seekg(offset + bytes_read);
            iso_file.read(reinterpret_cast<char*>(&entry), sizeof(ext4_dir_entry_2));

            // Se rec_len for 0, o bloco acabou ou está corrompido
            if (entry.rec_len == 0) break;

            // Inode 0 indica uma entrada que foi apagada (rm)
            if (entry.inode != 0) {
                // Lê o nome do arquivo, que fica logo após a struct
                char name[256] = {0};
                iso_file.read(name, entry.name_len);
                
                // Formatação: Coloca uma barra '/' se for diretório
                if (entry.file_type == 2) {
                    cout << name << "/  ";
                } else {
                    cout << name << "  ";
                }
            }
            // Pula para o próximo registro
            bytes_read += entry.rec_len;
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
    uint64_t bloco_fisico = get_physical_block(inode, 0);
    cout << "Bloco Fisico (Dados): " << bloco_fisico << endl;
}