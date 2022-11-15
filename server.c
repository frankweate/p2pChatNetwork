#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>

#include "queue.h"
#include "list.h"

int login(char *buffer, int sock, int block_dur,u_int32_t client_address, u_int16_t client_port);
void flush_offline_messages(struct client_data * client);
void add_client_creds();

void message_client(char * buffer);
void block_user(char *buffer,int socket, int unblock);
void broadcast(char * buffer);
void server_broadcast(char *message, int excluded_sock);
int handle_client_message(char * buffer, int socket);
void format_error(int socket,char *buffer);
void time_out_clients(int time_out);
void send_client_data(char *buffer);
void send_address_to_client(char *buffer) ;
void end_client_chat(char * buffer);

void update_set(fd_set * master,int listen_sock);
void ack_client_chat(char *buffer);




int logout(char *name);
void whoelse(char*user) ;
void whoelse_since(char*buffer);




struct client_list *list;
struct client_list *not_logged_in_users;
//block_matrix[i][j] means that i has blocked j
int8_t ** block_matrix;

int main(int argc, char *argv[])    {
    //create a new client_list
    list = list_create();
    not_logged_in_users = list_create();
    add_client_creds();
    struct sockaddr_storage client_address;
    fd_set master, read_fds;
    int fdmax, newfd;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    if(argc != 4)   {
        printf("Usage: <port> <block duration> <timeout>\n");
        return -1;
    }
    //adding args
    int port_num = htons(atoi(argv[1]));
    int timeout = atoi(argv[3]);
    int block_duration = atoi(argv[2]);
    int yes = 1;
    char receiving_buffer[MESSAGE_CHUNK_SIZE];
    // socket address used for the server
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    
    // htons: host to network short: transforms a value in host byte
    // ordering format to a short value in network byte ordering format
    server_address.sin_port = port_num;
    
    // htonl: host to network long: same as htons but to long
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    
    // create a TCP socket (using SOCK_STREAM), creation returns -1 on failure
    int listen_sock;
    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        //printf("could not create listen socket\n");
        return 1;
    }
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    // bind it to listen to the incoming connections on the created server
    // address, will return -1 on error
    if ((bind(listen_sock, (struct sockaddr *)&server_address,
              sizeof(server_address))) < 0) {
        //printf("could not bind socket\n");
        return 1;
    }
   
    int wait_size = 5;  // maximum number of waiting clients
    if (listen(listen_sock, wait_size) < 0) {
        //printf("could not open socket for listening\n");
        return 1;
    }
    
    FD_SET(listen_sock,&master);
    fdmax = listen_sock;
    //create a 1 second timeout for select
    struct timeval stop_block_time;
    stop_block_time.tv_sec = 1;
    stop_block_time.tv_usec = 0;

    // run indefinitely
    while (1) {
        read_fds = master;
        int select_ret = select(fdmax+1,&read_fds,NULL,NULL,&stop_block_time);
        if(select_ret == -1)    {
            perror("select");
            exit(4);
        }
        if(select_ret == 0) {
            //we have a timeout
            time_out_clients(timeout);
            stop_block_time.tv_sec = 1;
            update_set(&master,listen_sock);


            continue;
        }
        for(int sock = 0; sock <= fdmax; sock++)    {
            if(FD_ISSET(sock,&read_fds))  {
                if(sock == listen_sock) {
                    //new connection 
                    socklen_t  addrlen = sizeof(client_address);
                    newfd = accept(sock,(struct sockaddr *)&client_address,&addrlen);
                    //add them to the list of not logged in users
                    struct sockaddr_in * sock_in = (struct sockaddr_in*)&client_address;
                    list_add(create_not_logged_in_Node(newfd,sock_in->sin_addr.s_addr,sock_in->sin_port),not_logged_in_users);
                    
                    FD_SET(newfd,&master);
                    if(newfd > fdmax)   {
                        fdmax = newfd;
                    }
                
                }else{
                    //new data from existing client
                    //get the data

                    
                    struct client_data *client = get_client_from_socknum(list,sock);
                   
                    int message_length;
                    memset(&receiving_buffer,0,sizeof(receiving_buffer));
                    message_length = recv(sock,&receiving_buffer,sizeof(receiving_buffer),0);
                  
                    if(message_length == -1)  {
                        
                        perror("recv");
                        exit(4);
                    }
                if(client == NULL)  {
                    //this means the client is not logged in 
                    //so we get the socket_num of the not logged in client
                    struct client_data *not_logged_in_client = get_client_from_socknum(not_logged_in_users,sock);
                    if(not_logged_in_client == NULL) {
                        //printf("client data == null\n");
                    }
                    
                    if(login(receiving_buffer,sock,block_duration,not_logged_in_client->addr,not_logged_in_client->port) == -1) {
                        //login failed
                    }else{
                        //login successful
                       list_remove(not_logged_in_client,not_logged_in_users);
                     
                    }
                    continue;
                }
                char *pbuffer = receiving_buffer;
                
                //client message is inside client_buffer
                char *single_message = strstr(pbuffer+5,"MUMP\n");
                if(single_message == NULL)  {
                    handle_client_message(pbuffer,sock);
                }else{
                    //we have multiple messages in buffer
                    char *message = malloc(MESSAGE_CHUNK_SIZE);
                    memset(message,0,MESSAGE_CHUNK_SIZE);
                    while(1)    {
                        
                        strncpy(message,pbuffer,*single_message - *pbuffer);
                        
                        handle_client_message(message,sock);
                        pbuffer = single_message;
                        single_message = strstr(pbuffer+5,"MUMP\n");
                        if(single_message == NULL)  {
                            handle_client_message(pbuffer,sock);
                            free(message);
                            break;
                        }

                        memset(message,0,MESSAGE_CHUNK_SIZE);
                    }
                }
               //
              
                FD_ZERO(&master);
                FD_SET(listen_sock,&master);
                //timeout old clients
                //time_out_clients(timeout);
                //added logged in clients
                update_set(&master,listen_sock);
                }
                
            }
            
        }
    }
    //close listening socket
    close(listen_sock);
    return 0;

}
 /* MUMP
    LOGIN
    <username>
    <password>
    */
int login(char * buffer,int sock,int block_dur,u_int32_t client_address, u_int16_t client_port) {
    char *username;
    char *password;
    username = strtok(buffer,"\n");
    username = strtok(NULL,"\n");
    //if this is the user is trying to login then
    // this will be set to LOGIN
    if(username == NULL)    {
         
         
         send(sock,"MUMP\nLOGIN\n",strlen("MUMP\nLOGIN\n")+1,0);
        return -1;
    }
    if(strncmp("LOGIN",username,5) != 0)    {
    
        send(sock,"MUMP\nLOGIN\n",strlen("MUMP\nLOGIN\n")+1,0);
        return -1;
    }
    
    username = strtok(NULL,"\n");
    password = strtok(NULL,"");
    
    if(list->length == 0)   {
        return -1;
    }
    Node curr = list->head;
    while(1)    {
        if(strncmp(username,curr->data->name,100) == 0) {
           
            if(strncmp(password,curr->data->password,100) == 0 && time(NULL) >= curr->data->blocked_until)  {
            //if client is not already logged in 
                if(curr->data->is_online == 0)  {
                    //let's set the client data struct, to be logged in
                    curr->data->addr = client_address;
                    curr->data->port = client_port; 
                    curr->data->logged_in_times = 0;
                    curr->data->is_online = 1;
                    curr->data->sock_num = sock;
                    curr->data->last_query = time(NULL);
                    send(sock,"MUMP\nLGACK\n",12,0);
                    memset(buffer,0,MESSAGE_CHUNK_SIZE);
                    strcat(buffer,curr->data->name);
                    strcat(buffer," has logged in!");
                    server_broadcast(buffer,sock);
                    flush_offline_messages(curr->data);
                    return 1;
                }else{
                    //user is logged in on another session
                    send(sock,"MUMP\nA\n",8,0);
                    return -1;
                }
            }else{
                //we have a correct username but incorrect password
                curr->data->logged_in_times++;
                 if(curr->data->logged_in_times >= 3)    {
                //we block them
                if(curr->data->blocked_until == 0)  {
                    curr->data->blocked_until = time(NULL) + block_dur;
                }
                if(time(NULL) >= curr->data->blocked_until) {
                    //they have served their time
                    curr->data->blocked_until = 0;
                    curr->data->logged_in_times = 0;
                }else{
                    //SEND A BLOCK MESSAGE
                    char integer[11];
                    memset(buffer,0,MESSAGE_CHUNK_SIZE);
                    strcat(buffer,"MUMP\nE\n");
                    snprintf(integer,11,"%ld",curr->data->blocked_until - time(NULL));
                    strcat(buffer,integer);
                    send(sock,buffer,strlen(buffer) +1,0);
                    return -1;
                }
            }
                char login_attempts[2];
                        memset(buffer,0,MESSAGE_CHUNK_SIZE);
                        strcat(buffer,"MUMP\nLOGIN\n");
                        snprintf(login_attempts,2,"%d",3 - curr->data->logged_in_times);
                        strcat(buffer,login_attempts);
                        send(sock,buffer,strlen(buffer)+1,0);
                return -1;
            }
        }    
        
        if(curr != list->tail)  {
            curr = curr->next;
        }else {
            break;
        }
    }
    //no clients with that name
    send(sock,"MUMP\nN",7,0);
    return -1;
}


//creates a client list from the given Credentials.txt file
void add_client_creds() {
    FILE *fp;
    fp = fopen("Credentials.txt","r");
    char user_cred_buffer[120];
    char * username;
    char * password;
    int id_num = 0;
    while(fgets(user_cred_buffer,120,fp) != NULL)   {
        //read the user's name from the file
        username = malloc(MAX_NAME_LENGTH);
        password = malloc(MAX_NAME_LENGTH);
        strncpy(username,strtok(user_cred_buffer," "),MAX_NAME_LENGTH);
        strncpy(password,strtok(NULL,"\n"),60);
        Node curr_client = create_Node(username,password);
        curr_client->data->id = id_num;
        id_num++;
        list_add(curr_client,list);
    }

    //allocate memory for a 2d block array
    block_matrix = malloc(sizeof(block_matrix)*id_num);
    for(int i = 0; i < id_num;i++)  {
        block_matrix[i] = malloc(id_num);
        memset(block_matrix[i],0,id_num);
    }
    for(int i = 0; i < id_num;i++)  {
        for(int j = 0; j < id_num;j++)  {
            
    }
    
    }
}

int handle_client_message(char * buffer, int socket)    {
    struct client_data * client = get_client_from_socknum(list,socket);
    client->last_query = time(NULL);
    if(strlen(buffer) < 7 || memcmp(buffer,"MUMP\n",5) != 0)    {
       
        format_error(socket,buffer);
    }
    char client_request_type = buffer[5];
    
    switch(client_request_type)    {
        //message
        case 'M':
        //we don't need to send the header to this function
        message_client(buffer+strlen("MUMP\nM\n")- strlen(""));
        break;
        //broadcast
        
        case 'B':
        broadcast(buffer+strlen("MUMP\nB\n")- strlen(""));
        break;
        //whoelse on chat server
        case 'W':
            whoelse(buffer+strlen("MUMP\nw\n")- strlen(""));
        break;
        //unblock user
        case 'U':
        block_user(buffer+strlen("MUMP\nU\n")- strlen(""),socket,1);
        break;

        case 'S':
            whoelse_since(buffer+strlen("MUMP\nS\n")- strlen(""));
            break;
        //logout
        case 'L':
           return logout(buffer+strlen("MUMP\nL\n")- strlen(""));
        break;
        //block
        case 'K':
            block_user(buffer+strlen("MUMP\nK\n")- strlen(""),socket,0);
            break;
        case 'D':
            send_client_data(buffer + strlen("MUMP\nD\n") - strlen(""));
            break;
        case 'P':
            //client has setup virtual server with address and port numbers specified
            send_address_to_client(buffer + strlen("MUMP\nP\n") - strlen(""));
            break;
        case 'F':
            //the client has requested to end a private chat with another client
            end_client_chat(buffer + strlen("MUMP\nF\n") - strlen(""));
            break;
        case 'X':
            //the client has acknowledged an end of chat
            //we can send this straight to the other client
            ack_client_chat(buffer + strlen("MUMP\nX\n") - strlen(""));
            break;
        default:
        format_error(socket,buffer);
        break;
    }
    return 0;
}
//buffer comes in as :
/*
<reciever name>
<sender name>
<message>
*/

//sending to client as:
/*
MUMP
M
<sender name>
<message>
*/
void message_client(char * buffer)   {
    char *message = malloc(MESSAGE_CHUNK_SIZE); 
    memset(message,0,MESSAGE_CHUNK_SIZE);
    char * header = "MUMP\nM\n";
    char * receiver_name = strtok(buffer,"\n");
    char * sender_name = strtok(NULL,"\n");
    char * client_message = strtok(NULL,"");
    
    strcat(message,header);
    strcat(message,sender_name);
    strcat(message,"\n");
    strcat(message, client_message);
   

    struct  client_data * client = get_client_from_name(list,receiver_name);
    struct client_data *sender = get_client_from_name(list,sender_name);
    if(client == NULL)  {
        //invalid client so we notify sender
        send(sender->sock_num,"MUMP\nE\n2",9,0);
        return;
    }
    if(block_matrix[client->id][sender->id])    {
        //client has blocked sender so we notify sender of this
        send(sender->sock_num,"MUMP\nE\n3",9,0);
        
        return;
    }

    if(client->sock_num != -1) {
        //client is online
       
        send(client->sock_num,message,strlen(message),0);
        free(message);
    }else {
        //client is offline, so we add the message to the queue
        add_message_to_client(list,receiver_name,message);
    }
    
}

// client A broadcasts message to all online users who 
// have not blocked A
/* 
<sender name>
<message length>
<broadcast message>
*/
/*
MUMP
B
<sender name>
<message length>
<broadcast message>
*/
void broadcast(char * buffer)   {
    char * header = "MUMP\nB\n";
    char  sender_name[MAX_NAME_LENGTH]; 
    memset(sender_name,0,MAX_NAME_LENGTH);
    int length = strchr(buffer,'\n') - buffer;
    
    if(length > 60)
        length = 60;
    strncpy(sender_name,buffer,length);
    
    char *message = malloc(MESSAGE_CHUNK_SIZE);
    memset(message,0,MESSAGE_CHUNK_SIZE);
    
    
    strcat(message,header);
    strcat(message,buffer);
    struct client_data * sender = get_client_from_name(list,sender_name);
    if(sender == NULL)  {
        
        return;
    }
    Node client = list->head;
    int block_count = 0;
    while(1)    {
       
        if(client->data->sock_num != -1 && strcmp(sender_name,client->data->name) != 0)    {
            //so broadcast to them
            if(block_matrix[client->data->id][sender->id] == 0) {
                send(client->data->sock_num,message,strlen(message)+1,0);
            }else{ 
                block_count = 1;
            }
        }
        if(client == list->tail)
            break;
        client = client->next;
    }
    if(block_count == 1)
        send(sender->sock_num,"MUMP\nE\n12",10,0);
    free(message);
}

void toggle_login(char * buffer)    {
    
}



void block_user(char *buffer, int socket, int unblock)   {
    char *sender = strtok(buffer,"\n");
    char *to_be_blocked = strtok(NULL,"");
    struct client_data * sen = get_client_from_name(list,sender);
   
    struct client_data * block = get_client_from_name(list,to_be_blocked);
    
    if(block == NULL )   {
        send(socket,"MUMP\nE\n2",9,0);
        return;
    }
    if(sen->id == block->id)    {
        send(socket,"MUMP\nE\n11",10,0);
        return;
    }
    if(unblock != 1)    {
        if(block_matrix[sen->id][block->id] != 1) {
            block_matrix[sen->id][block->id] = 1;
        }else{
            //already blocked 
            send(sen->sock_num,"MUMP\nE\n9",9,0);
            return;
        }
        send(socket,"MUMP\nK\n",8,0);
    }else{
         if(block_matrix[sen->id][block->id] != 0) {
        block_matrix[sen->id][block->id] = 0;
         }else{
              //already unblocked 
            send(sen->sock_num,"MUMP\nE\n10",10,0);
            return;
         }
        send(socket,"MUMP\nU\n",8,0);
    }
    
}

void format_error(int socket, char *buffer)  {
    //eventually function to send error to client
    struct client_data *client = get_client_from_socknum(list,socket);
    if(client == NULL)  {
        //printf("error finding client\n\n");
    }
    send(socket,"MUMP\n99\n",9,0);
}






//returns 0 if no client with that name exists
//returns 1 if successful
//only used if client is offline, when the client comes online the message queue will be emptied.


void flush_offline_messages(struct client_data * client)   {
    QNode buffer;
    usleep(10000);
    while(client->messages->length > 0) {
        buffer = dequeue(client->messages);
       
        send(client->sock_num,buffer->message,strlen(buffer->message),0);
        
        free_QNode(buffer);
    }
    return;
}

int logout(char * user)    {
    int ret_val = 0;
    struct client_data * client = get_client_from_name(list,user);
    if(client == NULL)  {
        return 0;
    }
    ret_val = client->sock_num;
    send(client->sock_num,"MUMP\nL\n",8,0);
    char message[MAX_NAME_LENGTH +20];
    memset(message,0,MAX_NAME_LENGTH+20);
    strcat(message,user);
    strcat(message," has logged out!");
    server_broadcast(message,client->sock_num);
    close(client->sock_num);
  
    
    client->sock_num = -1;
    client->is_online = 0;
    client->last_online = time(NULL);
    return ret_val;
    
}


//send to user as
void whoelse(char *user)  {
    char message[MESSAGE_CHUNK_SIZE];
    char *p = message;
    struct client_data * client = get_client_from_name(list,user);
    if(client == NULL)  {
        return;
    }
    Node curr = list->head;
    strcat(p,"MUMP\nW\n");
    while(1)    {
        if(curr->data->is_online == 1 && strcmp(curr->data->name,client->name) != 0)  {
            //is user online and not the client
            if(block_matrix[curr->data->id][client->id] == 0)   {
                //the user has not blocked the client
                strcat(p,curr->data->name);
                strcat(p,"\n");
            }
        }
        if(curr->next != NULL)  {
            curr = curr->next;
        }else{
            break;
        }
        
    }
   
    send(client->sock_num,p,strlen(p)+1,0);
    memset(p,0,MESSAGE_CHUNK_SIZE);
}

void whoelse_since(char *buffer)  {
    char message[MESSAGE_CHUNK_SIZE];
    memset(message,0,MESSAGE_CHUNK_SIZE);
    char *p = message;
    char *user = strtok(buffer,"\n");
    char *time_char = strtok(NULL,"");
    int time_since = atoi(time_char);
    
    struct client_data * client = get_client_from_name(list,user);
    if(client == NULL)  {
        return;
    }
    if(time_since <= 0) {
        //not a valid int or person passed in 0
        //either way we return an error
        send(client->sock_num,"MUMP\nE\n4",9,0);
        return;
    }
    Node curr = list->head;
    strcat(p,"MUMP\nW\n");
    time_t curr_time = time(NULL);
    while(1)    {
        if(curr->data->is_online == 1 || curr->data->last_online >= curr_time - time_since)  {
            //is user online and not the client
            if(block_matrix[curr->data->id][client->id] == 0 && strcmp(curr->data->name,client->name) != 0)   {
                //the user has not blocked the client
                strcat(p,curr->data->name);
                strcat(p,"\n");
            }
        }
        if(curr->next != NULL)  {
            curr = curr->next;
        }else{
            break;
        }
        
    }
   
    send(client->sock_num,p,strlen(p)+1,0);
    memset(p,0,MESSAGE_CHUNK_SIZE);
}

void time_out_clients(int time_out) {
    Node curr = list->head;
    
    while(1)    {
        if(curr->data->is_online == 1 && time(NULL) >=  curr->data->last_query + time_out)    {
            //then client has been inactive for timeout period
            
            
            send(curr->data->sock_num,"MUMP\nE\n5\n",10,0);
            
            char *pmessage = malloc(MAX_NAME_LENGTH+20);
            memset(pmessage,0,MAX_NAME_LENGTH+20);
            strcat(pmessage,curr->data->name);
            
            //server_broadcast(pmessage,curr->data->sock_num);
            free(pmessage);
            
        }

        if(curr->next != NULL)  {
            curr = curr->next;
        }else{
            break;
        }
    }
}

void update_set(fd_set * master, int listen_sock)   {

                FD_ZERO(master);
                FD_SET(listen_sock,master);
                Node curr = list->head;
                //timeout old clients
                //time_out_clients(timeout);
                //added logged in clients
                while(1)    {
                    if(curr->data->sock_num != -1)  {
                        FD_SET(curr->data->sock_num,master);
                    }
                    if(curr->next != NULL)  {
                        curr = curr->next;
                    }else{
                        break;
                    }
                }
                //add not logged in clients
                if(not_logged_in_users->length > 0) {
                    curr = not_logged_in_users->head;
                    while(1)    {
                        if(curr->data->sock_num != -1)  {
                            FD_SET(curr->data->sock_num,master);
                        }
                        if(curr->next != NULL)  {
                            curr = curr->next;
                        }else{
                            break;
                        }
                    }
                }

}


void server_broadcast(char *message, int excluded_sock)    {
    Node curr = list->head;
    char final_message[MAX_NAME_LENGTH + 60];
    memset(final_message,0,MAX_NAME_LENGTH+60);
    strcat(final_message,"MUMP\nS\n");
    strcat(final_message,message);
    struct client_data * client = get_client_from_socknum(list,excluded_sock);
    
    while(1)    {
        if(curr->data->sock_num != -1 && curr->data->sock_num != excluded_sock) {
            if(!block_matrix[client->id][curr->data->id])
                send(curr->data->sock_num,final_message,strlen(final_message)+1,0);
        }
        if(curr->next != NULL)  {
            curr = curr->next;
        }else{
            break;
        }
    }
}
//
void send_client_data(char *buffer) {
    char * sender_name =  strtok(buffer,"\n");
    char * client_name = strtok(NULL,"\n");
    if(sender_name == NULL || client_name == NULL)  {
        return;
    }
    struct client_data * client = get_client_from_name(list,client_name);
    struct client_data * sender = get_client_from_name(list,sender_name);
    if(client == NULL)  {
        //no user error
        send(sender->sock_num,"MUMP\nE\n2",9,0);
        return;
    }
    if(!client->is_online)  {
        //not logged in error
        send(sender->sock_num,"MUMP\nE\n8",9,0);
        return;
    }

    if(block_matrix[client->id][sender->id] == 1)   {
        send(sender->sock_num,"MUMP\nE\n3",9,0);
        return;
    }
    //otherwise we send the address and port number of the client to the sender.
    char *message = malloc(MESSAGE_CHUNK_SIZE);
    memset(message,0,MESSAGE_CHUNK_SIZE);
  
   
    strcat(message,"MUMP\nD\n");
    strcat(message, client->name);
    //send a data message to sender
    send(sender->sock_num,message,strlen(message)+1,0);
    free(message);
}

void send_address_to_client(char *buffer)   {
    char * client_name = strtok(buffer,"\n");
    if(client_name == NULL) {
        return;
    }
    struct client_data * client = get_client_from_name(list,client_name);
    char * rest = strtok(NULL,"");
    char *message = malloc(MESSAGE_CHUNK_SIZE);
    memset(message,0,MESSAGE_CHUNK_SIZE);
    strcat(message,"MUMP\nP\n");
    strcat(message,rest);
    send(client->sock_num,message,strlen(message),0);
    free(message);
    return;
}

void end_client_chat(char * buffer) {
    char * sender = strtok(buffer,"\n");
    char * receiver = strtok(NULL,"\n");
    if(sender == NULL || receiver == NULL)  {
        return;
    }
    struct client_data *client = get_client_from_name(list,receiver);
    if(client == NULL)
        return;
    char *message = malloc(MESSAGE_CHUNK_SIZE);
    memset(message,0,MESSAGE_CHUNK_SIZE);
    strcat(message,"MUMP\nF\n");
    strcat(message,sender);
    send(client->sock_num,message,strlen(message)+1,0);
    
    free(message);
}

void ack_client_chat(char *buffer)    {
   
    char * sender = strtok(buffer,"\n");
    char * receiver = strtok(NULL,"\n");
    
    if(sender == NULL || receiver == NULL)  {
        return;
    }
    struct client_data *client = get_client_from_name(list,receiver);
    char *message = malloc(MESSAGE_CHUNK_SIZE);
    memset(message,0,MESSAGE_CHUNK_SIZE);
    strcat(message,"MUMP\nX\n");
    strcat(message,sender);
    send(client->sock_num,message,strlen(message)+1,0);
    free(message);
}