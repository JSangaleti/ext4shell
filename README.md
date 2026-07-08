# ext4shell

Um *shell* simplificado desenvolvido em C/C++ para ler e manipular imagens de disco formatadas em EXT4. O programa interage diretamente com as estruturas de dados da imagem (como superbloco, descritores de grupo, *inodes* e *extents*), sem depender de chamadas e bibliotecas de montagem do sistema operacional.

O sistema suporta a manipulação de imagens com blocos de tamanho 1 KiB, 2 KiB e 4 KiB e sempre inicia a navegação a partir do diretório raiz (`/`).

## 📂 Estrutura do Projeto

```text
ext4shell/
├── src/
│   ├── main.cpp
│   ├── ext4.cpp
│   ├── shell.cpp
│   └── commands.cpp
├── include/
│   ├── ext4.hpp
│   ├── shell.hpp
│   └── commands.hpp
├── build/
├── Makefile
├── README.md
└── .gitignore

```

## 🛠️ Dependências

Para compilar e executar este projeto, você precisará de:

* **Compilador C++**: `g++` com suporte ao padrão C++11 ou superior.


* **Make**: Ferramenta de automação de compilação.


* **Ambiente Linux**: Para utilizar os utilitários de criação de imagens de teste (`dd`, `mkfs.ext4`, `e2fsck`, `debugfs`).



## ⚙️ Compilação

O projeto utiliza um `Makefile` para facilitar o processo de build. Na raiz do projeto, abra o terminal e execute:

```bash
# Para compilar o projeto:
make

# Para limpar os arquivos compilados (se necessário):
make clean

```

Isso irá compilar os arquivos da pasta `src/` usando os cabeçalhos em `include/` e gerará o executável principal.

## 🚀 Como usar

Após compilar o projeto, inicie o shell passando o caminho da imagem do sistema de arquivos como argumento:

```bash
./ext4shell myext4image.img

```

Uma vez dentro do ambiente do shell, você terá acesso aos seguintes comandos:

### Comandos de Leitura

* `help`: Exibe a lista de comandos disponíveis.


* `info`: Exibe informações da imagem e do sistema de arquivos.


* `ls`: Lista os arquivos e diretórios do diretório corrente.


* `cd <dir>`: Altera o diretório corrente para o definido como path.


* `pwd`: Exibe o diretório corrente (caminho absoluto).


* `cat <file>`: Exibe o conteúdo de um arquivo no formato texto.


* `attr <file|dir>`: Exibe os atributos de um arquivo (file) ou diretório (dir).


* `testi <inode_number>`: Verifica se um inode está livre ou ocupado.


* `testb <block_number>`: Verifica se um bloco está livre ou ocupado.


* `export <source_path> <target_path>`: Copia arquivo da imagem (source_path) para destino (target_path) no sistema operacional da máquina.



### Comandos de Escrita

* `touch <file>`: Cria o arquivo file com conteúdo vazio.


* `mkdir <dir>`: Cria o diretório dir vazio.


* `rm <file>`: Remove o arquivo file do sistema.


* `rmdir <dir>`: Remove o diretório dir, se estiver vazio.


* `rename <file> <newfilename>`: Renomeia arquivo file para newfilename.




## 🔧 Apêndice: Gerando e Depurando Imagens EXT4

Abaixo estão os comandos úteis para criar, verificar e explorar as imagens de teste utilizadas neste projeto.

### Criando uma imagem de teste (512 MiB com blocos de 4 KiB)

```bash
dd if=/dev/zero of=./myext4image.img bs=1024 count=512K
mkfs.ext4 -b 4096 ./myext4image.img

```

### Verificando a integridade do sistema de arquivos

```bash
e2fsck myext4image.img

```

### Montando e Desmontando a imagem no Linux

Para adicionar arquivos ou visualizar o comportamento do sistema real, monte a imagem:

```bash
sudo mount myext4image.img /mnt

```

Para desmontar:

```bash
sudo umount /mnt

```

### Depurando a imagem do volume

Utilize as ferramentas de depuração nativas do Linux para conferir os offsets e metadados:

```bash
sudo debugfs myext4image.img
# Dentro do prompt do debugfs, você pode usar: stats

```

Alternativamente, obtenha estatísticas rapidamente com:

```bash
fsstat myext4image.img

```

### Estrutura original de arquivos de teste (referência)

Para garantir que a implementação funcione corretamente, a seguinte estrutura foi criada e inspecionada via `tree`:

```text
/
├── [1.0K] documentos
│   ├── [1.0K] emptydir
│   ├── [9.2K] alfabeto.txt
│   └── [   0] vazio.txt
├── [1.0K] imagens
│   ├── [8.1M] one_piece.jpg
│   ├── [391K] saber.jpg
│   └── [ 11M] toscana_puzzle.jpg
├── [1.0K] livros
│   ├── [1.0K] classicos
│   │   ├── [506K] A Journey to the Centre of the Earth - Jules Verne.txt
│   │   ├── [409K] Dom Casmurro - Machado de Assis.txt
│   │   ├── [861K] Dracula-Bram_Stoker.txt
│   │   ├── [455K] Frankenstein-Mary_Shelley.txt
│   │   └── [232K] The Worderful Wizard of Oz - L. Frank Baum.txt
│   └── [1.0K] religiosos
│       └── [3.9M] Biblia.txt
├── [ 12K] lost+found
└── [  29] hello.txt
