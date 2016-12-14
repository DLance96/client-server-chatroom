#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>


//GLOBAL VARIABLES
int sockfd;
int is_terminating = 0;
char exit_string[256] = "/exit";

//METHODS
void *write_to_server();
void *read_from_server();

//OTHER VARIABLES
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

//compare for buffer and another string
int compare(char str1[], char str2[])
{
    int i;
    for(i = 0; i < strlen(str1)-1; i++) //To remove null character from buffer string
    {
        if(str1[i] != str2[i])
        {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int portno, n, pid;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    pthread_t tid1; /* the thread identifiers */
    pthread_t tid2;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
       (char *)&serv_addr.sin_addr.s_addr,
       server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
      error("ERROR connecting");

    printf("> Please enter a username: ");
    bzero(buffer,256);
    fgets(buffer,255,stdin);
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0)
        error("ERROR writing to socket");

    bzero(buffer,256);

    pthread_create(&tid1, NULL, write_to_server, NULL);
    pthread_create(&tid2, NULL, read_from_server, NULL);
    
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    return 0;
}

void *write_to_server()
{
    char buffer[256];
    while(1)
    {
        sleep(1);
        printf("> ");
        bzero(buffer,256);
        fgets(buffer,255,stdin);
        if(buffer[0] == '\n')
        {
            continue;
        }
        else if(compare(buffer,exit_string))
        {
            is_terminating = 1;
            close(sockfd);
            pthread_exit(0);
        }
        write(sockfd,buffer,strlen(buffer));
    }
}

void *read_from_server()
{
    char buffer[256];
    while(1)
    {
        fflush(stdout);
        read(sockfd,buffer,255);
        if(strlen(buffer) != 0)
            printf("%s",buffer);
        bzero(buffer,256);
        if(is_terminating)
        {
            pthread_exit(0);
        }
    }
}