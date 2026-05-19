# Apêndice A: Comandos e estrutura do volume myext2image.img

Gerando imagens ext4 (512MiB com blocos de 4K):

```sh
dd if=/dev/zero of=./myext4image.img bs=1024 count=512K
mkfs.ext4 -b 4096 ./myext4image.img
```

Verificando a integridade de um sistema ext4:

```sh
e2fsck myext4image.img
```

Montando a imagem do volume com ext4:

```sh
sudo mount myext4image.img /mnt
```

Depurando a imagem do volume com ext4:

```sh
sudo debugfs myext4image.img
```

Estrutura original de arquivos do volume (comando tree via bash):

```text
/
├── [1.0K] documentos
│ ├── [1.0K] emptydir
│ ├── [9.2K] alfabeto.txt
│ └── [0] vazio.txt
├── [1.0K] imagens
│ ├── [8.1M] one_piece.jpg
│ ├── [391K] saber.jpg
│ └── [ 11M] toscana_puzzle.jpg
├── [1.0K] livros
│ ├── [1.0K] classicos
│ │ ├── [506K] A Journey to the Centre of the Earth - Jules Verne.txt
│ │ ├── [409K] Dom Casmurro - Machado de Assis.txt
│ │ ├── [861K] Dracula-Bram_Stoker.txt
│ │ ├── [455K] Frankenstein-Mary_Shelley.txt
│ │ └── [232K] The Worderful Wizard of Oz - L. Frank Baum.txt
│ └── [1.0K] religiosos
│
├── [3.9M] Biblia.txt
├── [ 12K] lost+found
└── [ 29] hello.txt
```

Informações de espaço (comando df via bash):
Blocos de 4k: X
Usado: X KiB
Disponível: X KiB
Informações detalhadas do sistema de arquivos :

```sh
fsstat myext4image.img
```

ou

```sh
debugfs myext4image.img
> stats
```

Desmontando a imagem do volume com ext4:

```sh
sudo umount /mnt
``
