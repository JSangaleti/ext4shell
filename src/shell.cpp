#include "commands.hpp"
#include "shell.hpp"

int start_shell(fstream& iso_file){
    
    fs_state state;
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
    
    string entry;
    
    while (true) {

        cout << "ext4shell:[myext4image" << state.path << "] $ ";
        
        getline(cin, entry);
        if (entry.empty()) continue;

        // --- PARSER DE ARGUMENTOS ---
        string command, arg1, arg2;
        stringstream ss(entry);
        ss >> command;  // Pega o comando
        ss >> arg1;     // Pega o primeiro argumento (se existir)
        ss >> arg2;     // Pega o segundo argumento (se existir)

        if (command == "exit" || command == "quit") {
            break;
        }

        if (command == "info") {
            info(super_block, state);
            continue;
        }

        if (command == "print_superblock") {
            print_superblock(super_block);
            continue;
        }

        if (command == "print_block") {
            if (arg1.empty()) {
                cout << "Erro: informe o bloco. Ex: print_block 0" << endl;
                continue;
            }
            
            uint32_t num_bloco = stoi(arg1); 
            print_block(iso_file, num_bloco, state.block_size);
            continue;
        }

        if (command == "pwd"){
            pwd(state);
        }

    }
    return 0;
};
