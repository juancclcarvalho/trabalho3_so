#ifndef FS_INCLUDED
#define FS_INCLUDED

#define FS_SIZE 2048
#define INODE_BLOCKS 2044/4 // 511
#define MAX_FILE_SIZE 26 * BLOCK_SIZE

void fs_init( void);
int fs_mkfs( void);
int fs_open( char *fileName, int flags);
int fs_close( int fd);
int fs_read( int fd, char *buf, int count);
int fs_write( int fd, char *buf, int count);
int fs_lseek( int fd, int offset);
int fs_mkdir( char *fileName);
int fs_rmdir( char *fileName);
int fs_cd( char *dirName);
int fs_link( char *old_fileName, char *new_fileName);
int fs_unlink( int inodeNo);
int fs_stat( char *fileName, fileStat *buf);


#define MAGIC_NUMBER 42

int PWD;

typedef struct superblock
{
    long long inode_count; // qtd inodes
    long long block_count;//de blocos de dados
    long long inode_root;// Qual bloco começa os inodes?
    long long data_begin;// Onde começam os blocos de dados?
    long long magic_number;// Número mágico 😎 
}superblock_t;
superblock_t* superblock;

typedef struct inode
{
    int inodeNo;  //  4 bytes -- número do i-node do arquivo
    short type;  // 2 bytes -- tipo do i-node doarquivo: DIRECTORY, FILE_TYPE (há outro valor FREE_INODE que nunca aparece aqui)
    char links; // 1 byte -- número de links para o i-node
    int size;  // 4 bytes -- tamanho do arquivo em byte
    int numBlocks; // 4 bytes -- número de blocos usados pelo arquivo
    int dot; // 4 bytes
    int dotdot; // 4 bytes
    char data[105]; // 105 bytes
    /**
    *   Quando um inode for do tipo de diretorio, ele sempre vai ser organizado de mood que:
    *   para cada arquivo/diretório associado a ele seguimos a mesma estrutura
    *   preenchemos o nome primeiro (string)
    *   inserimos o '\0' para identificar o tamanho da string
    *   e inserimos um inteiro de 4 bytes para identificar o inode referenciado
    *
    *   Quando o inode for do tipo FILE_TYPE, não precisamos mais de uma string identificadora 
    *   e o campo data é preenchido inteiramente com inteiros para identificar um bloco.
    */
} __attribute__ ((packed)) inode_t;

typedef struct file_descriptor // TODO: Se der merda, temos um possível culpado
{
    inode_t* inode;
    int seek_ptr;
    int hasNoLinks;
} fd_t;

fd_t* file_table[2044];

// assinaturas de funções criadas por nós
inode_t* create_inode(int number);
inode_t* retrieve_inode(int index);
inode_t* search_filename(char *fileName);
int find_unused_block();
void mark_block_as_occupied(int block);
inode_t* find_empty_inode();
fileStat* fs_ls(int* fileQuantity, char*** nameMatrix);

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
