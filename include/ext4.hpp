#pragma once

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string>

using namespace std;

// --- Super bloco ---
// Guarda os dados do super bloco 
struct ext4_super_block {
    uint32_t s_inodes_count;        // Total de inodes
    uint32_t s_blocks_count_lo;     // Total de blocos
    uint32_t s_r_blocks_count_lo;   // Blocos reservados
    uint32_t s_free_blocks_count_lo;// Blocos livres
    uint32_t s_free_inodes_count;   // Inodes livres
    uint32_t s_first_data_block;    // Primeiro bloco de dados
    uint32_t s_log_block_size;      // Tamanho do bloco
    uint32_t s_log_cluster_size;    // Tamanho do cluster de alocação
    uint32_t s_blocks_per_group;    // Blocos por grupo
    uint32_t s_clusters_per_group;  // Clusters por grupo
    uint32_t s_inodes_per_group;    // Inodes por grupo
    uint32_t s_mtime;               // Última montagem (tempo)
    uint32_t s_wtime;               // Última escrita (tempo)
    uint16_t s_mnt_count;           // Contagem de montagens
    uint16_t s_max_mnt_count;       // Máximo de montagens antes da checagem
    uint16_t s_magic;               // Assinatura mágica (Valor = 0xEF53)
    uint16_t s_state;               // Estado do sistema
    uint16_t s_errors;              // Comportamento em caso de erro
    uint16_t s_minor_rev_level;     // Nível de revisão menor
    uint32_t s_lastcheck;           // Tempo da última checagem
    uint32_t s_checkinterval;       // Intervalo máximo entre checagens
    uint32_t s_creator_os;          // SO criador
    uint32_t s_rev_level;           // Nível de revisão
    uint16_t s_def_resuid;          // UID padrão para blocos reservados
    uint16_t s_def_resgid;          // GID padrão para blocos reservados
    // --- IMPORTANTE: Estes campos só valem se s_rev_level > 0 ---
    uint32_t s_first_ino;           // Primeiro inode não reservado
    uint16_t s_inode_size;          // Tamanho do Inode
    uint16_t s_block_group_nr;      // Número do grupo deste superbloco
    uint32_t s_feature_compat;      // Features compatíveis
    uint32_t s_feature_incompat;    // Features incompatíveis
    uint32_t s_feature_ro_compat;   // Features apenas leitura
} __attribute__((packed));

// --- Tabela de Descritores de Grupo (BGD) ---
// Ela diz onde estão os mapas de bits e a tabela de inodes de cada grupo
struct ext4_group_desc {
    uint32_t bg_block_bitmap_lo;      // Bloco do mapa de bits de blocos
    uint32_t bg_inode_bitmap_lo;      // Bloco do mapa de bits de inodes
    uint32_t bg_inode_table_lo;       // Bloco inicial da tabela de inodes
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
} __attribute__((packed));

// --- Inode ---
// Guarda os metadados do arquivo/diretório e a árvore de onde os dados estão
struct ext4_inode {
    uint16_t i_mode;        // Permissões e tipo (arquivo, diretório, etc)
    uint16_t i_uid;
    uint32_t i_size_lo;     // Tamanho do arquivo em bytes
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;   // Quantidade de blocos de 512 bytes usados
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];   // Aqui fica a Árvore de Extents
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
} __attribute__((packed));

void read_superblock(fstream& iso_file, ext4_super_block& block_out, int pos);

void read_block(std::fstream& iso_file, uint32_t block_number, uint32_t block_size, char* buffer);

void write_block(fstream& iso_file, uint32_t block_number, uint32_t block_size, const char* buffer);

void read_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num, ext4_inode& inode_out);
