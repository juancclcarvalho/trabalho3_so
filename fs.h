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


#define MAGIC_NUMBER 4242424242424242

superblock_t* superblock;
int PWD;

typedef struct superblock
{
    long long inode_count; // qtd inodes
    long long block_count;//de blocos de dados
    long long inode_root;// Qual bloco começa os inodes?
    long long data_begin;// Onde começam os blocos de dados?
    long long magic_number;// Número mágico 😎 
}superblock_t;

typedef struct inode
{
    int inodeNo;  //  4 bytes -- número do i-node do arquivo
    short type;  // 2 bytes -- tipo do i-node doarquivo: DIRECTORY, FILE_TYPE (há outro valor FREE_INODE que nunca aparece aqui)
    char links; // 1 byte -- número de links para o i-node
    int size;  // 4 bytes -- tamanho do arquivo em byte
    int numBlocks; // 4 bytes -- número de blocos usados pelo arquivo
    int dot; // 4 bytes
    int dotdot; // 4 bytes
    char data[105]; // if(type == DIRECTORY) 
} inode_t;

typedef struct file_descriptor // TODO: Se der merda, temos um possível culpado
{
    inode_t* inode;
    int seek_ptr;
} fd_t;

fd_t* file_table[2044];

// assinaturas de funções criadas por nós
int find_unused_block();
void mark_block_as_occupied(int block);
inode_t* find_empty_inode();

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
