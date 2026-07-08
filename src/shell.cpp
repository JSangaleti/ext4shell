#include "commands.hpp"
#include "shell.hpp"

#include <limits>

static bool parse_uint64_arg(const string& text, uint64_t& value) {
    if (text.empty()) {
        return false;
    }

    if (text[0] == '+' || text[0] == '-') {
        return false;
    }

    try {
        size_t parsed_chars = 0;
        unsigned long long parsed_value = stoull(text, &parsed_chars, 10);
        if (parsed_chars != text.size()) {
            return false;
        }

        value = static_cast<uint64_t>(parsed_value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

static bool parse_uint32_arg(const string& text, uint32_t& value) {
    uint64_t parsed_value = 0;
    if (!parse_uint64_arg(text, parsed_value)) {
        return false;
    }

    if (parsed_value > numeric_limits<uint32_t>::max()) {
        return false;
    }

    value = static_cast<uint32_t>(parsed_value);
    return true;
}

int start_shell(fstream& iso_file){
    
    fs_state state;
    ext4_super_block super_block;

    try
    {
        read_superblock(iso_file, super_block, 1024);

        if (super_block.s_magic != 0xEF53) {
            cerr << "Erro fatal: O arquivo nao e uma imagem ext4 valida (Magic Number incorreto)." << endl;
            return 1;
        }

        if (!(super_block.s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS)) {
            cerr << "Erro fatal: A imagem ext4 nao possui suporte a Extents (Feature flag 0x40 ausente)." << endl;
            return 1;
        }

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
        // 1. Pega o comando
        stringstream ss(entry);
        ss >> command;

        // 2. Pega o restante da linha bruta
        string rest;
        getline(ss, rest);

        // 3. Função para extrair argumentos respeitando aspas
        auto get_args = [](string s) {
            vector<string> args;
            bool in_quotes = false;
            string current;
            for (char c : s) {
                if (c == '"') in_quotes = !in_quotes;
                else if (c == ' ' && !in_quotes) {
                    if (!current.empty()) { args.push_back(current); current = ""; }
                } else current += c;
            }
            if (!current.empty()) args.push_back(current);
            return args;
        };

        vector<string> args = get_args(rest);
        arg1 = (args.size() > 0) ? args[0] : "";
        arg2 = (args.size() > 1) ? args[1] : "";

        if (command == "exit" || command == "quit") {
            break;
        }

        if (command == "help") {
            help();
            continue;
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
                cout << "Erro: informe o bloco." << endl;
                continue;
            }
            uint32_t block_number = 0;
            if (!parse_uint32_arg(arg1, block_number)) {
                cout << "Erro: bloco invalido." << endl;
                continue;
            }
            if (block_number >= super_block.s_blocks_count_lo) {
                cout << "Erro: bloco fora da imagem." << endl;
                continue;
            }
            print_block(iso_file, block_number, state.block_size);
            continue;
        }

        if (command == "pwd") {
            pwd(state);
            continue;
        }

        if (command == "print_inode") {
            uint32_t num = 2;
            if (!arg1.empty() && !parse_uint32_arg(arg1, num)) {
                cout << "Erro: inode invalido." << endl;
                continue;
            }
            if (num == 0 || num > super_block.s_inodes_count) {
                cout << "Erro: inode fora da imagem." << endl;
                continue;
            }
            print_inode(iso_file, super_block, num);
            continue;
        }

        if (command == "ls") {
            ls(iso_file, super_block, state);
            continue;
        }

        if (command == "cd" || command == "cat" || command == "attr" || command == "testi" || command == "testb" || command == "touch" || command == "mkdir" || command == "rm" || command == "rmdir") {
            if (arg1.empty()) {
                cout << "Erro: o comando '" << command << "' precisa de um argumento." << endl;
                continue;
            }
        }

        if (command == "cd") {
            cd(arg1, iso_file, super_block, state);

        } else if (command == "cat") {
            cat(arg1, iso_file, super_block, state);

        } else if (command == "attr") {
            attr(arg1, iso_file, super_block, state);

        } else if (command == "testi") {
            uint32_t inode_number = 0;
            if (!parse_uint32_arg(arg1, inode_number)) {
                cout << "Erro: inode invalido." << endl;
                continue;
            }
            if (inode_number == 0 || inode_number > super_block.s_inodes_count) {
                cout << "Erro: inode fora da imagem." << endl;
                continue;
            }
            bool used = testi(inode_number, iso_file, super_block);
            cout << "Inode " << arg1 << " esta " << (used ? "OCUPADO" : "LIVRE") << endl;

        } else if (command == "testb") {
            uint64_t block_number = 0;
            if (!parse_uint64_arg(arg1, block_number)) {
                cout << "Erro: bloco invalido." << endl;
                continue;
            }
            if (block_number >= super_block.s_blocks_count_lo) {
                cout << "Erro: bloco fora da imagem." << endl;
                continue;
            }
            bool used = testb(block_number, iso_file, super_block);
            cout << "Bloco " << arg1 << " esta " << (used ? "OCUPADO" : "LIVRE") << endl;

        } else if (command == "touch") {
            touch(arg1, iso_file, super_block, state);

        } else if (command == "mkdir") {
            mkdir(arg1, iso_file, super_block, state);

        } else if (command == "rm") {
            rm(arg1, iso_file, super_block, state);

        } else if (command == "rmdir") {
            rmdir(arg1, iso_file, super_block, state);

        } else if (command == "rename") {
            if (arg1.empty() || arg2.empty()) {
                cout << "Erro: rename precisa de dois argumentos." << endl;
            } else {
                rename(arg1, arg2, iso_file, super_block, state);
            }

        } else if (command == "export") {
            if (arg1.empty() || arg2.empty()) {
                cout << "Erro: export precisa de dois argumentos." << endl;
            } else {
                command_export(arg1, arg2, iso_file, super_block, state);
            }
        } else {
            cout << "Erro: comando desconhecido '" << command << "'." << endl;
        }
    }
    return 0;
};
