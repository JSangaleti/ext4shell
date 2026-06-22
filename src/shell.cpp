#include "shell.hpp"
#include "ext4.hpp"

int start_shell(fstream& iso_file){
    struct fs
    {
        string path = "/";
        uint32_t current_inode = 2;
        uint32_t block_size;
    };
    
    fs state;
    ext4_super_block super_block;

    try
    {
        read_superblock(iso_file, super_block, 1024);

        state.block_size = 1024 << super_block.s_log_block_size;
    }
    catch(const std::exception& e) {
        std::cerr << "Erro fatal ao ler superbloco: " << e.what() << '\n';
        return 1;
    }
    
    string comando_digitado;
    
    while (true) {

        cout << "ext4shell:[myext4image" << state.path << "] $ ";
        
        getline(cin, comando_digitado);
        if (comando_digitado.empty()) continue;

        // --- PARSER DE ARGUMENTOS ---
        string comando, arg1;
        stringstream ss(comando_digitado);
        ss >> comando; // Pega a primeira palavra
        ss >> arg1;    // Pega a segunda palavra (se existir)

        if (comando == "exit" || comando == "quit") {
            break;
        }

        if (comando == "info") {
            cout << "--- INFO DO SISTEMA DE ARQUIVOS ---" << endl;
            cout << "Tamanho do Bloco: " << state.block_size << " bytes" << endl;
            cout << "Total de Inodes:  " << super_block.s_inodes_count << endl;
            cout << "Total de Blocos:  " << super_block.s_blocks_count_lo << endl;
            
            cout << "--- ESTADO DO SHELL ---" << endl;
            cout << "Caminho Atual:    " << state.path << endl;
            cout << "Inode Atual:      " << state.current_inode << endl;
            continue;
        }

        if (comando == "print_superblock") {
            print_superblock(super_block);
            continue;
        }

        if (comando == "print_block") {
            if (arg1.empty()) {
                cout << "Erro: informe o bloco. Ex: print_block 0" << endl;
                continue;
            }
            
            uint32_t num_bloco = stoi(arg1); 
            print_block(iso_file, num_bloco, state.block_size);
            continue;
        }
    }
    return 0;
};
