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

void write_block(fstream& iso_file, uint32_t block_number, uint32_t block_size, const char* buffer) {
    uint64_t offset = static_cast<uint64_t>(block_number) * block_size;
    
    iso_file.seekp(offset);
    
    iso_file.write(buffer, block_size);
}

void read_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num, ext4_inode& inode_out) {
    uint32_t block_size = 1024 << sb.s_log_block_size;
    
    // 1. Descobrir a qual grupo esse inode pertence e seu índice local
    uint32_t group_num = (inode_num - 1) / sb.s_inodes_per_group;
    uint32_t local_inode_idx = (inode_num - 1) % sb.s_inodes_per_group;

    // 2. Achar a Tabela de Descritores (BGD)
    // A BGD sempre fica no bloco imediatamente após o superbloco
    uint32_t bgd_block = sb.s_first_data_block + 1;
    
    uint64_t bgd_offset = (static_cast<uint64_t>(bgd_block) * block_size) + (group_num * 32);

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