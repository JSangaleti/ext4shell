#include "ext4.hpp"

void read_superblock(fstream& iso_file, ext4_super_block& block_out, int pos) {
    iso_file.seekg(pos);
    iso_file.read(reinterpret_cast<char*>(&block_out), sizeof(ext4_super_block));
}

void read_block(std::fstream& iso_file, uint32_t block_number, uint32_t block_size, char* buffer) {
    uint64_t offset = static_cast<uint64_t>(block_number) * block_size;
    
    iso_file.seekg(offset);
    
    iso_file.read(buffer, block_size);
}

void write_block(fstream& iso_file, uint32_t block_number, uint32_t block_size, const char* buffer) {
    uint64_t offset = static_cast<uint64_t>(block_number) * block_size;
    
    iso_file.seekp(offset);
    
    iso_file.write(buffer, block_size);
}

void read_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num, ext4_inode& inode_out) {
    // 0. Limpa estado de erro anterior (importante caso operações anteriores tenham falhado)
    iso_file.clear();
    
    // Zera a struct para evitar leitura de lixo da memória caso a leitura falhe
    memset(&inode_out, 0, sizeof(ext4_inode));

    uint32_t block_size = 1024 << sb.s_log_block_size;
    
    // 1. Descobrir a qual grupo esse inode pertence e seu índice local
    uint32_t group_num = (inode_num - 1) / sb.s_inodes_per_group;
    uint32_t local_inode_idx = (inode_num - 1) % sb.s_inodes_per_group;

    // 2. Achar a Tabela de Descritores (BGD)
    // A BGD sempre fica no bloco imediatamente após o superbloco
    uint32_t bgd_block = sb.s_first_data_block + 1;
    
    // Identifica se o descritor tem 64 bytes (flag 64-bit) ou o padrão de 32 bytes
    uint32_t desc_size = (sb.s_feature_incompat & 0x80) ? 64 : 32;
    
    uint64_t bgd_offset = (static_cast<uint64_t>(bgd_block) * block_size) + (group_num * desc_size);

    ext4_group_desc bgd;
    iso_file.seekg(bgd_offset);
    iso_file.read(reinterpret_cast<char*>(&bgd), sizeof(ext4_group_desc));

    // 3. Achar o Inode dentro da tabela desse grupo
    uint64_t inode_table_offset = static_cast<uint64_t>(bgd.bg_inode_table_lo) * block_size;
    
    // sb.s_inode_size é 128 ou 256 bytes
    uint64_t exact_inode_offset = inode_table_offset + (local_inode_idx * sb.s_inode_size);

    iso_file.seekg(exact_inode_offset);

    iso_file.read(reinterpret_cast<char*>(&inode_out), sizeof(ext4_inode));
}

uint64_t get_physical_block(const ext4_inode& inode, uint32_t logical_block) {
    // 1. Lemos os primeiros 12 bytes do array i_block como o cabeçalho da árvore.
    // O reinterpret_cast pega os bytes puros e trata como a struct ext4_extent_header.
    const ext4_extent_header* header = reinterpret_cast<const ext4_extent_header*>(inode.i_block);
    
    // 2. 0xF30A é a assinatura mágica obrigatória de uma árvore de extents válida.
    if (header->eh_magic != 0xF30A) return 0; 

    // 3. Calculamos onde começam os nós da árvore.
    // Pegamos o endereço base do i_block, tratamos como char (byte a byte) para somar 12 bytes (tamanho do cabeçalho) e trata como a struct ext4_extent.
    const ext4_extent* extent = reinterpret_cast<const ext4_extent*>(
        reinterpret_cast<const char*>(inode.i_block) + sizeof(ext4_extent_header)
    );

    // 4. Varremos todos os extents válidos (eh_entries) registrados no cabeçalho.
    for (int i = 0; i < header->eh_entries; ++i) {
        
        // Checamos se o bloco lógico procurado pertence ao intervalo coberto por este extent.
        if (logical_block >= extent[i].ee_block && logical_block < (extent[i].ee_block + extent[i].ee_len)) {
            
            // 5. O endereço físico no disco é de 48 bits, dividido em parte alta (16 bits) e baixa (32 bits).
            // O static_cast garante que o ee_start_hi não tenha um overflow ao ser empurrado 32 casas para a esquerda.
            // O operador bitwise OR (|) junta as duas partes num único número de 64 bits.
            uint64_t physical_block = (static_cast<uint64_t>(extent[i].ee_start_hi) << 32) | extent[i].ee_start_lo;
            
            // 6. Retornamos o bloco físico base + a diferença (offset) de onde o bloco lógico está dentro deste extent.
            return physical_block + (logical_block - extent[i].ee_block);
        }
    }
    
    // Retorna 0 se o bloco procurado não existir
    return 0;
}

vector<FileEntry> search_filedir(fstream& iso_file, const ext4_super_block& sb, uint32_t dir_inode_num, const string& target_name) {
    iso_file.clear(); // Garante que o stream esteja limpo para novas leituras
    
    vector<FileEntry> entries;
    
    // Obtém o Inode do diretório atual para acessar sua árvore de blocos
    ext4_inode dir_inode;
    read_inode(iso_file, sb, dir_inode_num, dir_inode);

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
                string current_name(name);

                // Se não buscou por nome específico, OU se o nome bater com a busca
                if (target_name.empty() || current_name == target_name) {
                    entries.push_back({entry.inode, entry.file_type, current_name});
                    
                    // Se achou o alvo específico, pode parar de ler o disco
                    if (!target_name.empty()) return entries; 
                }
            }
            // Pula para o próximo registro
            bytes_read += entry.rec_len;
        }
    }
    return entries;
}

