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
/**
    TODA VEZ QUE FOR GUARDAR UM INODE, TEM QUE SALVAR OS OUTROS NO BLOCO TAMBÉM
    UM INODE FICA EM UM BLOCO COM OUTROS 3
*/

inode_t* create_inode(int number)
{
    inode_t* newNode = (inode_t*) malloc(sizeof(inode_t));
    newNode->inodeNo = number;
    newNode->type = FREE_INODE;  
    newNode->links = 0; 
    newNode->size = 0;  
    newNode->numBlocks = 0; 
    newNode->dot = number; 
    newNode->dotdot = number;

    return newNode;
}

inode_t* retrieve_inode(int index)
{
    char* aux = (char*) malloc(BLOCK_SIZE);
    block_read((index/4) + 1, aux); // lê bloco relacionado ao indice DO INODE (inode_t->inodeNo)
    inode_t* inode = (inode_t*)&aux[(index % 4) * sizeof(inode_t)]; // busca inode correto dentro dos 4 inodes presentes do bloco
    return inode;
}

void save_inode(inode_t* inode, int index)
{
    char* aux = (char*) malloc(BLOCK_SIZE);
    block_read((index/4) + 1, aux); // lê bloco relacionado ao indice DO INODE (inode_t->inodeNo)
    memcpy(&aux[(index % 4) * sizeof(inode_t)], inode, sizeof(inode_t));
    block_write((index/4) + 1, aux);
}

void fs_init( void) 
{
    block_init();
    char* aux = (char*)malloc(BLOCK_SIZE);

    block_read(0, aux);
    superblock = (superblock_t*)aux;
    PWD = 0;
    //printf("got %lld  ", superblock->magic_number);
    if(superblock->magic_number != MAGIC_NUMBER)
    {
        superblock->magic_number = MAGIC_NUMBER;
        block_write(0,(char*)superblock);
        fs_mkfs(); // formata disco
    }

    for (fd_t** ptr = file_table; ptr != file_table + 2044; ptr++)
        *ptr = NULL;
}



int fs_mkfs(void) 
{
    char* aux;
    
    int inode_counter = 0;
    inode_t* inode[4];

    for(int i = 1; i <= INODE_BLOCKS; i++) // percorre todo os inodes do sistema de arquivos
    {
        // cria 4 inodes que preenchem o bloco
        inode[0] = create_inode(inode_counter++);
        inode[1] = create_inode(inode_counter++);   
        inode[2] = create_inode(inode_counter++);   
        inode[3] = create_inode(inode_counter++);
        // aux recebe os quatro inodes
        aux = (char*) malloc(BLOCK_SIZE);
        
        memcpy(&aux[sizeof(inode_t) * 0], inode[0], sizeof(inode_t));
        memcpy(&aux[sizeof(inode_t) * 1], inode[1], sizeof(inode_t));
        memcpy(&aux[sizeof(inode_t) * 2], inode[2], sizeof(inode_t));
        memcpy(&aux[sizeof(inode_t) * 3], inode[3], sizeof(inode_t));
        // salvamos o bloco
        block_write(i,aux);
    }

    // aloca mapa de alocação (bitmasks de blocos de dados)
    aux = (char*) malloc(BLOCK_SIZE);
    // preenche com zeros
    memset(aux, 0xFF, BLOCK_SIZE);
    // guarda no disco
    block_write(INODE_BLOCKS + 1, aux);
    
    inode_t* root = retrieve_inode(PWD);
    root->type = DIRECTORY;
    save_inode(root, 0);

    return 0;
}

inode_t* search_filename(char *fileName)
{
    inode_t* inode = retrieve_inode(PWD);
    int fileNameSize = strlen(fileName);
    
    if(inode->type == DIRECTORY)
    {
        if(strcmp(fileName, ".") == 0)
        {
            inode_t* dot = retrieve_inode(inode->dot);
            return dot; 
        }
        if(strcmp(fileName, "..") == 0)
        {
            inode_t* dotdot = retrieve_inode(inode->dotdot);
            return dotdot; 
        }
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
                // -----------> o condicional if(inode->data[dataIndex] == '\0') dentro do else vai ser ativado
                // -----------> damos break e tentamos novamente com a proxima string, se tiver
                
                // strings diferentes, fileName tem tamanho menor
                // -----------> eh possivel entrar no condicional de quando fileName terminar em '\0'. (um eh substring do outro)
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
                    // inode->data = "abcd"
                    if(inode->data[dataIndex] == '\0') // encontrou a string correta
                    {
                        // deu bom
                        int index;
                        // constroi int a partir do char[4]
                        memcpy(&index, &inode->data[dataIndex + 1], sizeof(int));
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
        //printf("NOT A DIRECTORY, SOMEHOW\n"); print de debug
        // função precisa buscar o arquivo dentro de um diretório.
        // idealmente PWD nunca apontará para um arquivo... eu sou burro
        return NULL;
    }
}

inode_t* find_empty_inode() // SOLUÇÃO O(N) bem ruim mas dá pro gasto
{
    // procura todos os inodes sequencialmente até achar um livre
    for(int i = 1; i <= INODE_BLOCKS * 4; i++)
    {
        inode_t* inode = retrieve_inode(i);
        if(inode->type == FREE_INODE)
            return inode;
    }

    return NULL;
}

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
        int fileNameSize = strlen(fileName) + 1;

        // fileName[fileNameIndex]
        int fileNameIndex = 0;
        for(int i = lastUsedIndex; i < lastUsedIndex + fileNameSize; i++)
        {
            // se i == 101, não vai caber. sendo que o campo data de cada inode possui 105 bytes.
            if(i == 101)
                return -1;
            
            inode->data[i] = fileName[fileNameIndex++];
        }
        // encontramos um inode marcado como vazio para utilizar
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
        char* intToCharArray = (char*)&newNode->inodeNo;
        int charArrIndex = 0 ;
        
        lastUsedIndex = lastUsedIndex + fileNameSize;
        for(int i = lastUsedIndex; i < lastUsedIndex + sizeof(int)/* 4 */; i++)
            inode->data[i] = intToCharArray[charArrIndex++];

        inode->numBlocks++;
        inode->size += fileNameSize + 4;
        mark_block_as_occupied(unusedBlockIndex);
        save_inode(inode, inode->inodeNo);
        save_inode(newNode, newNode->inodeNo);
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
    if(file_table[fd] == NULL)
        return -1;

    // buscamos o inode na tabela hash de tabela de arquivos
    // como o mapeamento é 1 pra 1, usamos como função hash, o inodeNo de um determinado inode.
    inode_t* inode = file_table[fd]->inode;
    inode->links--;

    // o arquivo foi deletado, liberamos a memória associada a ele
    if(file_table[fd]->hasNoLinks)
        inode->type = FREE_INODE;
    
    // guarda inode no disco
    save_inode(inode, inode->inodeNo);

    free(file_table[fd]);
    file_table[fd] = NULL;

    return 0;
}

int find_unused_block()
{
    // essa função se responsabiliza de encontrar um bloco DE DADOS vazio
    // buscamos isso no bitmap presente no bloco seguinte dos inodes no disco
    // retorna o indice do bloco para quem chamou a função
    char* aux = (char*) malloc(BLOCK_SIZE);
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
    char* aux = (char*) malloc(BLOCK_SIZE);
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
    int blocksNeeded = (count / BLOCK_SIZE) + 1;

    int readByteCounter = file_table[fd]->seek_ptr;
    for(int i = 0; i < blocksNeeded; i++)
    {
        int blockIndex;
        //transforma char[4] pra int
        memcpy(&blockIndex, &inode->data[i * 4], sizeof(int));

        char* aux = (char*) malloc(BLOCK_SIZE);
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

    int blocksNeeded = (count / BLOCK_SIZE) + 1;
    
    int startingBlock = file_table[fd]->seek_ptr / BLOCK_SIZE;
    int startingByte =  file_table[fd]->seek_ptr % BLOCK_SIZE;

    int lastBlock = startingBlock + blocksNeeded; 

    // 23 == ultimo indice possivel para guardar o role ai e pa
    // É um erro tentar gravar em um ponto além do tamanho máximo de um arquivo
    if(lastBlock >= 23)
        return -1;

    int writtenByteCounter = file_table[fd]->seek_ptr;
    for(int i = startingBlock; i < lastBlock; i++)
    {
        int blockIndex;
        char* aux = (char*) malloc(BLOCK_SIZE);

        // verifica se bloco ainda não foi alocado
        if(startingBlock >= file_table[fd]->inode->numBlocks)
        {
            int unusedBlockIndex = find_unused_block();
            mark_block_as_occupied(unusedBlockIndex);
            char* intToCharArray = (char*)&unusedBlockIndex;
        
            for(int j = 0; j < 4 ; j++)
                inode->data[(i*4) + j] = intToCharArray[j];
            // não atualiza inode agora, apenas escreve quando fechar o file descriptor
        }
        
        //transforma char[4] pra int
        memcpy(&blockIndex, &inode->data[i * 4], sizeof(int));
        block_read(blockIndex, aux);
        
        int currentBlockCounter = 0;
        while(writtenByteCounter < count && currentBlockCounter < BLOCK_SIZE)
        {
            aux[currentBlockCounter++] = buf[startingByte++];
            writtenByteCounter++;
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
        int previouslyAllocatedSpace = inode->numBlocks * BLOCK_SIZE;

        // se nova quantidade de dados é maior que espaço alocado anteriormente
        if(file_table[fd]->seek_ptr > previouslyAllocatedSpace)
        {
            inode->numBlocks = inode->size/BLOCK_SIZE;
        }
        save_inode(inode, inode->inodeNo);
    }
    
    return 0;
}

int fs_lseek( int fd, int offset) {
    file_table[fd]->seek_ptr += offset;
    // TODO: ADICIONAR LOGICA DE PREENCHER '\0' NO ESPAÇO INTERMEDIARIO
    // não deu tempo professor :(, desculpa
    return 0;
}

int fs_mkdir( char *fileName) {
    inode_t* inode = retrieve_inode(PWD);
    int unusedBlockIndex = find_unused_block();
    int lastUsedIndex = inode->size;
    int fileNameSize = strlen(fileName) + 1;

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

    newNode->type = DIRECTORY;
    newNode->links = 1;
    newNode->size = 0;
    newNode->numBlocks = 0;
    newNode->dot = newNode->inodeNo;
    newNode->dotdot = inode->inodeNo;

    //convert newNode->inodeNo to char[4] and insert into inode->data[]
    char* intToCharArray = &newNode->inodeNo;
    int charArrIndex = 0 ;
        
    lastUsedIndex = lastUsedIndex + fileNameSize;
    for(int i = lastUsedIndex; i < lastUsedIndex + sizeof(int)/* 4 */; i++)
        inode->data[i] = intToCharArray[charArrIndex++];

    inode->numBlocks++;
    inode->size += fileNameSize + 4;

    save_inode(newNode, newNode->inodeNo);
    save_inode(inode, inode->inodeNo);
    mark_block_as_occupied(unusedBlockIndex);
    
    return 0;
}

int fs_rmdir( char *fileName) {
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

    fs_unlink(inode->inodeNo);
    return 0;
}

int fs_cd( char *dirName) {
    // buscamos pelo inode do diretório especificado pelo usuário
    inode_t* inode = search_filename(dirName);

    // esse diretório não existe, ou não está na pasta apontada por PWD
    if(inode == NULL)
        return -1;
    // não é um diretório
    if(inode->type == FILE_TYPE)    
        return -1;
    
    // atualizamos PWD com o diretório requisitado
    PWD = inode->inodeNo;
    return 0;
}


int 
fs_link( char *old_fileName, char *new_fileName) {
    inode_t* linkedInode = search_filename(old_fileName);
    // não encontrou inode com o nome antigo
    if(linkedInode == NULL)
        return -1;
    else if(linkedInode->type == DIRECTORY)
        return -1;
    inode_t* linkedInodeNew = search_filename(new_fileName);
    // link com mesmo "novo nome" já existe
    if(linkedInodeNew != NULL)
        return -1;
    
    inode_t* inode = retrieve_inode(PWD);
    int numBlocks = inode->numBlocks;
    int dataIndex = inode->size;

    /*
        Leandro, vc eh burro - assinado: Leandro
        esse laço inteiro poderia ser resolvido com UMA, repito.. UMA LINHA
        ----------------> int dataIndex = inode->size; <-----------------

    // iteramos por referencias de inodes
    while(foundBlocks < numBlocks && dataIndex < 105)
    {
        // '\0' indica o fim de uma string, os proximos 4 bytes é um número identificando o inode.
        if(inode->data[dataIndex++] == '\0')
        {
            dataIndex += 4;
            foundBlocks++;
        }
    }*/

    int newFileSize = strlen(new_fileName) + 1;

    // verificamos se podemos inserir de modo que caiba no inode.
    if(dataIndex + newFileSize + 3 < 105)
    {
        for(int i = 0; i < newFileSize; i++)
            inode->data[dataIndex++] = new_fileName[i];
        
        char* intToCharArr = &linkedInode->inodeNo;

        for(int i = 0; i < 4; i++)
            inode->data[dataIndex++] = intToCharArr[i];
        

        linkedInode->links++;
        inode->numBlocks++;
        inode->size = dataIndex;
        save_inode(linkedInode, linkedInode->inodeNo);
        save_inode(inode, inode->inodeNo);
        return 0;
    }
    else return -1;
    
}


int fs_unlink(int inodeNo) {
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
            memcpy(&index, &inode->data[i+1], sizeof(int));
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
    inode->size = newDataIndex;
    memcpy(inode->data, newData, 105);
    // salva mudanças do diretório pai no disco
    save_inode(inode, inode->inodeNo);

    // se o usuário estiver com o arquivo aberto
    // não podemos deletar o arquivo de fato ainda
    // isso só vai ser feito quando o file descriptor for fechado
    if(file_table[inodeNo] == NULL)
    {
        inode_t* deletedInode = retrieve_inode(inodeNo);
        deletedInode->type = FREE_INODE;
        save_inode(deletedInode, deletedInode->inodeNo);
    }
    else
    {
        file_table[inodeNo]->hasNoLinks = 1;
    }
    return 0;
}

int fs_stat(char *fileName, fileStat *buf) {
    inode_t* inode = search_filename(fileName);
    if(inode == NULL)
        return -1;
    //printf("inode no == %d\n", inode->inodeNo);
    buf->inodeNo = inode->inodeNo;
    buf->type = inode->type;
    buf->links = inode->links;
    buf->size = inode->size;
    buf->numBlocks = inode->numBlocks;

    return 0;
}

fileStat* fs_ls(int* fileQuantity, char*** nameMatrix)
{    
    // buscamos o inode do diretório atual
    inode_t* inode = retrieve_inode(PWD);
    // informamos a shell quantos arquivos tem na pasta
    *fileQuantity = inode->numBlocks; 

    // alocamos espaço suficiente para o fileStat de todos os arquivos
    fileStat* output = (fileStat*) malloc(sizeof(fileStat) *  inode->numBlocks);
    // alocamos espaço suficiente para cada LINHA da matriz de nomes correspondente a um arquivo/diretório
    *nameMatrix = (char**)malloc(sizeof(char**) * inode->numBlocks );

    int filesFound = 0;
    int startingIndex = 0;
    int dataIndex = 0;
    while(filesFound < *fileQuantity)
    {
        // '\0' indica que acabou a string identificadora de um arquivo/diretório no inode
        if(inode->data[dataIndex++] == '\0')
        {
            (*nameMatrix)[filesFound] = (char*)malloc(dataIndex - startingIndex); 
            char* fileName = (*nameMatrix)[filesFound];
            int fileNameIndex = 0;

            for(int i = startingIndex; i < dataIndex; i++)
                fileName[fileNameIndex++] = inode->data[i];
            inode_t* foundInode = search_filename(fileName);

            output[filesFound].inodeNo = foundInode->inodeNo;
            output[filesFound].type = foundInode->type;
            output[filesFound].links = foundInode->links;
            output[filesFound].size = foundInode->size;
            output[filesFound].numBlocks = foundInode->numBlocks;

            startingIndex = dataIndex + 4;
            dataIndex = startingIndex;
            filesFound++;
        }
    }
    return output;
}