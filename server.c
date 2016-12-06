#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

//SERVER MESSAGES
char help_command[] = "/help";
char help_string1[] =
"************HELP************\n"
"/exit - exit client\n"
"/msg [clientName] - private message another client\n"
"/tictactoe [clientName] - start tictactoe match with another client\n";
char help_string2[] =
"/tictactoe team [clientName] - add client to your team for match\n"
"************HELP************\n";
char username_success[] = "Username successfully made\n";

//ERRORS
char username_length_error[] = "INVALID USERNAME: more than 20 characters\n";
char username_duplicate_error[] = "INVALID USERNAME: username already exists\n";

struct Client
{
    int socket;
    char username[20];
};

int total_clients = 0;
struct Client clients[100];

int compare(char str1[], char str2[]); /* function prototype */
void run(int); /* function prototype */
int duplicate_client(char[]); /* function prototype */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, pid;
  socklen_t clilen;
  char buffer[256];
  struct sockaddr_in serv_addr, cli_addr;
  int n;
  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
    sizeof(serv_addr)) < 0) 
    error("ERROR on binding");
  listen(sockfd,5);
  clilen = sizeof(cli_addr);
  while (1) {
    newsockfd = accept(sockfd,
      (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
      error("ERROR on accept");
    pid = fork();
    if (pid < 0)
      error("ERROR on fork");
    if (pid == 0)  {
      close(sockfd);
      run(newsockfd);
      exit(0);
    }
    else close(newsockfd);
    } /* end of while */
  close(sockfd);
  return 0; 
}

/******** RUN() **************************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void run(int sock)
{
    int n, running = 1, invalid_username = 1;
    char buffer[256];
    while(invalid_username)
    {
        bzero(buffer,256);
        read(sock,buffer,255);
        printf("Username Provided: %s", buffer);
        if(strlen(buffer) >= 20)
        {
            bzero(buffer,256);
            write(sock,username_length_error,256);
        }
        else if(duplicate_client(buffer))
        {
            bzero(buffer,256);
            write(sock,username_duplicate_error,256);
        }
        else
        {
            write(sock,username_success,256);
            clients[total_clients].socket = sock;
            strcpy(buffer,clients[total_clients].username);
            total_clients++;
            invalid_username = 0;
        }
    }
    while(running) {
      bzero(buffer,255);
      n = read(sock,buffer,256);
      if (n < 0) error("ERROR reading from socket");
      if (strlen(buffer) <= 0) {
        running = 0;
        continue;
      }
      else if(compare(buffer,help_command))
      {
          write(sock,help_string1,255);
          write(sock,help_string2,255);
      }
      else
      {
          //send to everyone else
          printf("Here is the message: %s\n",buffer);
      }
    }
}

/******** COMPARE() *********************
 Compares the two input strings. Parameter str1 
 must be the input from the clients as it is 
 contains a null character. Returns value of 
 0 if inputs have same characters. 
 Returns 1 otherwise.
 *****************************************/
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

//return true if there is a duplicate
int duplicate_client(char buffer[])
{
    int i;
    for(i = 0; i < total_clients; i++)
    {
        if(strcmp(clients[i].username,buffer)==0)
            return 1;
    }
    return 0;
}
