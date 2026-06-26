#pragma once

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string>
#include <cstring>

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
    uint16_t bg_free_blocks_count_lo; // Qtd de blocos livres no grupo
    uint16_t bg_free_inodes_count_lo; // Qtd de inodes livres no grupo
    uint16_t bg_used_dirs_count_lo;   // Qtd de diretórios criados no grupo
    uint16_t bg_flags;                // Flags de estado
    uint32_t bg_exclude_bitmap_lo;    // Bloco do mapa de exclusão (snapshots)
    uint16_t bg_block_bitmap_csum_lo; // Checksum do mapa de blocos
    uint16_t bg_inode_bitmap_csum_lo; // Checksum do mapa de inodes
    uint16_t bg_itable_unused_lo;     // Inodes não usados na tabela
    uint16_t bg_checksum;             // Checksum geral do próprio descritor
} __attribute__((packed));

// --- Inode ---
// Guarda os metadados do arquivo/diretório e a árvore de onde os dados estão
struct ext4_inode {
    uint16_t i_mode;        // Permissões e tipo (arquivo, diretório, etc)
    uint16_t i_uid;         // ID do usuário dono do arquivo (User ID)
    uint32_t i_size_lo;     // Tamanho do arquivo em bytes (parte baixa)
    uint32_t i_atime;       // Tempo do último acesso
    uint32_t i_ctime;       // Tempo de criação/mudança dos metadados
    uint32_t i_mtime;       // Tempo da última modificação dos dados
    uint32_t i_dtime;       // Tempo em que o arquivo foi deletado
    uint16_t i_gid;         // ID do grupo dono do arquivo (Group ID)
    uint16_t i_links_count; // Qtd de hard links apontando para este inode
    uint32_t i_blocks_lo;   // Qtd de blocos de 512 bytes reservados pelo inode
    uint32_t i_flags;       // Flags específicas do arquivo
    uint32_t i_osd1;        // Dados dependentes do Sistema Operacional
    uint32_t i_block[15];   // Aqui fica a Árvore de Extents (ou os dados diretos)
    uint32_t i_generation;  // Versão do arquivo
    uint32_t i_file_acl_lo; // Bloco que guarda atributos estendidos (ACL)
    uint32_t i_size_high;   // Tamanho do arquivo em bytes (parte alta para > 4GB)
    uint32_t i_obso_faddr;  // Endereço de fragmento (obsoleto no ext4)
} __attribute__((packed));

// --- Cabeçalho da Árvore de Extents ---
// Fica no início do i_block do Inode e controla a árvore de blocos
struct ext4_extent_header {
    uint16_t eh_magic;      // Assinatura de validação (deve ser obrigatoriamente 0xF30A)
    uint16_t eh_entries;    // Quantos extents (nós) válidos estão listados logo a seguir
    uint16_t eh_max;        // Capacidade máxima de extents que cabem neste espaço
    uint16_t eh_depth;      // Profundidade da árvore (0 significa que aponta para os blocos reais)
    uint32_t eh_generation; // Geração da árvore de extents
} __attribute__((packed));

// --- Extent (Nó Folha) ---
// Mapeia uma sequência de blocos lógicos para blocos físicos no disco
struct ext4_extent {
    uint32_t ee_block;      // Número do primeiro bloco lógico do arquivo coberto por este extent
    uint16_t ee_len;        // Quantidade de blocos contíguos armazenados em sequência
    uint16_t ee_start_hi;   // Parte alta (16 bits) do número do bloco físico real no disco
    uint32_t ee_start_lo;   // Parte baixa (32 bits) do número do bloco físico real no disco
} __attribute__((packed));

// --- Entrada de Diretório ---
// Guarda o nome do arquivo e aponta para o Inode dele
struct ext4_dir_entry_2 {
    uint32_t inode;      // Número do Inode (0 significa que o arquivo foi deletado)
    uint16_t rec_len;    // Tamanho total deste registro
    uint8_t name_len;    // Tamanho do nome do arquivo
    uint8_t file_type;   // Tipo: 1 = Arquivo, 2 = Diretório
} __attribute__((packed));

void read_superblock(fstream& iso_file, ext4_super_block& block_out, int pos);

void read_block(std::fstream& iso_file, uint32_t block_number, uint32_t block_size, char* buffer);

void write_block(fstream& iso_file, uint32_t block_number, uint32_t block_size, const char* buffer);

void read_inode(fstream& iso_file, const ext4_super_block& sb, uint32_t inode_num, ext4_inode& inode_out);

uint64_t get_physical_block(const ext4_inode& inode, uint32_t logical_block);