#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

void create_inode(inode_t* inode, int number)
{
    inode->inodeNo = number;
    inode->type = FREE_INODE;  
    inode->links = 0; 
    inode->size = 0;  
    inode->numBlocks = 0; 
    inode->dot = number; 
    inode->dotdot = number;
}

void fs_init( void) 
{
    /*
        TODO: void fs init(void);
        Essa funcao inicializa as estruturas de dados e os recursos usados pelo subsistema
        do sistema de arquivos e, se detectar que o disco ja esta formatado, faz sua montagem automaticamente
        no diretorio raiz. Ela eh chamada no momento da inicializacao do sistema, mas antes do modulo de bloco
        (block.c) ser inicializado. Entao, voce precisa chamar block init em fs init. Observe que no momento
        em que fs init eh chamada, o disco nao esta necessariamente formatado. Como resultado, voce precisa criar
        um mecanismo para que um disco formatado seja reconhecido (consulte fs mkfs).
    */
    block_init();
    
    char* aux;
    block_read(0, aux);
    superblock = aux;
    if(superblock->magic_number == MAGIC_NUMBER)
        fs_mkfs(); // formata disco

    PWD = 1;

    for (fd_t** ptr = file_table; ptr != file_table + 2044; ptr++)
        *ptr = NULL;
}



int fs_mkfs(void) 
{
    /**
    *   TODO: int fs mkfs(void);
    *   Esta funcao formata um disco: um disco bruto ou um que foi formatado anteriormente.
    *   Em seguida, ela monta o sistema de arquivos recem-formatado no diretorio raiz. O tamanho do
    *   disco (em blocos) eh definido como FS SIZE em fs.h. 
    *   (Voce pode aumentar o tamanho do disco para teste, se desejar) 
    *   Assumimos que existe um e apenas um disco presente no sistema. Voce precisa escrever um
    *   numero magico no super bloco para que mais tarde possa ser reconhecido como um disco formatado 
    *   (ou seja, voce ainda pode acessar um disco formatado e obter seu conteudo apos o interpretador de comandos
    *   terminar e reiniciar). A funcao deve retornar 0 em caso de sucesso e -1 em caso de falha.
    */
    char* aux;
    int inode_counter = 0;
    inode_t* inode[4] = (inode_t*) malloc(sizeof(inode_t) * 4);
 
    for(int i = 1; i <= INODE_BLOCKS; i++) // percorre todo os inodes do sistema de arquivos
    {
        // cria 4 inodes que preenchem o bloco
        create_inode(inode[0], inode_counter++);
        create_inode(inode[1], inode_counter++);   
        create_inode(inode[2], inode_counter++);   
        create_inode(inode[3], inode_counter++);
        // aux recebe os quatro inodes
        aux = inode;
        // salvamos o bloco
        block_write(i,aux);
    }

    // aloca mapa de alocação 
    aux = (char*) malloc(BLOCK_SIZE);
    // preenche com zeros
    memset(aux, 0xFF, BLOCK_SIZE);
    // guarda no disco
    block_write(INODE_BLOCKS + 1, aux);

    return 0;
}


inode_t* retrieve_inode(int index)
{
    char* aux;
    block_read((index/4) + 1, aux); // lê bloco relacionado ao indice DO INODE (inode_t->inodeNo)
    inode_t* inode = &aux[(index % 4) * sizeof(inode_t)]; // busca inode correto dentro dos 4 inodes presentes do bloco
    return inode;
}

inode_t* search_filename(char *fileName)
{
    /**
         TODO: TRATAR CASOS DE QUANDO |
                                      |
         FILENAME = "."       <-------|
         FILENAME = ".."      <-------|
    */
    inode_t* inode = retrieve_inode(PWD);
    int fileNameSize = strlen(fileName);
    
    if(inode->type == DIRECTORY)
    {
        // inode->data[dataIndex]
        int dataIndex = 0;
        // iteramos sob o campo data
        for(int i = 0 ; i < inode->numBlocks; i++)
        {
            // fileName[fileStrIndex]
            int fileStrIndex = 0;
            short flag = 1;
            while(dataIndex < (105 - 4)) // itera sob o campo de dados
            {
                // temos 4 situações , funciona pra todos?
                // strings iguais
                // -----------> sim? VAI entrar no condicional de quando fileName terminar em '\0'
                // -----------> 

                // strings diferentes, fileName tem tamanho maior
                // -----------> NUNCA vai entrar no condicional de quando fileName terminar em '\0'.
                // -----------> o condicional if(inode->data[dataIndex] == '\0') vai ser ativado
                // -----------> damos break e tentamos novamente com a proxima string, se tiver
                
                // strings diferentes, fileName tem tamanho menor
                // -----------> eh possivel entrar no condicional de quando fileName terminar em '\0'. (um e substring do outro)
                // -----------> vai verificar que inode->data não terminou e setar a flag como falsa.
                // -----------> em todos os laços subsequentes, o primeiro condicional vai pro else até acabar a string em inode->data,
                // -----------> damos break e tentamos novamente com a proxima string, se tiver

                // strings diferentes, fileName tem tamanho igual
                // -----------> NÃO vai entrar no condicional de quando fileName terminar em '\0'
                // -----------> A flag vai ser necessariamente falsa e nunca entrará no condicional
                // -----------> damos break e tentamos novamente com a proxima string, se tiver

                if(fileName[fileStrIndex] == '\0' && flag) // terminou de ler a string
                {
                    // condicional interno evita falsamente avaliar o exemplo abaixo como verdadeiro
                    // filename =  "abc"
                    // inode->data "abcd"
                    if(inode->data[dataIndex] == '\0') // encontrou a string correta
                    {
                        // deu bom
                        int index;
                        // constroi int a partir do char[4]
                        memcpy(&index, inode->data[dataIndex + 1], sizeof(int));
                        inode_t* foundInode = retrieve_inode(index);
                        return foundInode;
                    }
                    else flag = 0;
                }
                else // vai lendo a string  
                {
                    if(inode->data[dataIndex] != fileName[fileStrIndex]) 
                        flag = 0;
                    
                    if(inode->data[dataIndex] == '\0')
                    {
                        dataIndex += 5;
                        break;
                    }

                    if(fileName[fileStrIndex] != '\0')
                        fileStrIndex++;
                }
                dataIndex++;
            }
        }
        return NULL;
    }
    else
    {
        // função precisa buscar o arquivo dentro de um diretório.
        // idealmente PWD nunca apontará para um arquivo... eu sou burro
        return NULL;
    }
}

inode_t* find_empty_inode()
{

    for(int i = 2; i <= INODE_BLOCKS * 4; i++)
    {
        inode_t* inode = retrieve_inode(i);
        if(inode->type == FREE_INODE)
            return inode;
    }

    return NULL;
}

/*
    TODO: int fs open(char *filename, int flags);
    Dado um nome de arquivo (filename), fs open() 
    retorna um descritor de arquivo, um numero inteiro pequeno e nao negativo para uso em chamadas de sistema subsequentes 
    (fs read,fs write, fs lseek, etc.).
    
    O descritor de arquivo retornado por uma chamada bem-sucedida sera o descritor
    de arquivo com o numero mais baixo que nao esta aberto no momento.

    O parametro flags deve incluir um dos seguintes modos de acesso: 
    ------>FS O RDONLY, 
    ------>FS O WRONLY,
    ------>FS O RDWR.

    Eles solicitam a abertura do arquivo somente leitura, somente gravacao ou leitura/gravacao, respectivamente.
    As constantes sao definidas no arquivo common.h.
    Abrir um arquivo retorna um novo descritor de arquivo ou -1 se ocorreu um erro.
    Se um arquivo inexistente for aberto para gravação, ele devera ser criado. Uma tentativa de abrir um
    arquivo inexistente somente para leitura deve falhar.
    Para facilitar sua vida, assumimos que o nome do arquivo (filename) passado para as chamadas de
    sistema pode ser apenas “.”, “..”, um diretório ou um nome de arquivo no diretorio atual. Portanto, voce nao
    precisa analisar o caminho com nomes de diretorio e arquivo separados por “/”. Voce tambem pode supor
    que o comprimento do nome do arquivo (e do nome do diretorio) seja menor que 32 bytes (MAX FILE NAME).
    Essas suposicoes permanecem as mesmas para as funcoes seguintes.
    E considerado um erro abrir um diretório em qualquer modo alem do  FS O RDONLY.
    Voce pode usar uma tabela de descritor de arquivo compartilhada. Voce nao precisa se preocupar com
    gerenciamento de usuarios ou listas de controle de acesso.
*/


int fs_open(char *fileName, int flags) 
{
    inode_t* inode = search_filename(fileName);

    // Uma tentativa de abrir um arquivo inexistente somente para leitura deve falhar.
    if(inode == NULL && flags == FS_O_RDONLY)
        return -1;
    // Se um arquivo inexistente for aberto para gravação, ele devera ser criado
    if(inode == NULL) 
    {
        inode = retrieve_inode(PWD);
        int unusedBlockIndex = find_unused_block();
        int lastUsedIndex = inode->size;
        int fileNameSize = strlen(fileName);

        // fileName[fileNameIndex]
        int fileNameIndex = 0;
        for(int i = lastUsedIndex; i < lastUsedIndex + fileNameSize; i++)
        {
            // se i == 101, não vai caber
            if(i == 101)
                return -1;
            
            inode->data[i] = fileName[fileNameIndex++];
        }
        inode_t* newNode = find_empty_inode();
        if(newNode == NULL)
            return -1;

        newNode->type = FILE_TYPE;
        newNode->links = 1;
        newNode->size = 0;
        newNode->numBlocks = 0;
        newNode->dot = newNode->inodeNo;
        newNode->dotdot = inode->inodeNo;

        //convert newNode->inodeNo to char[4] and insert into inode->data[]
        char intToCharArray[4] = &newNode->inodeNo;
        int charArrIndex = 0 ;
        
        lastUsedIndex = lastUsedIndex + fileNameSize;
        for(int i = lastUsedIndex; i < lastUsedIndex + sizeof(int)/* 4 */; i++)
            inode->data[i] = intToCharArray[charArrIndex++];

        inode->numBlocks++;
        inode->size += fileNameSize + 4;
        mark_block_as_occupied(unusedBlockIndex);
        inode = newNode;
    }
    // E considerado um erro abrir um diretório em qualquer modo alem do FS_O_RDONLY.
    if(inode->type == DIRECTORY && flags != FS_O_RDONLY)
        return -1;

    inode->links++;
    // aloca espaço para o file descriptor
    fd_t* file_descriptor = (fd_t*) malloc(sizeof(fd_t));
    // seta valores do file descriptor criado
    file_descriptor->seek_ptr = 0;
    file_descriptor->inode = inode;
    file_descriptor->hasNoLinks = 0;
    // coloca o file descriptor na tabela de descritor, não precisamos se preocupar em sobrescrever outra tabela
    // foi estipulado na descrição que apenas um programa vai abrir o arquivo por vez
    file_table[inode->inodeNo] = file_descriptor;
    return inode->inodeNo;
}


int fs_close( int fd) 
{
    /**
    *   TODO: int fs close(int fd);
    *   Fecha um descritor de arquivo, para que ele nao se refira mais a
    *   nenhum arquivo e possa ser reutilizado. Retorna zero em caso de sucesso e -1 em caso de falha. Se o
    *   descritor foi a ultima referencia a um arquivo que foi removido usando a desvinculacao (unlink)
    *   o arquivo sera excluıdo.
    */
    inode_t* inode = file_table[fd]->inode;

    // o arquivo foi deletado, liberamos a memória associada a ele
    if(file_table[fd]->hasNoLinks)
        inode->type = FREE_INODE;
        
    block_write(inode->inodeNo + 1, inode);
    
    free(file_table[fd]);
    file_table[fd] = NULL;

    return 0;
}

int find_unused_block()
{
    char* aux;
    block_read(INODE_BLOCKS + 1, aux);
    int firstDataBlockIndex = INODE_BLOCKS + 2;

    int blockDisplacement = 0;

    for(int i = 0 ; i < (MAX_IMAGE_SIZE - FS_SIZE)/8; i++)
    {
        char x = 1;
        for(int i = 0 ; i < 8; i++)
        {
            if(aux[i] & x)
                return firstDataBlockIndex + blockDisplacement;
            blockDisplacement++;
            x = x << 1;
        }

    }

    return -1;
}

void mark_block_as_occupied(int block)
{
    char* aux;
    block_read(INODE_BLOCKS + 1, aux);
    int firstDataBlockIndex = INODE_BLOCKS + 2;
    block -= firstDataBlockIndex;
    
    char x = 1 << block;
    // checa se o bloco já não está ocupado
    if(aux[block/8] & x) 
    {
        // se entrou aqui, o bloco ainda não foi marcado como ocupado
        aux[block/8] = aux[block/8] ^ x; // bitwise xor
        block_write(INODE_BLOCKS + 1, aux);
    }
}
    
int fs_read( int fd, char *buf, int count) {
    /**
    *   TODO: int fs read(int fd, char *buf, int count);
    *   fs read() tenta ler ate (count) bytes do descritor de arquivo fd no buffer,
    *   indicado por buf. Se count for zero, fs read() retornara zero e nao executa nenhuma outra acao. Em
    *   caso de sucesso, o numero de bytes lidos com exito e retornado e a posicao do arquivo e avancada por esse
    *   numero. Nao e um erro se esse numero for menor que o numero de bytes solicitados; isso pode acontecer,
    *   por exemplo, porque menos bytes estao disponıveis no momento. Em caso de erro, -1 e retornado. No caso
    *   de erro, nao e necessario alterar ponteiro de deslocamento do arquivo.
    */

    // usuário pediu pra ler 0 bytes 😑
    if(count == 0) 
        return 0;
    // não tem file descriptor para arquivo requisitado pelo usuário (esqueceu de dar open?)
    if(file_table[fd] == NULL)
        return -1;
    
    inode_t* inode = file_table[fd]->inode;

    // tentando ler um diretorio??
    if(inode->type == DIRECTORY)
        return 0;
    // bytes disponiveis = tamanho arquivo - bytes já lidos
    int availableBytes = inode->size - file_table[fd]->seek_ptr;
    int blocksNeeded = count/BLOCK_SIZE;

    buf = (char*) malloc(count);
    int readByteCounter = 0;
    for(int i = 0; i < blocksNeeded; i++)
    {
        int blockIndex;
        //transforma char[4] pra int
        memcpy(&blockIndex, inode->data[i * 4], sizeof(int));

        char* aux;
        block_read(blockIndex, aux);
        int currentByteCounter = 0;
        while(readByteCounter < count && currentByteCounter < BLOCK_SIZE)
        {
            buf[readByteCounter++] = aux[currentByteCounter++];
        }
    }
    // atualiza ponteiro do file descriptor
    file_table[fd]->seek_ptr += readByteCounter;

    return readByteCounter;
}
    
int fs_write( int fd, char *buf, int count) {
    /**
    *   TODO: int fs write(int fd, char *buf, int count);
    *   Descrição da função: fs write() grava count bytes no arquivo referenciado pelo descritor de arquivo
    *   fd do buffer indicado por buf.
    *   Em caso de sucesso, o número de bytes gravados de buf é retornado (um número menor que count
    *   pode ser retornado) e a posição do arquivo é avançada por esse número. Em caso de erro, -1 é retornado.
    *   E um erro tentar gravar em um ponto além do tamanho máximo de um arquivo. ´ E um erro se a contagem ´
    *   for maior que zero, mas nenhum byte for gravado.
    *   Um arquivo de tamanho zero não deve ocupar nenhum bloco de dados.
    *   Se count for zero, 0 será retornado sem causar outros efeitos.
    */

     // usuário pediu pra escrever 0 bytes 😑
    if(count == 0) 
        return 0;
    // não tem file descriptor para arquivo requisitado pelo usuário (esqueceu de dar open?)
    if(file_table[fd] == NULL)
        return -1;

    inode_t* inode = file_table[fd]->inode;

    // tentando escrever em um diretorio??
    if(inode->type == DIRECTORY)
        return -1;

    int blocksNeeded = count / BLOCK_SIZE;
    
    int startingBlock = file_table[fd]->seek_ptr / BLOCK_SIZE;
    int startingByte =  file_table[fd]->seek_ptr % BLOCK_SIZE;

    int lastBlock = startingBlock + blocksNeeded; 

    // 23 == ultimo indice possivel para guardar o role ai e pa
    // É um erro tentar gravar em um ponto além do tamanho máximo de um arquivo
    if(lastBlock >= 23)
        return -1;

    int writtenByteCounter = 0;
    for(int i = startingBlock; i < lastBlock; i++)
    {
        int blockIndex;
         char* aux;

        // verifica se bloco ainda não foi alocado
        if(startingBlock >= file_table[fd]->inode->numBlocks)
        {
            int unusedBlockIndex = find_unused_block();
            mark_block_as_occupied(unusedBlockIndex);
            char intToCharArray[4] = &unusedBlockIndex;
        
            for(int j = 0; j < 4 ; j++)
                inode->data[(i*4) + j] = intToCharArray[j];
            // não atualiza inode agora, apenas escreve quando fechar o file descriptor
        }
        
        //transforma char[4] pra int
        memcpy(&blockIndex, inode->data[i * 4], sizeof(int));
        block_read(blockIndex, aux);
        
        int currentBlockCounter = 0;
        while(writtenByteCounter < count && currentBlockCounter < BLOCK_SIZE)
        {
            aux[currentBlockCounter++] = buf[writtenByteCounter++];
        }
        block_write(blockIndex, aux);
    }

    file_table[fd]->seek_ptr += writtenByteCounter;

    // quando escreveu dados além do espaço previamente alocado
    if(file_table[fd]->seek_ptr > file_table[fd]->inode->size) 
    {
        // atualiza espaço ocupado pelo arquivo
        inode->size = file_table[fd]->seek_ptr;

        // precisamos verificar se esses novos dados ocuparam blocos novos
        inode_t* inode = file_table[fd]->inode;
        int previouslyAllocatedSpace = inode->numBlocks * BLOCK_SIZE;

        // se nova quantidade de dados é maior que espaço alocado anteriormente
        if(file_table[fd]->seek_ptr > previouslyAllocatedSpace)
        {
            inode->numBlocks = inode->size/BLOCK_SIZE;
        } 
    }
    
    return 0;
}

int fs_lseek( int fd, int offset) {
    /**
    *   TODO: int fs lseek(int fd, int offset);
    *   Descrição da função: A função fs lseek() reposiciona o deslocamento do arquivo aberto associado ao
    *   descritor de arquivo fd para o deslocamento (offset) do argumento.
    *   A função fs lseek() permite que o deslocamento do arquivo seja definido além do final do arquivo
    *   (mas isso não altera o tamanho do arquivo). Se os dados forem gravados posteriormente nesse deslocamento,
    *   o arquivo será preenchido com ’\0’ no espaço intermediário.
    *   Após a conclusão bem-sucedida, fs lseek() retorna o local de deslocamento resultante conforme
    *   medido em bytes desde o início do arquivo. Caso contrário, o valor -1 será retornado.
    */

    file_table[fd]->seek_ptr += offset;
    // TODO: ADICIONAR LOGICA DE PREENCHER '\0' NO ESPAÇO INTERMEDIARIO
    return 0;
}

int fs_mkdir( char *fileName) {
    /**
    *   TODO: int fs mkdir(char *dirname);
    *   Descrição da função: fs mkdir() tenta criar um diretório chamado dirname. Retorna zero em caso de
    *   sucesso ou -1 se ocorreu um erro. fs mkdir() deve falhar se o diretório dirname já existir.
    *   Novos diretórios devem conter entradas “.” e “..”. E um erro tentar criar um diretório sem eles.
    */

    inode_t* inode = retrieve_inode(PWD);
    int unusedBlockIndex = find_unused_block();
    int lastUsedIndex = inode->size;
    int fileNameSize = strlen(fileName);

    // fileName[fileNameIndex]
    int fileNameIndex = 0;
    for(int i = lastUsedIndex; i < lastUsedIndex + fileNameSize; i++)
    {
            // se i == 101, não vai caber
        if(i == 101)
            return -1;
            
        inode->data[i] = fileName[fileNameIndex++];
    }
    inode_t* newNode = find_empty_inode();
    if(newNode == NULL)
        return -1;

    newNode->type = FILE_TYPE;
    newNode->links = 1;
    newNode->size = 0;
    newNode->numBlocks = 0;
    newNode->dot = newNode->inodeNo;
    newNode->dotdot = inode->inodeNo;

    //convert newNode->inodeNo to char[4] and insert into inode->data[]
    char intToCharArray[4] = &newNode->inodeNo;
    int charArrIndex = 0 ;
        
    lastUsedIndex = lastUsedIndex + fileNameSize;
    for(int i = lastUsedIndex; i < lastUsedIndex + sizeof(int)/* 4 */; i++)
        inode->data[i] = intToCharArray[charArrIndex++];

    inode->numBlocks++;
    inode->size += fileNameSize + 4;
    mark_block_as_occupied(unusedBlockIndex);
    
    return 0;
}

int fs_rmdir( char *fileName) {
    /**
    *   TODO: int fs rmdir(char *dirname);
    *   Descrição da função: fs rmdir() exclui um diretório que deve estar vazio. Em caso de sucesso, zero é
    *   retornado; em caso de erro, -1 é retornado (por exemplo, tentando excluir um diretório não vazio).
    */
    inode_t* inode = search_filename(fileName);

    // tentando deletar um diretório que não existe??
    if(inode == NULL)
        return -1;
    
    // não é um diretorio
    if(inode->type == FILE_TYPE)
        return -1;
    
    // tentando deletar um diretório não vazio
    if(inode->numBlocks > 0)
        return -1;
    
    inode->type = FREE_INODE;
    /**
    *   TODO: ATUALIZAR INODE APONTADO POR PWD (precisa tirar o campo que apontava pro diretorio excluido)
    *       No caso é só chamar unlink?????                     - sim.
    */
    fs_unlink(inode->inodeNo);
    return 0;
}

int fs_cd( char *dirName) {
    /**
    *   TODO: int fs_cd(char *dirname);
    *   Descrição da função: fs_cd() altera o diretório atual para o especificado em dirname. Em caso de
    *   sucesso, zero é retornado. Em caso de erro, -1 é retornado.
    */
    inode_t* inode = search_filename(dirName);
    if(inode == NULL)
        return -1;
    if(inode->type == FILE_TYPE)    
        return -1;
    
    PWD = inode->inodeNo;
    return 0;
}


int 
fs_link( char *old_fileName, char *new_fileName) {
    /**
    *   TODO: int fs link(char *oldpath, char *newpath);
    *   Descrição da função: fs link() cria um novo link (também conhecido como link físico) para um
    *   arquivo existente com o nome oldpath. Se já existir um newpath, ele não será substituído. O novo nome
    *   pode ser usado exatamente como o antigo para qualquer operação; os dois nomes se referem ao mesmo
    *   arquivo e é impossível dizer qual nome era o “original”.
    *   Em caso de sucesso, zero é retornado. Em caso de erro, -1 é retornado. E um erro usar esta função em ´
    *   um diretório.
    *   Observe que, como não existem “caminhos” além do diretório atual, o pai ou o diretório filho, oldpath
    *   e newpath são na verdade ambos nomes de arquivos e só podem estar no mesmo diretório
    */

    inode_t* linkedInode = search_filename(old_fileName);
    return -1;
}


int fs_unlink(int inodeNo) {
    /**
    *   TODO: int fs unlink(int inodeNo);
    *   Descrição da função: fs unlink() exclui um nome do sistema de arquivos. Se esse nome foi o último
    *   link para um arquivo e nenhum processo o abriu, o arquivo será excluído e o espaço usado estará disponível
    *   para reutilização.
    *   Se o nome for o último link para um arquivo, mas ele ainda estiver aberto em um descritor de arquivo
    *   existente, ele permanecerá até que o último descritor de arquivo referente a ele seja fechado.
    *   Em caso de sucesso, zero é retornado. Em caso de erro, -1 é retornado. E um erro usar esta função em ´
    *   um diretório.
    */
    inode_t* inode = retrieve_inode(PWD);
     
    char newData[105];
    int newDataIndex;

    int numBlocks = inode->numBlocks;
    int newNumBlocks = 0;
    int blocksFound = 0;
    
    int referenceStart = 0;
    int i = 0;

    /* 
        Laço vai iterando até achar um \0
        achando um '\0', sabemos que os proximos 4 bytes representam numero do proximo inode
    */
    while(blocksFound < numBlocks)
    {  
        // acabou a string do nome do diretorio/arquivo
        if(inode->data[i] == '\0')
        {
            blocksFound++;
            int index;
            // constroi int a partir do char[4]
            memcpy(&index, inode->data[i+1], sizeof(int));
            i += 4;
            // se o proximo ponteiro não for correspondente ao numero do arquivo a ser excluido
            if(index != inodeNo)
            {
                // incrementa a quantidade de blocos
                newNumBlocks++;
                for(int j = referenceStart; j <= i; j++)
                    newData[newDataIndex++] = inode->data[j];
                referenceStart = i + 1;
            }
        }
        i++;
    }

    inode->numBlocks = newNumBlocks;
    memcpy(inode->data, newData, 105);
    // salva mudanças do diretório pai no disco
    block_write(inode->inodeNo + 1, inode);


    // se o usuário estiver com o arquivo aberto
    // não podemos deletar o arquivo de fato ainda
    // isso só vai ser feito quando o file descriptor for fechado
    if(file_table[inodeNo] == NULL)
    {
        inode_t* deletedInode = retrieve_inode(inodeNo);
        deletedInode->type = FREE_INODE;
        block_write(inodeNo + 1, deletedInode);
    }
    else
    {
        file_table[inodeNo]->hasNoLinks = 1;
    }
    return 0;
}

int fs_stat( char *fileName, fileStat *buf) {
    /**
     * TODO: int fsstat(char *filename, fileStat *buf);
     * Descrição da função: fsstat() retorna informações sobre um arquivo.  
     * Ela retorna uma estrutura fileStat(definida emcommon.h), que contém os seguintes campos:
     * typedef struct{
     *  int inodeNo; // número do i-node do arquivo
     *  short type;  // tipo do i-node doarquivo: DIRECTORY, FILE_TYPE (há outro valor FREE_INODE que nunca aparece aqui
     *  char links;  // número de links para o i-node
     *  int size;    // tamanho do arquivo em byte
     *  int numBlocks; // número de blocos usados pelo arquivo
     * } fileStat;
     * Não reutilize essa estrutura para seus inodes.
     * Nota: se sua implementação precisar de membros diferentes, você poderá modificar essa estrutura, 
     * mas adicione-a apenas e atualize os membros que já estão lá. 
     */
    return -1;
}
