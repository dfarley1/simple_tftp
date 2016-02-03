/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 1
 * Client.  Run as "./myclient <server-info.text> <num connections>"
 */

#include "myunp.h"

typedef struct {
    char **IPs;
    unsigned int *ports;
    int num;
    int linesAlloced;
} servers_t;

typedef struct {
    char *filename;
    int start, size, thread_id;
} tInfo_t;

servers_t *servers;

void readServerList(FILE *serversFile);
void freeServerList();
int getFileSize(char *filename);
void downloadFile(char *filename, int fileSize);
void *childDownloader (void *threadInfo);

int main(int argc, char **argv)
{
    int numConns = 0, fileSize;
    char filename[MAXLINE];
    FILE *serversFile;
    
    //arg checking
    //printf("checking args...\n");
    if (argc != 3) {
        printf("  Usage: myclient <server-info.text> <num connections>\n\n");
        exit(1);
    }
    
    if (sscanf(argv[2], "%d", &numConns) != 1) {
        printf("  Error: Invalid num connections\n\n");
        exit(2);
    }
    
    //printf("trying to fopen(%s)...\n", argv[1]);
    if ((serversFile = fopen(argv[1], "r")) == NULL) {
        printf("  Error opening server-info.text: %d: %s\n\n", errno, strerror(errno));
        exit(3);
    } 
    
    //Get the list of servers
    //printf("starting read....\n");
    readServerList(serversFile);
    fclose(serversFile);
    
    servers->num = (servers->num > numConns)?(numConns):(servers->num);
    
    printf("  num=%d\n", servers->num);
    int i;
    for (i = 0; i < servers->num; i++) {
        printf("  %s:%d\n", servers->IPs[i], servers->ports[i]);
    }
    
    fileSize = getFileSize(filename);
    if (fileSize < 1) {
        exit(4);
    }
    
    downloadFile(filename, fileSize);
    
    
    freeServerList();
    return 0;
}

//Read server IPs/ports into string array
//Modified from https://stackoverflow.com/a/19174415
void readServerList(FILE *serversFile)
{
    int max_line_len = 30;
    
    servers = calloc(1, sizeof(servers_t));
    servers->linesAlloced = 4;
    servers->num = 0;
    
    servers->IPs = (char **)calloc(servers->linesAlloced, sizeof(char *));
    if (servers == NULL) {
        printf("  Error allocating memory\n\n");
        exit(1);
    }

    //First part: Read raw lines into strings (servers->IPs for now)
    int i;
    for (i = 0; 1; i++) {
        //printf("i=%d\n", i);
        //Do we need to realloc?
        if (i >= servers->linesAlloced) {
            //if so, double the size and 
            //printf("current=%d, ", servers->linesAlloced);
            servers->linesAlloced *= 2;
            //printf("new=%d|| size=%d\n", servers->linesAlloced, sizeof(char *) * (servers->linesAlloced));
            servers->IPs = (char **)realloc(servers->IPs, sizeof(char *) * (servers->linesAlloced));
            if (servers == NULL) {
                printf("  Error re-allocating memory\n\n");
                exit(1);
            }
            int j;
            for (j = (servers->linesAlloced)/2; j < (servers->linesAlloced); j++) {
                servers->IPs[j] = NULL;
            }
        }
        
        //Allocate space for next server
        servers->IPs[i] = (char *)calloc(max_line_len, sizeof(char));
        if(servers->IPs[i] == NULL) {
            printf("  Error allocating server line\n\n");
            exit(1);
        }
        
        //printf("getting line...\n");
        if ((fgets(servers->IPs[i], max_line_len, serversFile)) == NULL) {
            servers->num = i;
            break;
        }
        //printf("got line: \"%s\"...\n", servers->IPs[i]);
        
        //strip CR, LF characters and insert \0
        int j;
        for (j = strlen(servers->IPs[i])-1; j <= 0 && (servers->IPs[i][j] == '\n' || servers->IPs[i][j] == '\r');  j--)
            ;
        //printf("len = %d\n", j);
        servers->IPs[i][j] = '\0';
        
    }
    
    //Second part: read ports into servers->ports and trim them from servers->IPs
    servers->ports = (unsigned int *)calloc(servers->num, sizeof(unsigned int));
    for (i = 0; i < servers->num; i++) {
        char *port_num_str = strchr(servers->IPs[i], ' ');
        //printf("%d    ", port_num_str - servers->IPs[i]);
        servers->ports[i] = strtoul(port_num_str, NULL, 10);
        *port_num_str = '\0';
    }
}

void freeServerList()
{
    int i;
    for (i = 0; i < servers->linesAlloced; i++) {
        free(servers->IPs[i]);
    }
    free(servers->IPs);
    free(servers->ports);
    free(servers);
}

int getFileSize(char *filename)
{
    int fileSize = 0, sockfd, n;
    char recvline[MAXLINE], sizePacket[MAXLINE];;
    struct sockaddr_in servaddr;
    
    bzero(sizePacket, MAXLINE);
    bzero(filename, MAXLINE);
    bzero(recvline, MAXLINE);
    
    printf("Enter file name: ");
    fgets(filename, MAXLINE/2, stdin);  //should be enough room, right?
    filename[strcspn(filename, "\n")] = 0;
    
    //Try to connect to a server
    int i = 0;
    while (1) {
        //If we've tried all servers then just crash
        if (i >= servers->num) {
            err_sys("getFileSize():  ERROR: No servers reachable, tried %d!\n", i);
        }
        
        bzero(&servaddr, sizeof(servaddr));
        sockfd = Socket(AF_INET, SOCK_STREAM, 0);
        
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(servers->ports[i]);
        if (inet_pton(AF_INET, servers->IPs[i], &servaddr.sin_addr) <= 0) {
            err_quit("getFileSize():  ERROR: inet_pton error for %s\n", servers->IPs[i]);
        }
        
        //If connection is successful then break, if not, i++ and try next one
        if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
            i++;
            continue;
        } else {
            break;
        }
    }
    printf("getFileSize():  Connected to %s:%d\n", servers->IPs[i], servers->ports[i]);
    sprintf(sizePacket, "%d\n%d\n\n%s", -1, -1, filename);
    
    printf("getFileSize():  sending \"%s\"\n", sizePacket);
    Write(sockfd, sizePacket, strlen(sizePacket));
    
    while ((n = Read(sockfd, recvline, MAXLINE-1)) > 0) {
        recvline[n] = 0;
        printf("recieved \"%s\"\n", recvline);
        if (sscanf(recvline, "%d", &fileSize) != 1) {
            printf("getFileSize():  sscanf() ERROR.  Server returned:\n  %s\n\n", recvline);
            exit(4);
        }
    }
    
    printf("getFileSize():  Returning %d\n\n", fileSize);
    return fileSize;
}

void downloadFile(char *filename, int fileSize)
{
    //spawn children
    int rc;
    pthread_t tid[servers->num];
    void *vptr_return[servers->num];
    tInfo_t *tInfo[servers->num];
    
    int i;
    for (i = 0; i < servers->num; i++) {
        tInfo[i] = (tInfo_t *) malloc(sizeof(tInfo_t));
        tInfo[i]->filename = filename;
        tInfo[i]->thread_id = i;
        
        //The last thread needs to pick up the slack if fileSize doesn't divide evenly
        if (i == (servers->num - 1)) { 
            tInfo[i]->start = i * (fileSize / servers->num);
            tInfo[i]->size = (fileSize / servers->num) 
                           + (fileSize - (fileSize / servers->num) * servers->num);
        } else {
            tInfo[i]->start = i * (fileSize / servers->num);
            tInfo[i]->size = fileSize / servers->num;
        }
        printf("Creating thread %d: \"%s\"|%d|%d\n", 
                tInfo[i]->thread_id, tInfo[i]->filename, tInfo[i]->start, tInfo[i]->size);
                
        rc = pthread_create(&tid[i], NULL, childDownloader, (void *) tInfo[i]);
        if (rc) {
            printf("downloadFile(): pthread_create() ERROR: %s\n", strerror(rc));
            exit(5);
        }
    }
    
    //get string pointers from children
    for (i = 0; i < servers->num; i++) {
        pthread_join(tid[i], &vptr_return[i]);
        printf("\n----- %d: -----\n|%s|\n----------\n", i, (char *)vptr_return[i]);
        //free(vptr_return[i]);
    }
    
    //print all children to file (how should it be named?)
    char *newFileName = calloc(strlen(filename) + 3, sizeof(char));
    strcpy(newFileName, filename);
    strcpy(newFileName+strlen(newFileName), "_d");
    
    FILE *fp = fopen(newFileName, "w+");
    if (fp) {
        int i;
        for (i = 0; i < servers->num; i++) {
            char *chunk = (char *)vptr_return[i];
            int j = 0;
            do {
                fputc(chunk[j++], fp);
                if (j >= tInfo[i]->size) break;
            } while (1);
        }
    } else {
        printf("downloadFile(): fopen(%s) ERROR", newFileName);
        exit(6);
    }
    fclose(fp);
    
    free(newFileName);
    for (i = 0; i < servers->num; i++) {
        free(vptr_return[i]);
        free(tInfo[i]);
    }
}

void *childDownloader (void *threadInfo)
{
    char *sizePacket, *recvline, *fileChunk;
    int sockfd = 0, n, tStart = 0, tSize = 0;
    struct sockaddr_in servaddr;
    tInfo_t *tInfo = (tInfo_t *) threadInfo;
    
    setvbuf(stdout,NULL,_IONBF,0);
    printf("Thread %d created: \"%s\"|%d|%d\n",
             tInfo->thread_id, tInfo->filename, tInfo->start, tInfo->size);
    
    //Let's offset the children so they're not all trying to connect at the same time.
    // Does this actually help at all?  Not really sure...
    //usleep(100000*(tInfo->thread_id));
    
    int i = 0;
    while (1) {
        //If we've tried all servers then wait a second and try again
        if (i >= servers->num) {
            usleep(1000000);
            i = 0;
        }
        
        bzero(&servaddr, sizeof(servaddr));
        sockfd = Socket(AF_INET, SOCK_STREAM, 0);
        
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(servers->ports[i]);
        if (inet_pton(AF_INET, servers->IPs[i], &servaddr.sin_addr) <= 0) {
            err_quit("getFileSize():  ERROR: inet_pton error for %s\n", servers->IPs[i]);
        }
        
        //If connection is successful then break, if not, i++ and try next one
        if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
            i++;
            continue;
        } else {
            break;
        }
    }
    
    printf("childDownloader(%d):  connected to %s:%d\n", 
            tInfo->thread_id, servers->IPs[i], servers->ports[i]);
            
    //Send packet requesting chunk
    asprintf(&sizePacket, "%d\n%d\n\n%s", tInfo->start, tInfo->size, tInfo->filename);
    printf("childDownloader(%d):  sending \"%s\"\n", tInfo->thread_id, sizePacket);
    Write(sockfd, sizePacket, strlen(sizePacket));
    
    
    
    //get the chunk
    recvline = calloc(strlen(sizePacket) + tInfo->size + 2, sizeof(char));
    while ((n = Read(sockfd, recvline, strlen(sizePacket) + tInfo->size - 1)) > 0) {
        recvline[n] = 0;
    }
    printf("recieved \"%s\"\n", recvline);
    
    if (sscanf(recvline, "%d\n%d", &tStart, &tSize) != 2) {
        printf("childDownloader(%d):  sscanf() ERROR: Could not get header. Server returned:\n  %s\n\n", 
                tInfo->thread_id, recvline);
        pthread_exit(NULL);
    }
    if ((tStart != tInfo->start) || (tSize != tInfo->size)) {
        printf("childDownloader(%d):  sscanf() ERROR: Sizes don't match.\n  %d  %d || %d  %d\n\n", 
                tInfo->thread_id, tStart, tInfo->start, tSize, tInfo->size);
        pthread_exit(NULL);
    }
    
    fileChunk = calloc(tInfo->size + 1, sizeof(char));
    memcpy(fileChunk, (strstr(recvline, "\n\n") + 2), tInfo->size);
    fileChunk[tInfo->size] = 0;
    
    printf("childDownloader(%d): Chunk:\n%s\n", tInfo->thread_id, fileChunk);
    
    free(recvline);
    free(sizePacket);
    pthread_exit((void *) fileChunk);
}
