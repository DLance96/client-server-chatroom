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
char online_command[] = "/online";
char help_string1[] =
"************HELP************\n"
"/exit - exit client\n"
"/msg [clientName] - private message another client\n"
"/tictactoe [clientName] - start tictactoe match with another client\n";
char help_string2[] =
"/tictactoe team [clientName] - add client to your team for match\n"
"/online - lists the users online\n"
"************HELP************\n";
char online_string[] = 
"***********ONLINE***********\n";
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
    int online;
};
typedef struct Group group;
struct Group
{
  char name[20];
  int members[20];
  int member_count;
};
//OTHER VARIABLES
const char *name = "Clients";
const char *name_group = "Groups";
struct Client *clients_ptr;
struct Group *groups_ptr;
const int SIZE = 100 * sizeof(struct Client);
const int GROUP_SIZE = 100 * sizeof(struct Group);
int* total_clients;
int* total_groups;
//FUNCTIONS
int compare(char str1[], char str2[]);
void *handle_client(int);
int duplicate_client(char[]);
int duplicate_group(char[]);
char * setup_client(char buffer[], int sock);
void handle_messages(char *username, char buffer[],int sock);
void send_to_all_other_users(char *username, char message[256]);
int has_x_command(char buffer[], char command[], int size);
int get_nth_keyword_index(char buffer[], int n);
int send_to_username(char username[25], char message[256]);
int get_username_index(char username[]);
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
  pthread_t threads[100];
  int sockfd, newsockfd, portno, pid, shm_fd, shm_fd2, thread_id = 0;
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
  total_groups = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  *total_groups = 0;
  shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
  shm_fd2 = shm_open(name_group, O_CREAT | O_RDWR, 0666);
  ftruncate(shm_fd, SIZE);
  clients_ptr = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  groups_ptr = mmap(0, GROUP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd2, 0);
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
    pthread_exit(0);
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
        fflush(stdout);
        if (strlen(buffer) <= 0) {
            pthread_exit(0);
        }
        else if(strlen(buffer) >= 20)
        {
            printf("USERNAME EXCEEDS LENGTH LIMIT: %s\n", buffer);
            bzero(buffer,256);
            write(sock,username_length_error,256);
        }
        else if(duplicate_client(buffer))
        {
            printf("USERNAME ALREADY EXISTS: %s\n", buffer);
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
            printf("Username Created: %s\n", buffer);
            clients_ptr[*total_clients].socket = sock;
            clients_ptr[*total_clients].online = 1;
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
    char username_loc[25], message_loc[256], message_to_send[256], group[25];
    int username_index = 6, message_index, group_index;
    while(running) {
        bzero(buffer,255);
        n = read(sock,buffer,256);
        if (n < 0) error("ERROR reading from socket");
        if (strlen(buffer) <= 0) {
            //change this to not exit loop? just do nothing
            for(i = 0; i < *total_clients; i++)
            {
              if(strcmp(username, clients_ptr[i].username) == 0)
                  clients_ptr[i].online = 0;
            }
            break;
        }
        else if(compare(buffer,help_command))
        {
            write(sock,help_string1,255);
            write(sock,help_string2,255);
        }
        else if(has_x_command(buffer, msg_command, 5))
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
        else if(has_x_command(buffer, "/grpadd ", 8))
        {
            int i;
            group_index = get_nth_keyword_index(buffer, 2);
            strncpy(group, buffer+message_index, 230);
            sprintf(message_to_send, "Group added: %s> ", group);
            for(i = 0; i < *total_groups; i++) {
              if(!duplicate_group(group)) {
                  sprintf(((struct Group *)groups_ptr)[*total_groups].name, "%s", group);
                  groups_ptr[*total_groups].members[0] = get_username_index(username);
                  groups_ptr[*total_groups].member_count = 1;
                  *total_groups = *total_groups + 1;
              }
            }
        }
        else if(has_x_command(buffer, "/grp ", 5))
        {
            int i, j;
            message_index = get_nth_keyword_index(buffer, 3);
            strncpy(group, buffer+5, message_index-6);
            strncpy(message_loc, buffer+message_index, 230);
            sprintf(message_to_send, "(%s) %s: %s> ", group, username, message_loc);
            for(i = 0; i < *total_groups; i++) {
                if(strcmp(group, groups_ptr[i].name) == 0) {
                    for(j = 0; j < groups_ptr[i].member_count; j++) {
                        write(clients_ptr[groups_ptr[i].members[j]].socket, message_to_send, 255);
                    }
                    i = 100;
                }
                else if(i == 100) {
                    printf("ERROR invalid group nane from user %s\n", username);
                    write(sock,username_generic_error,255);
                    fflush(stdout);
                }
            }
            fflush(stdout);
        }
        else if(compare(buffer, online_command))
        {
            write(sock,online_string, 255);
            for(i = 0; i < *total_clients; i++) {
              char str[255];
              sprintf(str, "- %s %d\n",clients_ptr[i].username ,clients_ptr[i].online);
              write(sock,str,255);
            }
        }
        else
        {
            char return_message[256];
            buffer[ strlen(buffer) - 1 ] = '\0';
            sprintf(return_message, "%s: %s\n> ", username, buffer);
            send_to_all_other_users(username, return_message);
            printf("%s: %s\n", username, buffer);
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

/******** HAS_x_COMMAND() *********************
 Check provided char array for the
 "/msg " keyword. Returns 1 if present.
 Otherwise, returns 0.
 *****************************************/
int has_x_command(char buffer[], char command[], int size)
{
    char x_command_loc[10];
    strncpy(x_command_loc,buffer, size);
    return(compare(x_command_loc,command));
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

/******** DUPLICATE_GROUP() *********************
 Returns true if there is a duplicate group.
 *****************************************/
int duplicate_group(char buffer[])
{
    int i;
    for(i = 0; i < *total_groups; i++)
    {
        if(strcmp(((struct Group *)groups_ptr)[i].name,buffer)==0)
            return 1;
    }
    return 0;
}


/******** GET_USERNAME_INDEX() *********************
 Gets the index of current username
 *****************************************/
int get_username_index(char username[]) {
    int i;
    for(i = 0; i < *total_clients; i++)
    {
        if(strcmp(((struct Client *)clients_ptr)[i].username,username)==0)
            return i;
    }
    return -1; 
}
