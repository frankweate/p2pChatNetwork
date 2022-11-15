#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "list.h"
#include "queue.h"

#define SERVER_ERR 1
#define NO_USER_ERR  2
#define BLOCK_ERR  3
#define WHOELSE_PARSE 4
#define TIMEOUT_ERR 5
#define ALREADY_LOGGED_IN 6
#define BLOCKED 7
#define NOT_LOGGED_IN 8
#define ALREADY_BLOCK_ERR 9
#define ALREADY_UNBLOCK_ERR 10
#define BLOCK_SELF_ERR 11
#define BLOCK_BROADCAST 12
void p2p_server(char *other_peer);
void p2p_client(uint32_t address,uint16_t port, char *name);
void get_addy(char * buffer);

void *p2p_send(void * data);
void *p2p_receive(void * data);

void stopprivate(char *user);
void acknowledge_p2p_close(char *sender);
void p2p_start_close(struct client_data * client);
void p2p_close(char *sender);
//p2p logout works like p2p_start_close, but for all connections
void p2p_logout();

void process_buffer(char * buffer,int sock);
void query_error();
void* query_server();
void* receive_requests();
void process_requests(char * buffer);

void print_message(char *buffer);
void print_broadcast(char *buffer);
void print_whoelse(char *buffer);
void print_err(char *buffer);
char  query_buffer[MESSAGE_CHUNK_SIZE];
char sending_buffer[MESSAGE_CHUNK_SIZE];
char user_username[60];
int sock;
int login_flag = 0;
struct client_list *p2p_clients;

int main(int argc,char *argv[]) {
    //prepare server credentials
    if(argc < 3)    {
        printf("Usage ./Client <server address> <server port>\n");
    }
   
    p2p_clients = list_create();
    
    const char* server_name = argv[1];
    const int server_port = atoi(argv[2]);
    //change this port No if required
    
    //this struct will contain address + port No
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    
    // http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
    inet_pton(AF_INET, server_name, &server_address.sin_addr);
    
    // htons: port in network order format
    server_address.sin_port = htons(server_port);
    
    // open a TCP stream socket using SOCK_STREAM, verify if socket successfuly opened
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("could not open socket\n");
        return 1;
    }
    
    // TCP is connection oriented, a reliable connection
    // **must** be established before any data is exchanged
    //initiate 3 way handshake
    //verify everything ok
    if (connect(sock, (struct sockaddr*)&server_address,
                sizeof(server_address)) < 0) {
        printf("could not connect to server\n");
        return 1;
    }
    
    // get input from the user
    char * data_to_send = "MUMP\n";
    //fgets reads in the newline character in buffer, get rid of it

    // data that will be sent to the server
    //printf("read : %s\n",data_to_send);
    send(sock, data_to_send, strlen(data_to_send)+1, 0);
    //actual send call for TCP socket
    while(1)    {
        
    // prepare to receive
        int len = 0, maxlen = MESSAGE_CHUNK_SIZE;
        char buffer[maxlen];
        char* pbuffer = buffer;
    
    // ready to receive back the reply from the server
        memset(buffer,0,sizeof(buffer));
        
        len = recv(sock, pbuffer, maxlen, 0);
        buffer[len] = '\0';
        process_buffer(buffer,sock);
    }  
    
    
    
    
    // close the socket
    close(sock);
    return 0;
}

void process_buffer(char * buffer,int sock) {
    int header_length = 5;
    
    if(strncmp(buffer,"MUMP\n",header_length) == 0) {
        //ignore header from now on
        char username[60];
        char password[60];
        buffer = buffer + header_length;
        if(strncmp(buffer,"LOGIN",header_length) == 0)   {
            //have we tried logging in and failed?
            buffer = buffer + 6;
            int attempts_left = atoi(buffer);
            if(attempts_left == 0 )  {
                if(buffer[0] == '0')    {
                    printf("You have been blocked!\n");
                }
            }else{
            
                printf("Login failed %d attempt(s) remaining!\n",attempts_left);
            }
            memset(user_username,0,60);
            printf("Enter Username:");
            scanf("%s", username);
            strcpy(user_username,username);
            printf("Enter Password:");
            scanf("%s", password);
            char login[122];
            memset(login,0,122);
            strcat(login,"MUMP\nLOGIN\n");
            strcat(login,username);
            strcat(login,"\n");
            strcat(login,password);
            send(sock,login,strlen(login)+1,0);
            
        }else if(strncmp(buffer,"LGACK",header_length) == 0)    {
            printf("------------------------------\n-                            -\n-   you are now logged in :) -\n-                            -\n------------------------------\n");
            
            pthread_t send_id,recv_id;
            pthread_create(&send_id, NULL, query_server, NULL);
            pthread_create(&recv_id,NULL,receive_requests,NULL);
            pthread_join(send_id,NULL);
            pthread_join(recv_id,NULL);
            exit(4);
        }else if(strncmp(buffer,"E",1) == 0)    {
            //we have been blocked 
            buffer = buffer + 2;
            int blocked_time = atoi(buffer);
            printf("You have been blocked, please try again in %d second(s)\n",blocked_time);
            send(sock,"MUMP\n",6,0);
        }else if(strncmp(buffer,"A",1) == 0)    {
            //user is logged in already
              printf("This user is already logged in\n");
              send(sock,"MUMP\n",6,0);
        }else if(strncmp(buffer,"N",1) == 0)    {
            //user does not exist
              printf("Invalid Username\n");
              send(sock,"MUMP\n",6,0);
        }

    }
}

void* query_server()  {
    char * pbuffer = query_buffer;
    char *part1, *part2, *part3;
    size_t size = MESSAGE_CHUNK_SIZE;
    getline(&pbuffer,&size,stdin);
    while(1)    {
        
        memset(pbuffer,0,MESSAGE_CHUNK_SIZE);
        getline(&pbuffer,&size,stdin);
        
        //replace the '\n' char with ' '
        pbuffer = strcat(pbuffer," ");
        part1 = strtok(pbuffer," ");
        memset(sending_buffer,0,MESSAGE_CHUNK_SIZE);
        //start with the one command queries:
        if(strncmp(part1,"whoelse\n",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Whoelse\n",MESSAGE_CHUNK_SIZE) == 0)  {
            //AUTO GENERATED METHOD STUB 
            strcat(sending_buffer,"MUMP\nW\n");
            strcat(sending_buffer,user_username);
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
           
            
            continue;
        }else if(strncmp(part1,"logout\n",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Logout\n",MESSAGE_CHUNK_SIZE) == 0)  {
            //AUTO GENERATED METHOD STUB
            //lets remove all p2p connections
            p2p_logout();
            
            strcat(sending_buffer,"MUMP\nL\n");
            strcat(sending_buffer,user_username);
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
            continue;
        }

        if(strncmp(part1,"broadcast",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Broadcast",MESSAGE_CHUNK_SIZE) == 0)  {
            part2 = strtok(NULL,"");
            strcat(sending_buffer,"MUMP\nB\n");
            strcat(sending_buffer,user_username);
            strcat(sending_buffer,"\n");
            strcat(sending_buffer,part2);
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
            continue;
        }

        part2 = strtok(NULL," ");
        if(part2 == NULL)   {
            query_error();
            continue;
        }
        //use the buffer that we have to craft the message
        //now do the two command queries:
        if(strncmp(part1,"block",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Block",MESSAGE_CHUNK_SIZE) == 0) {
            //AUTO GENERATED METHOD STUB
            strcat(sending_buffer,"MUMP\nK\n");
            strcat(sending_buffer,user_username);
            strcat(sending_buffer,"\n");
            strcat(sending_buffer,strtok(part2,"\n"));
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
            continue;
        }else if(strncmp(part1,"unblock",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Unblock",MESSAGE_CHUNK_SIZE) == 0)  {
            //AUTO GENERATED METHOD STUB
            strcat(sending_buffer,"MUMP\nU\n");
            strcat(sending_buffer,user_username);
            strcat(sending_buffer,"\n");
            strcat(sending_buffer,strtok(part2,"\n"));
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
            continue;
        }else if(strncmp(part1,"whoelsesince",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Whoelsesince",MESSAGE_CHUNK_SIZE) == 0)  {
            //AUTO GENERATED METHOD STUB
            strcat(sending_buffer,"MUMP\nS\n");
            strcat(sending_buffer,user_username);
            strcat(sending_buffer,"\n");
            strcat(sending_buffer,strtok(part2,"\n"));
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
            continue;
        }else if(strncmp(part1,"startprivate",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Startprivate",MESSAGE_CHUNK_SIZE) == 0)  {
            part2 = strtok(part2,"\n");
            
            struct client_data * c = get_client_from_name(p2p_clients,part2);
            
            //if c != NULL then we already have a connection    
            if(c != NULL)   {
                printf("You already are privately connected to this client\n");
                continue;
            }
            if(strcmp(user_username,part2) == 0)    {
                printf("you cannot connect with yourself\n");
                continue;
            }
           
            strcat(sending_buffer,"MUMP\nD\n");
            strcat(sending_buffer,user_username);
            strcat(sending_buffer,"\n");
            strcat(sending_buffer,strtok(part2,"\n"));
            
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
            
            continue;
        }else if(strncmp(part1,"stopprivate",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Stopprivate",MESSAGE_CHUNK_SIZE) == 0)  {
            part2 = strtok(part2,"\n");
            struct client_data * c = get_client_from_name(p2p_clients,part2);
            //if c != NULL then we already have a connection    
            if(c == NULL)   {
                printf("You are not privately connected to this client\n");
                continue;
            }
            if(strcmp(user_username,part2) == 0)    {
                printf("you cannot disconnect with yourself\n");
                continue;
            }
            pthread_mutex_lock(&p2p_clients->mutex);
            p2p_start_close(c); 
            pthread_mutex_unlock(&p2p_clients->mutex);
            continue;
        }
        part3 = strtok(NULL,"");
        if(part3 == NULL)   {
            query_error();
            continue;
        }

        //finally do the three command queries:
        if(strncmp(part1,"message",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Message",MESSAGE_CHUNK_SIZE) == 0) {
            //AUTO GENERATED METHOD STUB
            
            strcat(sending_buffer,"MUMP\nM\n");
            strcat(sending_buffer,part2);
            strcat(sending_buffer,"\n");
            strcat(sending_buffer,user_username);
            strcat(sending_buffer,"\n");
           
            strcat(sending_buffer,part3);
            send(sock,sending_buffer,strlen(sending_buffer) +1,0);
            continue;
        }else if(strncmp(part1,"private",MESSAGE_CHUNK_SIZE) == 0 || strncmp(part1,"Private",MESSAGE_CHUNK_SIZE) == 0){
                strcat(sending_buffer,"MUMP\nM\n");
                strcat(sending_buffer,part3);
               if(add_message_to_client(p2p_clients,part2,sending_buffer) == 0) {
                   printf("No p2p connection with this client\n");
                   continue;
               }
              

        }else{
            query_error();
            continue;
        }
    }
}

void query_error()  {
    printf("Error: Invalid query\n");
      printf("Command List:\n");
        printf("\tmessage <user> <message>\n");
            printf("\tbroadcast <message>\n");
            printf("\tblock <user>\n");
            printf("\tunblock <user>\n");
            printf("\twhoelse\n");
            printf("\twhoelsesince <time(seconds)>\n");
            printf("\tstartprivate <user>\n");
            printf("\tprivate <user> <message>\n");
            printf("\tstopprivate <user>\n");
            printf("\tlogout\n");
            printf("------------------------\n");
}


void* receive_requests() {
    fd_set master;
    FD_ZERO(&master);
    FD_SET(sock,&master);
    char * receiving_buffer = malloc(MESSAGE_CHUNK_SIZE);
    while(1)    {
        
        select(sock+1,&master,NULL,NULL,NULL);
        memset(receiving_buffer,0,MESSAGE_CHUNK_SIZE);
        if(FD_ISSET(sock,&master))  {
            int message_length;
            message_length = recv(sock,receiving_buffer,MESSAGE_CHUNK_SIZE,0);
            
            if(message_length == 0) {
                printf("Successfully Logged Out\n");
                exit(4);
            }
            if(message_length == -1)    {
                printf("ERROR RECIEVING\n");
                exit(4);
            }
            process_requests(receiving_buffer);


        }
    }
    free(receiving_buffer);
}

void process_requests(char * buffer)    {
    char *mump = "MUMP\n";
    if(strlen(buffer) < 7 || strncmp(buffer,mump,5) != 0)   {
        printf("%d\n%d\n",strlen(buffer),strncmp(buffer,"MUMP\n",5));
        printf("%s\n",buffer);
        printf("------------------------\nServer Error :(\n------------------------\n");
        return;
    }
    char *req_type = buffer+5;
    char *second_mes = strstr(req_type,"MUMP");
    if(second_mes != NULL)  {
        //end the string at the end of the message
        memset(second_mes,0,1);
    
    }
    switch(*req_type)    {

        case 'E':
            //the server has given us an error
            print_err(req_type);
            break;
        case 'M':
            //the server is delivering a message
            print_message(req_type);
         
            break;
        case 'B':
            //the server has delivered a boradcast
            print_broadcast(req_type);
            break;
        case 'W':
            print_whoelse(req_type);
        break;
        
        case 'L':
            printf("Successfully Logged Out\n");
            exit(4);
        break;

        case 'U':
            //the server acknowledges our unblock
            printf("Unblock Successful\n");
        break;
        case 'K':
            //the server acknowledges our block
            printf("Block Successful\n");
        break;
        case 'S':
            //server boradcasts e.g. <<user> logged in>
            printf("<%s>\n",req_type+2);
            break;
        case 'D':
            //server acknowledges our request to connect with valid client
            p2p_server(strtok(req_type+2,""));
            break;
        case 'P':
            //server is letting us know that a client is attempting a private connection
            get_addy(req_type+2);
            break;
        case 'F':
            //this other peer wants to end p2p connection
            p2p_close(req_type+2);
            acknowledge_p2p_close(req_type+2);
            printf("finished connection\n");
            break;
        case 'X':
            //the other client has acknowledged our request to close p2p connection 
            p2p_close(req_type+2);
            break;


    }
    if(second_mes != NULL)  {
        //reset the string to how is was
        memset(second_mes,'M',1);
        
        return process_requests(second_mes);
    }
    
}

void print_message(char * buffer)   {
    char *sender,*message;
    
    strtok(buffer,"\n");
    sender = strtok(NULL,"\n");
    message = strtok(NULL,"");
    printf("<%s> %s",sender,message);
}

void print_broadcast(char *buffer)  {
     char *sender,*message;
    
    strtok(buffer,"\n");
    sender = strtok(NULL,"\n");
    message = strtok(NULL,"");
    printf("<ALL> <%s> %s",sender,message);
}

void print_err(char *buffer)    {
    char *err_val_char;
    strtok(buffer,"\n");
    err_val_char = strtok(NULL,"");
    int err_val = atoi(err_val_char); 
    char logout_char[100];  
    
    switch(err_val) {
        case SERVER_ERR:
            printf("Error: Server cannot process request at this time\n");
        break;
        case BLOCK_ERR:
            printf("You have been blocked by this user\n");
            break;
        case NO_USER_ERR:
            printf("Error: the user you have specified does not exist\n");
            break;
        case WHOELSE_PARSE:
            printf("Error: please enter a positive integer for <whoelsesince> command\n");
            break;
        case TIMEOUT_ERR:
            printf("You have been timed out due do your inactivity\n");
            //so we end p2p connections
            p2p_logout();
            memset(logout_char,0,100);
            strcat(logout_char,"MUMP\nL\n");
            strcat(logout_char,user_username);
            send(sock,logout_char,strlen(logout_char)+1,0);
            break;
         case NOT_LOGGED_IN:
            printf("User is not logged in\n");
            break;
        case ALREADY_BLOCK_ERR:
            printf("Error: User is already Blocked\n");
            break;
        case ALREADY_UNBLOCK_ERR:
            printf("Error: User is not blocked\n");
            break;
        case BLOCK_SELF_ERR:
            printf("Error: You cannot block/unblock yourself\n");
            break;
        case BLOCK_BROADCAST:
            printf("Broadcast was blocked by some recipients\n");
            break;
          
    
    }
}

void print_whoelse(char *buffer)    {
    char *user;
    strtok(buffer,"\n");
    user = strtok(NULL,"\n");
    printf("Users:\n");
    while(user != NULL) {
        printf("%s\n",user);
        user = strtok(NULL,"\n");
    }
}

void p2p_server(char *other_peer)   {
    int yes = 1;
    struct sockaddr_storage client_address;
    struct sockaddr_in server_address;
    socklen_t length = sizeof(server_address);
    memset(&server_address, 0, sizeof(server_address));
    
    
    server_address.sin_port = 0;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_family = AF_INET;
    int listen_sock;
    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("could not create listen socket\n");
        return;
    }
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    // bind it to listen to the incoming connections on the created server
    // address, will return -1 on error
    if ((bind(listen_sock, (struct sockaddr *)&server_address,
              sizeof(server_address))) < 0) {
        printf("could not bind socket\n");
        return;
    }
    if(listen(listen_sock,5)< 0)  {
        printf("couldn't open socket for listening\n");
    }
    int p2p_sock;
    
    getsockname(listen_sock,(struct sockaddr *)&server_address,&length);

    char *message = malloc(MESSAGE_CHUNK_SIZE);
    char* temp = malloc(MAX_INT_STRING_LENGTH);
    memset(message,0,MESSAGE_CHUNK_SIZE);
    memset(temp,0,MAX_INT_STRING_LENGTH);
    strcat(message,"MUMP\nP\n");
    strcat(message,other_peer);
    strcat(message,"\n");
    strcat(message,user_username);
    strcat(message,"\n");
    sprintf(temp,"%u",server_address.sin_addr.s_addr);
    strcat(message,temp);
    memset(temp,0,MAX_INT_STRING_LENGTH);
    strcat(message,"\n");
    sprintf(temp,"%u",server_address.sin_port);
    strcat(message,temp);
    /*  MUMP
        P
        <client>
        <user_username>
        <our_server_address>
        <our_server_port>
    */   
    send(sock,message,strlen(message)+1,0);
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);
    p2p_sock = accept(listen_sock,(struct sockaddr*)&their_addr,&addr_size);
    
    free(message);
    free(temp);
    Node client = create_client_node(p2p_sock,other_peer,1);
 
    list_add(client,p2p_clients);
       if(client->data == NULL)    {
           
    }
   

    pthread_create(&client->data->receiver,NULL,p2p_receive,client->data);
    
    pthread_create(&client->data->sender,NULL,p2p_send,client->data);
    pthread_detach(client->data->sender);
    pthread_detach(client->data->receiver);
    printf("Successfully established private connection\n");
    
}

void p2p_client(uint32_t address, uint16_t port,char *name)    {
    struct sockaddr_in server_address;
    memset(&server_address,0,sizeof(server_address));
    server_address.sin_port = port;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = address;
    int p2p_sock;
    
    if ((p2p_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("could not open socket\n");
        return;
    }
     
    // TCP is connection oriented, a reliable connection
    // **must** be established before any data is exchanged
    //initiate 3 way handshake
    //verify everything ok
    if (connect(p2p_sock, (struct sockaddr*)&server_address,
                sizeof(server_address)) < 0) {
        printf("%s\n",strerror(errno));
        return;
    }
    Node client = create_client_node(p2p_sock,name,0);
    list_add(client,p2p_clients);
    if(client->data == NULL)    {
        printf("null client ERROR please logout\n");
    }
    pthread_create(&client->data->receiver,NULL,p2p_receive,client->data);
    
    pthread_create(&client->data->sender,NULL,p2p_send,client->data);
    pthread_detach(client->data->sender);
    pthread_detach(client->data->receiver);
}

void get_addy(char * buffer) {
    char *name = strtok(buffer,"\n");
    uint32_t address = atoi(strtok(NULL,"\n"));
    uint16_t port = atoi(strtok(NULL,"\n"));
    p2p_client(address,port,name);
}

void * p2p_send(void * data) {
    pthread_mutex_lock(&p2p_clients->mutex);
    struct client_data *client = (struct client_data  *) data;
    pthread_mutex_unlock(&p2p_clients->mutex);
    if(client == NULL)
        printf("client NULL send\n");
    //this is the client we are talking to 
    while(1)    {
        pthread_mutex_lock(&p2p_clients->mutex);
        if(client->messages->length > 0)    {
            QNode message = dequeue(client->messages);
            
            send(client->sock_num,message->message,strlen(message->message) +1,0);
            free_QNode(message);
        }
        pthread_mutex_unlock(&p2p_clients->mutex);
        sleep(1);
    }
    
}

void * p2p_receive(void * data)  {
    pthread_mutex_lock(&p2p_clients->mutex);
    struct client_data *client = (struct client_data  *)data;
    
    pthread_mutex_unlock(&p2p_clients->mutex);
     if(client == NULL)
        printf("client NULL recv\n");
    char * buffer = malloc(MESSAGE_CHUNK_SIZE);
    
    while(1)    {
        memset(buffer,0,sizeof(buffer));
        recv(client->sock_num,buffer,MESSAGE_CHUNK_SIZE,0);
        buffer = strtok(buffer,"\n");
        buffer = strtok(NULL,"\n");
        
        if(strcmp(buffer,"M") == 0)   {
            buffer = strtok(NULL,"\n");
            printf("<private><%s> %s\n",client->name,buffer);
        }
    }
}

void acknowledge_p2p_close(char *sender)    {
        char * message = malloc(MESSAGE_CHUNK_SIZE);
            memset(message,0,MESSAGE_CHUNK_SIZE);
            strcat(message,"MUMP\nX\n");
            strcat(message,user_username);
            strcat(message,"\n");
            strcat(message,sender);
            
            //send a message to the server acknowleding the request
            send(sock,message,strlen(message)+1,0);
            free(message);
}

void p2p_close(char *sender)    {
            
    struct client_data * c = get_client_from_name(p2p_clients,sender);
    
            if(c == NULL) 
                printf("Client == null %s %d\n",sender,strlen(sender));
            if(c->sender != -1)   {
            pthread_cancel(c->sender);
            pthread_cancel(c->receiver);
            }
            close(c->sock_num);
            
            list_remove(c,p2p_clients);
}

void p2p_start_close(struct client_data * client)   {
            pthread_cancel(client->sender);
            pthread_cancel(client->receiver);
            client->sender = -1;
            client->receiver = -1;
            strcat(sending_buffer,"MUMP\nF\n");
            strcat(sending_buffer,user_username);
            strcat(sending_buffer,"\n");
            strcat(sending_buffer,client->name);
            strcat(sending_buffer,"\n");
            send(sock,sending_buffer,strlen(sending_buffer)+1,0);
}

void p2p_logout()   {
    if(p2p_clients->length > 0) {
                pthread_mutex_lock(&p2p_clients->mutex);
                Node curr = p2p_clients->head;
                while(1)    {
                    
                    
                    
                    p2p_start_close(curr->data);
                    usleep(100000);
                    if(curr->next != NULL)  {
                        curr = curr->next;
                    }else{
                        pthread_mutex_unlock(&p2p_clients->mutex);
                        break;
                    }
                }
            }

}
