#include "ext4.hpp"
#include "ext4checksum.h"

static constexpr uint32_t EXT4_FEATURE_RO_COMPAT_METADATA_CSUM = 0x0400;
static constexpr uint32_t EXT4_SUPERBLOCK_OFFSET = 1024;
static constexpr uint32_t EXT4_SUPERBLOCK_UUID_OFFSET = 104;
static constexpr uint16_t EXT4_DIR_ENTRY_TAIL_REC_LEN = 12;
static constexpr uint8_t EXT4_FT_DIR_CSUM = 0xDE;
static constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT = 0x0080;
static constexpr uint32_t EXT4_GROUP_DESC_SIZE_64BIT = 64;
static constexpr uint32_t EXT4_GROUP_DESC_SIZE_LEGACY = 32;
static constexpr uint32_t EXT4_GD_BLOCK_BITMAP_CSUM_LO_OFFSET = 0x18;
static constexpr uint32_t EXT4_GD_INODE_BITMAP_CSUM_LO_OFFSET = 0x1A;
static constexpr uint32_t EXT4_GD_ITABLE_UNUSED_LO_OFFSET = 0x1C;
static constexpr uint32_t EXT4_GD_CHECKSUM_OFFSET = 0x1E;
static constexpr uint32_t EXT4_GD_BLOCK_BITMAP_CSUM_HI_OFFSET = 0x38;
static constexpr uint32_t EXT4_GD_INODE_BITMAP_CSUM_HI_OFFSET = 0x3A;
static constexpr uint32_t EXT4_INODE_CSUM_LO_OFFSET = 0x7C;
static constexpr uint32_t EXT4_INODE_EXTRA_ISIZE_OFFSET = 0x80;
static constexpr uint32_t EXT4_INODE_CSUM_HI_OFFSET = 0x82;

static bool read_superblock_uuid(fstream& iso_file, char uuid[16]) {
    iso_file.clear();
    iso_file.seekg(EXT4_SUPERBLOCK_OFFSET + EXT4_SUPERBLOCK_UUID_OFFSET);
    return static_cast<bool>(iso_file.read(uuid, 16));
}

static uint32_t group_desc_size(const ext4_super_block& sb) {
    return (sb.s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) ? EXT4_GROUP_DESC_SIZE_64BIT : EXT4_GROUP_DESC_SIZE_LEGACY;
}

static uint32_t groups_count(const ext4_super_block& sb) {
    uint32_t count = sb.s_blocks_count_lo / sb.s_blocks_per_group;
    if (sb.s_blocks_count_lo % sb.s_blocks_per_group != 0) {
        count++;
    }
    return count;
}

static uint32_t inodes_in_group(const ext4_super_block& sb, uint32_t group_num) {
    uint32_t first_inode = group_num * sb.s_inodes_per_group;
    if (first_inode >= sb.s_inodes_count) {
        return 0;
    }

    uint32_t remaining = sb.s_inodes_count - first_inode;
    return remaining < sb.s_inodes_per_group ? remaining : sb.s_inodes_per_group;
}

static uint64_t group_desc_offset(const ext4_super_block& sb, uint32_t group_num, uint32_t block_size) {
    uint32_t bgd_block = sb.s_first_data_block + 1;
    return (static_cast<uint64_t>(bgd_block) * block_size) + (static_cast<uint64_t>(group_num) * group_desc_size(sb));
}

static void write_le16(vector<char>& data, uint32_t offset, uint16_t value) {
    if (offset + sizeof(value) <= data.size()) {
        memcpy(data.data() + offset, &value, sizeof(value));
    }
}

static bool read_group_desc_raw(fstream& iso_file, const ext4_super_block& sb, uint32_t group_num, uint32_t block_size, vector<char>& raw_desc, ext4_group_desc& bgd) {
    uint32_t desc_size = group_desc_size(sb);
    raw_desc.assign(desc_size, 0);
    memset(&bgd, 0, sizeof(bgd));

    iso_file.clear();
    iso_file.seekg(group_desc_offset(sb, group_num, block_size));
    if (!iso_file.read(raw_desc.data(), raw_desc.size())) {
        return false;
    }

    memcpy(&bgd, raw_desc.data(), sizeof(ext4_group_desc));
    return true;
}

static void sync_group_desc_to_raw(vector<char>& raw_desc, const ext4_group_desc& bgd) {
    if (raw_desc.size() >= sizeof(ext4_group_desc)) {
        memcpy(raw_desc.data(), &bgd, sizeof(ext4_group_desc));
    }
}

static bool update_bitmap_checksum(fstream& iso_file, const ext4_super_block& sb, vector<char>& raw_desc, const vector<char>& bitmap, size_t checksum_size, bool inode_bitmap) {
    if (!ext4_has_metadata_csum(sb)) {
        return true;
    }

    char uuid[16] = {0};
    if (!read_superblock_uuid(iso_file, uuid)) {
        return false;
    }

    if (checksum_size > bitmap.size()) {
        checksum_size = bitmap.size();
    }

    uint32_t checksum = checksum_bitmap(uuid, const_cast<char*>(bitmap.data()), checksum_size);
    uint16_t checksum_lo = checksum & 0xFFFF;
    uint16_t checksum_hi = (checksum >> 16) & 0xFFFF;

    uint32_t lo_offset = inode_bitmap ? EXT4_GD_INODE_BITMAP_CSUM_LO_OFFSET : EXT4_GD_BLOCK_BITMAP_CSUM_LO_OFFSET;
    uint32_t hi_offset = inode_bitmap ? EXT4_GD_INODE_BITMAP_CSUM_HI_OFFSET : EXT4_GD_BLOCK_BITMAP_CSUM_HI_OFFSET;

    write_le16(raw_desc, lo_offset, checksum_lo);
    write_le16(raw_desc, hi_offset, checksum_hi);
    return true;
}

static bool write_group_desc_raw(fstream& iso_file, const ext4_super_block& sb, uint32_t group_num, uint32_t block_size, vector<char>& raw_desc) {
    if (ext4_has_metadata_csum(sb) && raw_desc.size() >= EXT4_GROUP_DESC_SIZE_64BIT) {
        char uuid[16] = {0};
        if (!read_superblock_uuid(iso_file, uuid)) {
            return false;
        }

        uint16_t checksum = checksum_group(uuid, group_num, raw_desc.data());
        write_le16(raw_desc, EXT4_GD_CHECKSUM_OFFSET, checksum);
    }

    iso_file.clear();
    iso_file.seekp(group_desc_offset(sb, group_num, block_size));
    iso_file.write(raw_desc.data(), raw_desc.size());
    return static_cast<bool>(iso_file);
}

static uint16_t recompute_itable_unused(const ext4_super_block& sb, uint32_t group_num, const vector<char>& inode_bitmap) {
    uint32_t local_inodes = inodes_in_group(sb, group_num);
    uint32_t unused = 0;

    for (uint32_t bit = local_inodes; bit > 0; --bit) {
        if (check_bit(inode_bitmap.data(), bit - 1)) {
            break;
        }
        unused++;
    }

    return unused > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(unused);
}

static size_t inode_bitmap_checksum_size(const ext4_super_block& sb, uint32_t group_num) {
    return (inodes_in_group(sb, group_num) + 7) / 8;
}

static size_t block_bitmap_checksum_size(const ext4_super_block& sb, uint32_t block_size) {
    size_t bitmap_size = (sb.s_blocks_per_group + 7) / 8;
    return bitmap_size > block_size ? block_size : bitmap_size;
}

static uint16_t read_le16_from_vector(const vector<char>& data, uint32_t offset) {
    uint16_t value = 0;
    if (offset + sizeof(value) <= data.size()) {
        memcpy(&value, data.data() + offset, sizeof(value));
    }
    return value;
}

bool ext4_has_metadata_csum(const ext4_super_block& sb) {
    return (sb.s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_METADATA_CSUM) != 0;
}

static void write_superblock(fstream& iso_file, const ext4_super_block& sb) {
    vector<char> raw_superblock(1024, 0);

    iso_file.clear();
    iso_file.seekg(EXT4_SUPERBLOCK_OFFSET);
    iso_file.read(raw_superblock.data(), raw_superblock.size());

    memcpy(raw_superblock.data(), &sb, sizeof(ext4_super_block));

    if (ext4_has_metadata_csum(sb)) {
        uint32_t checksum = checksum_superblock(raw_superblock.data());
        memcpy(raw_superblock.data() + 1020, &checksum, sizeof(checksum));
    }

    iso_file.seekp(EXT4_SUPERBLOCK_OFFSET);
    iso_file.write(raw_superblock.data(), raw_superblock.size());
}

void update_dir_block_checksum(fstream& iso_file, const ext4_super_block& sb, uint32_t dir_inode_num, const ext4_inode& dir_inode, char* dir_block, uint32_t block_size) {
    if (!ext4_has_metadata_csum(sb) || block_size < EXT4_DIR_ENTRY_TAIL_REC_LEN) {
        return;
    }

    char uuid[16] = {0};
    if (!read_superblock_uuid(iso_file, uuid)) {
        return;
    }

    char* tail = dir_block + block_size - EXT4_DIR_ENTRY_TAIL_REC_LEN;
    uint32_t zero_inode = 0;
    uint16_t rec_len = EXT4_DIR_ENTRY_TAIL_REC_LEN;
    uint8_t name_len = 0;
    uint8_t file_type = EXT4_FT_DIR_CSUM;

    memcpy(tail, &zero_inode, sizeof(zero_inode));
    memcpy(tail + 4, &rec_len, sizeof(rec_len));
    memcpy(tail + 6, &name_len, sizeof(name_len));
    memcpy(tail + 7, &file_type, sizeof(file_type));

    uint32_t checksum = checksum_dir(uuid, dir_inode_num, dir_inode.i_generation, dir_block, block_size);
    memcpy(tail + 8, &checksum, sizeof(checksum));
}

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
    if (!iso_file.read(reinterpret_cast<char*>(&bgd), sizeof(ext4_group_desc))) {
        cerr << "\n[ERRO] Falha ao ler Tabela de Descritores no offset: " << bgd_offset << endl;
        return;
    }

    // 3. Achar o Inode dentro da tabela desse grupo
    uint64_t inode_table_offset = static_cast<uint64_t>(bgd.bg_inode_table_lo) * block_size;
    
    // sb.s_inode_size é 128 ou 256 bytes
    uint64_t exact_inode_offset = inode_table_offset + (local_inode_idx * sb.s_inode_size);

    iso_file.seekg(exact_inode_offset);

    if (!iso_file.read(reinterpret_cast<char*>(&inode_out), sizeof(ext4_inode))) {
        cerr << "\n[ERRO] Falha ao ler Inode " << inode_num << " no offset: " << exact_inode_offset << endl;
    };
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

bool check_bit(const char* bitmap, uint32_t bit_index) {
    uint32_t byte_index = bit_index / 8;
    uint32_t bit_offset = bit_index % 8;
    return (bitmap[byte_index] & (1 << bit_offset)) != 0;
}

void set_bit(char* bitmap, uint32_t bit_index) {
    uint32_t byte_index = bit_index / 8;
    uint32_t bit_offset = bit_index % 8;
    bitmap[byte_index] |= (1 << bit_offset);
}

uint32_t allocate_inode(fstream& iso_file, ext4_super_block& sb) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t total_groups = groups_count(sb);

    for (uint32_t g = 0; g < total_groups; g++) {
        ext4_group_desc bgd;
        vector<char> raw_desc;
        if (!read_group_desc_raw(iso_file, sb, g, block_size, raw_desc, bgd)) {
            continue;
        }

        if (bgd.bg_free_inodes_count_lo > 0) {
            vector<char> bitmap(block_size);
            iso_file.seekg((uint64_t)bgd.bg_inode_bitmap_lo * block_size);
            iso_file.read(bitmap.data(), block_size);

            uint32_t local_inodes = inodes_in_group(sb, g);
            for (uint32_t bit = 0; bit < local_inodes; bit++) {
                if (!check_bit(bitmap.data(), bit)) {
                    
                    set_bit(bitmap.data(), bit);

                    iso_file.seekp((uint64_t)bgd.bg_inode_bitmap_lo * block_size);
                    iso_file.write(bitmap.data(), block_size);

                    bgd.bg_free_inodes_count_lo--;
                    uint32_t first_unused_idx = local_inodes - bgd.bg_itable_unused_lo;
                    if (bit >= first_unused_idx) {
                        bgd.bg_itable_unused_lo = local_inodes - bit - 1;
                    }
                    sync_group_desc_to_raw(raw_desc, bgd);
                    update_bitmap_checksum(iso_file, sb, raw_desc, bitmap, inode_bitmap_checksum_size(sb, g), true);
                    write_group_desc_raw(iso_file, sb, g, block_size, raw_desc);

                    sb.s_free_inodes_count--;
                    write_superblock(iso_file, sb);

                    return (g * sb.s_inodes_per_group) + bit + 1;
                }
            }
        }
    }
    return 0; 
}

void write_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num, const ext4_inode& inode) {
    iso_file.clear();

    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t group_num = (inode_num - 1) / sb.s_inodes_per_group;
    uint32_t local_idx = (inode_num - 1) % sb.s_inodes_per_group;

    ext4_group_desc bgd;
    uint32_t bgd_block = sb.s_first_data_block + 1;
    uint32_t desc_size = (sb.s_feature_incompat & 0x80) ? 64 : 32;
    
    iso_file.seekg((uint64_t)bgd_block * block_size + (group_num * desc_size));
    iso_file.read(reinterpret_cast<char*>(&bgd), sizeof(ext4_group_desc));

    uint64_t inode_table_offset = (uint64_t)bgd.bg_inode_table_lo * block_size;
    uint64_t exact_inode_offset = inode_table_offset + (local_idx * sb.s_inode_size);

    vector<char> raw_inode(sb.s_inode_size, 0);

    iso_file.seekg(exact_inode_offset);
    iso_file.read(raw_inode.data(), sb.s_inode_size);

    bool inode_was_free = read_le16_from_vector(raw_inode, 0) == 0;
    if (inode_was_free) {
        memset(raw_inode.data(), 0, raw_inode.size());
    }

    size_t bytes_to_copy = sizeof(ext4_inode);
    if (bytes_to_copy > raw_inode.size()) {
        bytes_to_copy = raw_inode.size();
    }

    memcpy(raw_inode.data(), &inode, bytes_to_copy);

    if (ext4_has_metadata_csum(sb) && raw_inode.size() >= sb.s_inode_size && raw_inode.size() >= 256) {
        char uuid[16] = {0};
        if (read_superblock_uuid(iso_file, uuid)) {
            uint32_t checksum = checksum_inode(uuid, inode_num, inode.i_generation, raw_inode.data());
            uint16_t checksum_lo = checksum & 0xFFFF;
            uint16_t checksum_hi = (checksum >> 16) & 0xFFFF;
            uint16_t extra_isize = read_le16_from_vector(raw_inode, EXT4_INODE_EXTRA_ISIZE_OFFSET);

            memcpy(raw_inode.data() + EXT4_INODE_CSUM_LO_OFFSET, &checksum_lo, sizeof(checksum_lo));
            if (extra_isize >= 4 && EXT4_INODE_CSUM_HI_OFFSET + sizeof(checksum_hi) <= raw_inode.size()) {
                memcpy(raw_inode.data() + EXT4_INODE_CSUM_HI_OFFSET, &checksum_hi, sizeof(checksum_hi));
            }
        }
    }
    
    // Preserva os bytes estendidos do inode que ainda nao sao modelados pela struct.
    iso_file.seekp(exact_inode_offset);
    iso_file.write(raw_inode.data(), raw_inode.size());
}

void update_group_used_dirs_count(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num, int delta) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t group_num = (inode_num - 1) / sb.s_inodes_per_group;

    ext4_group_desc bgd;
    vector<char> raw_desc;
    if (!read_group_desc_raw(iso_file, sb, group_num, block_size, raw_desc, bgd)) {
        return;
    }

    if (delta > 0) {
        uint32_t updated = bgd.bg_used_dirs_count_lo + static_cast<uint32_t>(delta);
        bgd.bg_used_dirs_count_lo = updated > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(updated);
    } else if (delta < 0) {
        uint32_t decrement = static_cast<uint32_t>(-delta);
        bgd.bg_used_dirs_count_lo = decrement > bgd.bg_used_dirs_count_lo ? 0 : bgd.bg_used_dirs_count_lo - decrement;
    }

    sync_group_desc_to_raw(raw_desc, bgd);
    write_group_desc_raw(iso_file, sb, group_num, block_size, raw_desc);
}

// Função para arredondar o tamanho para o múltiplo de 4 mais próximo
uint32_t get_dir_rec_len(uint32_t name_length) {
    return (8 + name_length + 3) & ~3; 
}

bool add_dir_entry(fstream& iso_file, const ext4_super_block& sb, uint32_t parent_inode_num, uint32_t target_inode, const string& name, uint8_t file_type) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    
    ext4_inode parent_inode;
    read_inode(iso_file, sb, parent_inode_num, parent_inode);

    uint32_t required_space = get_dir_rec_len(name.length());
    
    uint32_t logical_block = 0;
    uint32_t remaining_bytes = parent_inode.i_size_lo;
    vector<char> buffer(block_size);

    while (remaining_bytes > 0) {
        uint64_t phys_block = get_physical_block(parent_inode, logical_block);
        if (phys_block == 0) break;

        iso_file.seekg(phys_block * block_size);
        iso_file.read(buffer.data(), block_size);

        uint32_t offset = 0;
        while (offset < block_size) {
            ext4_dir_entry_2* entry = reinterpret_cast<ext4_dir_entry_2*>(buffer.data() + offset);
            
            // Fim inesperado do bloco
            if (entry->rec_len == 0) break; 

            uint32_t actual_rec_len = get_dir_rec_len(entry->name_len);

            // Se for um arquivo válido (inode != 0)
            if (entry->inode != 0) {
                // Calcula quanto de espaço está sobrando neste registro
                uint32_t free_space = entry->rec_len - actual_rec_len;

                // Se o espaço que sobra é maior ou igual ao que precisamos
                if (free_space >= required_space) {
                    
                    // 1. Encurta o registro atual para o seu tamanho estritamente necessário
                    entry->rec_len = actual_rec_len;

                    // 2. O nosso novo arquivo entra logo em seguida
                    uint32_t new_offset = offset + actual_rec_len;
                    ext4_dir_entry_2* new_entry = reinterpret_cast<ext4_dir_entry_2*>(buffer.data() + new_offset);
                    
                    new_entry->inode = target_inode;
                    new_entry->rec_len = free_space; // Ele herda todo o espaço que sobrou
                    new_entry->name_len = name.length();
                    new_entry->file_type = file_type;
                    memcpy(new_entry->name, name.c_str(), name.length());

                    update_dir_block_checksum(iso_file, sb, parent_inode_num, parent_inode, buffer.data(), block_size);

                    // 3. Salva o bloco modificado no disco
                    iso_file.seekp(phys_block * block_size);
                    iso_file.write(buffer.data(), block_size);
                    
                    return true;
                }
            }
            offset += entry->rec_len; // Pula para o próximo registro
        }
        
        uint32_t bytes_to_read = (remaining_bytes < block_size) ? remaining_bytes : block_size;
        remaining_bytes -= bytes_to_read;
        logical_block++;
    }

    cout << "Erro: Sem espaco no bloco de diretorio (alocar novos blocos nao implementado)." << endl;
    return false;
}

uint64_t allocate_block(fstream& iso_file, ext4_super_block& sb) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t total_groups = groups_count(sb);

    for (uint32_t g = 0; g < total_groups; g++) {
        ext4_group_desc bgd;
        vector<char> raw_desc;
        if (!read_group_desc_raw(iso_file, sb, g, block_size, raw_desc, bgd)) {
            continue;
        }

        if (bgd.bg_free_blocks_count_lo > 0) {
            vector<char> bitmap(block_size);
            iso_file.seekg((uint64_t)bgd.bg_block_bitmap_lo * block_size);
            iso_file.read(bitmap.data(), block_size);

            for (uint32_t bit = 0; bit < sb.s_blocks_per_group; bit++) {
                if (!check_bit(bitmap.data(), bit)) {
                    
                    set_bit(bitmap.data(), bit);

                    iso_file.seekp((uint64_t)bgd.bg_block_bitmap_lo * block_size);
                    iso_file.write(bitmap.data(), block_size);

                    bgd.bg_free_blocks_count_lo--;
                    sync_group_desc_to_raw(raw_desc, bgd);
                    update_bitmap_checksum(iso_file, sb, raw_desc, bitmap, block_bitmap_checksum_size(sb, block_size), false);
                    write_group_desc_raw(iso_file, sb, g, block_size, raw_desc);

                    sb.s_free_blocks_count_lo--;
                    write_superblock(iso_file, sb);

                    // O número do bloco é = Primeiro bloco de dados (0 ou 1) + (Grupo * blocos por grupo) + bit local
                    uint64_t new_block = sb.s_first_data_block + (g * sb.s_blocks_per_group) + bit;

                    // Zera o novo bloco no disco para evitar lixo
                    vector<char> empty_block(block_size, 0);
                    iso_file.seekp(new_block * block_size);
                    iso_file.write(empty_block.data(), block_size);

                    return new_block;
                }
            }
        }
    }
    return 0; // Sem espaço
}

void free_inode(fstream& iso_file, ext4_super_block& sb, uint32_t inode_num) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t group_num = (inode_num - 1) / sb.s_inodes_per_group;
    uint32_t local_idx = (inode_num - 1) % sb.s_inodes_per_group;

    ext4_group_desc bgd;
    vector<char> raw_desc;
    if (!read_group_desc_raw(iso_file, sb, group_num, block_size, raw_desc, bgd)) {
        return;
    }

    vector<char> bitmap(block_size);
    iso_file.seekg((uint64_t)bgd.bg_inode_bitmap_lo * block_size);
    iso_file.read(bitmap.data(), block_size);

    // Zera o bit (marca como livre)
    bitmap[local_idx / 8] &= ~(1 << (local_idx % 8));

    iso_file.seekp((uint64_t)bgd.bg_inode_bitmap_lo * block_size);
    iso_file.write(bitmap.data(), block_size);

    bgd.bg_free_inodes_count_lo++;
    bgd.bg_itable_unused_lo = recompute_itable_unused(sb, group_num, bitmap);
    sync_group_desc_to_raw(raw_desc, bgd);
    update_bitmap_checksum(iso_file, sb, raw_desc, bitmap, inode_bitmap_checksum_size(sb, group_num), true);
    write_group_desc_raw(iso_file, sb, group_num, block_size, raw_desc);

    sb.s_free_inodes_count++;
    write_superblock(iso_file, sb);
}

bool remove_dir_entry(fstream& iso_file, const ext4_super_block& sb, uint32_t parent_inode_num, const string& name) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    ext4_inode parent_inode;
    read_inode(iso_file, sb, parent_inode_num, parent_inode);

    uint32_t logical_block = 0;
    uint32_t remaining_bytes = parent_inode.i_size_lo;
    vector<char> buffer(block_size);

    while (remaining_bytes > 0) {
        uint64_t phys_block = get_physical_block(parent_inode, logical_block);
        if (phys_block == 0) break;

        iso_file.seekg(phys_block * block_size);
        iso_file.read(buffer.data(), block_size);

        uint32_t offset = 0;
        ext4_dir_entry_2* prev_entry = nullptr;

        while (offset < block_size) {
            ext4_dir_entry_2* entry = reinterpret_cast<ext4_dir_entry_2*>(buffer.data() + offset);
            if (entry->rec_len == 0) break;

            // Se encontrou o arquivo
            if (entry->inode != 0 && entry->name_len == name.length() && strncmp(entry->name, name.c_str(), name.length()) == 0) {
                if (prev_entry != nullptr) {
                    // Apaga o registro atual somando o tamanho no anterior
                    prev_entry->rec_len += entry->rec_len;
                } else {
                    // Se for o primeiro do bloco, apaga logicamente
                    entry->inode = 0; 
                }

                update_dir_block_checksum(iso_file, sb, parent_inode_num, parent_inode, buffer.data(), block_size);

                iso_file.seekp(phys_block * block_size);
                iso_file.write(buffer.data(), block_size);
                return true;
            }
            prev_entry = entry;
            offset += entry->rec_len;
        }
        uint32_t bytes_to_read = (remaining_bytes < block_size) ? remaining_bytes : block_size;
        remaining_bytes -= bytes_to_read;
        logical_block++;
    }
    return false;
}

bool is_dir_empty(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num) {
    // Busca todas as entradas do diretório
    auto entries = search_filedir(iso_file, sb, inode_num);
    
    // Se tiver apenas 2 (ou menos, em caso de raiz corrompida), é apenas o '.' e '..'
    return entries.size() <= 2;
}

void free_block(fstream& iso_file, ext4_super_block& sb, uint64_t block_num) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t group_num = (block_num - sb.s_first_data_block) / sb.s_blocks_per_group;
    uint32_t local_idx = (block_num - sb.s_first_data_block) % sb.s_blocks_per_group;

    ext4_group_desc bgd;
    vector<char> raw_desc;
    if (!read_group_desc_raw(iso_file, sb, group_num, block_size, raw_desc, bgd)) {
        return;
    }

    vector<char> bitmap(block_size);
    iso_file.seekg((uint64_t)bgd.bg_block_bitmap_lo * block_size);
    iso_file.read(bitmap.data(), block_size);

    // Zera o bit correspondente ao bloco
    bitmap[local_idx / 8] &= ~(1 << (local_idx % 8));

    iso_file.seekp((uint64_t)bgd.bg_block_bitmap_lo * block_size);
    iso_file.write(bitmap.data(), block_size);

    bgd.bg_free_blocks_count_lo++;
    sync_group_desc_to_raw(raw_desc, bgd);
    update_bitmap_checksum(iso_file, sb, raw_desc, bitmap, block_bitmap_checksum_size(sb, block_size), false);
    write_group_desc_raw(iso_file, sb, group_num, block_size, raw_desc);

    sb.s_free_blocks_count_lo++;
    write_superblock(iso_file, sb);
}
