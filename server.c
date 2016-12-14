#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <pthread.h>

//SERVER MESSAGES
char help_command[] = "/help";
char msg_command[] = "/msg ";
char help_string1[] =
"************HELP************\n"
"/exit - exit client\n"
"/msg [clientName] - private message another client\n"
"/tictactoe [clientName] - start tictactoe match with another client\n";
char help_string2[] =
"/tictactoe team [clientName] - add client to your team for match\n"
"************HELP************\n";
char username_success[] = "Username created!\n";
//ERRORS
char username_length_error[] = "INVALID USERNAME: more than 20 characters\n";
char username_duplicate_error[] = "INVALID USERNAME: username already exists\n";
char username_spacing_error[] = "INVALID USERNAME: username contains a space\n";
char username_generic_error[] = "INVALID USERNAME: username could not be found\n";
//STRUCTS
typedef struct Client client;
struct Client
{
    int socket;
    char username[20];
};
//OTHER VARIABLES
const char *name = "Clients";
struct Client *clients_ptr;
const int SIZE = 100 * sizeof(struct Client);
int* total_clients;
//FUNCTIONS
int compare(char str1[], char str2[]); /* function prototype */
void *handle_client(int); /* function prototype */
int duplicate_client(char[]); /* function prototype */
char * setup_client(char buffer[], int sock);
void handle_messages(char *username, char buffer[],int sock);
void send_to_all_other_users(char *username, char message[256]);
int has_msg_command(char buffer[]);
int get_nth_keyword_index(char buffer[], int n);
int send_to_username(char username[25], char message[256]);
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
  pthread_t threads[100];
  int sockfd, newsockfd, portno, pid, shm_fd, thread_id = 0;
  socklen_t clilen;
  char buffer[256];
  struct sockaddr_in serv_addr, cli_addr;
  int n;
  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }
  //Shared Memory declaration
  total_clients = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  *total_clients = 0;
  shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
  ftruncate(shm_fd, SIZE);
  clients_ptr = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if(clients_ptr ==  MAP_FAILED) {
    printf("Map failed\n");
    return -1;
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
    pthread_create(&threads[thread_id], NULL, handle_client, (void *) newsockfd);
  } /* end of while */
  close(sockfd);
  return 0; 
}

/******** HANDLE_CLIENT() **************************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void *handle_client(int sock)
{
    char buffer[256];
    char* username = setup_client(buffer,sock);
    handle_messages(username, buffer,sock);
}

/******** SETUP_CLIENT() *********************
 Runs until client provides valid username.
 *****************************************/
char * setup_client(char buffer[], int sock)
{
    int invalid_username = 1, counter, has_space;
    char *username = malloc(256 * sizeof(char));;
    while(invalid_username)
    {
        has_space = 0;
        bzero(buffer,256);
        read(sock,buffer,255);
        buffer[ strlen(buffer) - 1 ] = '\0';
        strcpy(username, buffer);
        printf("Username Provided: %s\n", buffer);
        fflush(stdout);
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
            for(counter = 0; counter < sizeof(buffer); counter++)
            {
                if(buffer[counter] == ' ')
                {
                    write(sock,username_spacing_error,256);
                    has_space = 1;
                }
            }
            if(has_space)
            {
                continue;
            }
            clients_ptr[*total_clients].socket = sock;
            sprintf(((struct Client *)clients_ptr)[*total_clients].username, "%s", buffer);
            *total_clients = *total_clients + 1;
            invalid_username = 0;
            write(sock,username_success,255);
            return username;
        }
    }
}

/******** HANDLE_MESSAGES() *********************
 Runs indefinitely, handling all messages 
 supplied by the client. Handles commands, 
 blank messages, and generic messages
 *****************************************/
void handle_messages(char *username, char buffer[], int sock)
{
    int n, running = 1, i;
    char username_loc[25], message_loc[256], message_to_send[256];
    int username_index = 6, message_index;
    while(running) {
        bzero(buffer,255);
        n = read(sock,buffer,256);
        if (n < 0) error("ERROR reading from socket");
        if (strlen(buffer) <= 0) {
            //change this to not exit loop? just do nothing
            running = 0;
            continue;
        }
        else if(compare(buffer,help_command))
        {
            write(sock,help_string1,255);
            write(sock,help_string2,255);
        }
        else if(has_msg_command(buffer))
        {
            message_index = get_nth_keyword_index(buffer, 3);
            strncpy(username_loc, buffer+5, message_index-6);
            strncpy(message_loc, buffer+message_index, 230);
            sprintf(message_to_send, "DM from %s: %s> ", username, message_loc);
            if(!send_to_username(username_loc, message_to_send))
            {
                printf("ERROR invalid DM username from user %s\n", username);
                write(sock,username_generic_error,255);
                fflush(stdout);
            }
        }
        else
        {
            char return_message[256];
            buffer[ strlen(buffer) - 1 ] = '\0';
            sprintf(return_message, "%s: %s\n> ", username, buffer);
            send_to_all_other_users(username, return_message);
            printf("Here is the message: %s\n",buffer);
            fflush(stdout);
        }
    }
}

/******** GET_NTH_KEYWORD_INDEX() *********************
 Returns index for start of nth word in buffer.
 Useful for finding index of message in 
 "/msg username message".
 *****************************************/
int get_nth_keyword_index(char buffer[], int n)
{
    int index, space_counter = 0;
    for(index = 0; index < 27 && space_counter < n - 1; index++)
    {
        if(buffer[index] == ' ')
        {
            space_counter++;
        }
    }
    return index;
}

/******** SEND_TO_USERNAME() *********************
 Sends provided message to specified username.
 If user does not exist -> return 0
 Otherwise -> return 1
 *****************************************/
int send_to_username(char username[25], char message[256])
{
    int i;
    for(i = 0; i < *total_clients; i++)
    {
        if(strcmp(username, clients_ptr[i].username) == 0)
        {
            write(clients_ptr[i].socket,message,255);
            return 1;
        }
    }
    return 0;
}

/******** HAS_MSG_COMMAND() *********************
 Check provided char array for the
 "/msg " keyword. Returns 1 if present.
 Otherwise, returns 0.
 *****************************************/
int has_msg_command(char buffer[])
{
    char msg_command_loc[10];
    int i, space_loc;
    strncpy(msg_command_loc,buffer,5);
    return(compare(msg_command_loc,msg_command));
}

/******** SEND_TO_ALL_OTHER_USERS() *********************
 Sends supplied message to all other users on 
 the server except for the supplied username.
 *****************************************/
void send_to_all_other_users(char *username, char message[256])
{
    int i;
    for(i = 0; i < *total_clients; i++)
    {
        if(strcmp(username, clients_ptr[i].username) != 0)
            write(clients_ptr[i].socket,message,255);
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

/******** DUPLICATE_CLIENT() *********************
 Returns true if there is a duplicate client.
 Currently does not work properly
 *****************************************/
int duplicate_client(char buffer[])
{
    int i;
    for(i = 0; i < *total_clients; i++)
    {
        if(strcmp(((struct Client *)clients_ptr)[i].username,buffer)==0)
            return 1;
    }
    return 0;
}
