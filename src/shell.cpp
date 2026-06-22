#include "shell.hpp"
#include "ext4.hpp"

int start_shell(fstream& iso_file){
    struct fs
    {
        string path = "/";
        uint32_t current_inode = 2;
        uint32_t current_block;
    };
    
    fs state;

    try
    {
        ext4_super_block block;
        read_superblock(1024, iso_file, block);
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

        if (comando_digitado == "exit" || comando_digitado == "quit") {
            break;
        }

        cout << "[DEBUG] Você digitou: " << comando_digitado << endl;
    }

    return 0;
};
