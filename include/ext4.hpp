#pragma once

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string>

using namespace std;

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

void read_superblock(fstream& iso_file, ext4_super_block& block_out, int pos);

void read_block(std::fstream& iso_file, uint32_t block_number, uint32_t block_size, char* buffer);

void write_block(fstream& iso_file, uint32_t block_number, uint32_t block_size, const char* buffer);