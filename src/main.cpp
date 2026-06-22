#include <iostream>
#include <fstream>

#include "../include/shell.hpp"

using namespace std;

bool DEBUG_MODE = true;

int main(int argc, char const *argv[])
{
    if (argc == 2){

        fstream iso_file(argv[1], std::ios::in | std::ios::out | std::ios::binary);

        if (!iso_file.is_open())
        {
            std::cerr << "Erro ao abrir a imagem!" << std::endl;
            return 1;
        }

        return start_shell(iso_file);

    } else {
        cout << "Por favor passe o caminho para o arquivo, ex.: ./" << argv[0] << "\"caminho/para/a/iso\"" << endl;
        return 1;
    }
}
