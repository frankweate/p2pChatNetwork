#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include "list.h"
#include <string.h>
struct client_list *  list_create()  {
    struct client_list * list = malloc(sizeof(struct client_list));
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    pthread_mutex_init(&list->mutex,NULL);
    
    return list;
}

void list_add(Node client, struct client_list * list)  {
    pthread_mutex_lock(&list->mutex);
    if(list->length == 0)    {
        list->head = client;
        list->tail = client;
        list->length++;
        pthread_mutex_unlock(&list->mutex);
        return;
    }
    list->tail->next = client;
    list->tail = client;
    list->length++;
    pthread_mutex_unlock(&list->mutex);
}

void list_remove(struct client_data* client, struct client_list * list)  {
    pthread_mutex_lock(&list->mutex);
    Node remove_node;
    if(list->length == 0)    {
        pthread_mutex_unlock(&list->mutex);
        return;
    }
    if(list->length == 1 && client->sock_num == list->head->data->sock_num)    {
        free(client->messages);
        free(client);
        free(list->head);
        list->head = NULL;
        list->tail = NULL;
        list->length = 0;
        pthread_mutex_unlock(&list->mutex);
        return;
    }
    if(client->sock_num == list->head->data->sock_num) {
        remove_node = list->head;
        list->head = list->head->next;
        list->length--;
        free(client->messages);
        free(client);
        free(remove_node);
        pthread_mutex_unlock(&list->mutex);
        return;
    }
    Node curr = list->head;
    Node prev = NULL;
    while(1)    {
        if(curr->data->sock_num == client->sock_num) {
            if(curr->next != NULL)  {
                prev->next = curr->next;
            }else{
                prev->next = NULL;
                list->tail = prev;
            }
            free(client->messages);
            free(client);
            free(curr);
            list->length--;
            pthread_mutex_unlock(&list->mutex);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}



Node create_Node(char * username, char *password)  {
    Node client = malloc(sizeof(struct client_node));
    client->data = malloc(sizeof(struct client_data));
    //address = -1, means that client exists but is not online
    client->data->blocked_until = 0;
    client->data->logged_in_times = 0;
    client->data->sock_num = -1;
    client->data->is_online = 0;
    client->data->last_online = 0;
    client->data->messages = create_queue();
    client->data->name = username;
    client->data->password = password;
    client->next = NULL;
    return client;
}

Node create_not_logged_in_Node(int sock_number,u_int32_t address, u_int16_t port)  {
    Node client = malloc(sizeof(struct client_node));
    client->data = malloc(sizeof(struct client_data));
    //address = -1, means that client exists but is not online
    client->data->addr = address;
    client->data->port = port;
    client->data->sock_num = sock_number;
    client->data->id = 0;
    client->next = NULL;
    client->data->last_query = 0;
    return client;
}

Node create_client_node(int sock_number, char* name, int server)    {
    Node client = malloc(sizeof(struct client_node));
    client->data = malloc(sizeof(struct client_data));
    //address = -1, means that client exists but is not online
    client->data->logged_in_times = server;
    client->data->name = malloc(60);
    strncpy(client->data->name,name,60);
    client->data->sock_num = sock_number;
    client->data->id = -1;
    client->next = NULL;
    client->data->last_query = 0;
    client->data->messages = create_queue();
    return client;
}

struct client_data * get_client_from_socknum(struct  client_list * list,int sock)  {
    pthread_mutex_lock(&list->mutex);
    if(list->length == 0 || sock == -1)   {
        //no clients with that name
        //make sure that -1 isn't passed in and returns an offline client
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }
     
    Node curr = list->head;
   
    while(1)    {
       
        if(curr->data->sock_num == sock)    {
            pthread_mutex_unlock(&list->mutex);
            return curr->data;
        }
     
        if(curr->next != NULL)  {
          
            curr = curr->next;
        }else {
           
            break;
        }
    //no clients with that name
    
    }
    pthread_mutex_unlock(&list->mutex);
    return NULL;
}

//returns 0 if no client with that name exists
//returns 1 if client exists but is offline
struct client_data * get_client_from_name(struct  client_list * list, char * name)  {
    pthread_mutex_lock(&list->mutex);
   
    if(list->length == 0)   {
        //no clients with that name
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }
    Node curr = list->head;
    
    while(1)    {
    
        
        if(strncmp(curr->data->name,name, MAX_NAME_LENGTH) == 0)  {
            pthread_mutex_unlock(&list->mutex);
            return curr->data;
        }
        if(curr != list->tail)  {
            curr = curr->next;
        }else {
            break;
        }
    }
    //no clients with that name
    pthread_mutex_unlock(&list->mutex);
    return NULL;
}

struct client_data * get_client_by_id(struct  client_list * list,int id)  {
    pthread_mutex_lock(&list->mutex);
    if(list->length == 0 || id < 0)   {
        //no clients with that name
        //make sure that -1 isn't passed in and returns an offline client
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }
     
    Node curr = list->head;
   
    while(1)    {
       
        if(curr->data->id == id)    {
            pthread_mutex_unlock(&list->mutex);
            return curr->data;
        }
     
        if(curr->next != NULL)  {
          
            curr = curr->next;
        }else {
           
            break;
        }
    //no clients with that name
    
    }
    pthread_mutex_unlock(&list->mutex);
    return NULL;
}

struct client_data * get_client_by_thread(struct  client_list * list,pthread_t thread, int send)  {
    pthread_mutex_lock(&list->mutex);
    if(list->length == 0 || thread < 0)   {
        //no clients with that name
        //make sure that -1 isn't passed in and returns an offline client
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }
     
    Node curr = list->head;
   
    while(1)    {
        
        if(send)    {
            if(thread == curr->data->sender)    {
                pthread_mutex_unlock(&list->mutex);
                return curr->data;
            }
        }else
        {
            if(thread == curr->data->receiver)    {
                return curr->data;
            }
        }
        
     
        if(curr->next != NULL)  {
          
            curr = curr->next;
        }else {
           
            break;
        }
    //no clients with that name
    
    }
    pthread_mutex_unlock(&list->mutex);
    return NULL;
}


int add_message_to_client(struct client_list * list,char * name, char * message)    {
    pthread_mutex_lock(&list->mutex);
     if(list->length == 0)   {
        //no clients with that name
        pthread_mutex_unlock(&list->mutex);
        return 0;
    }
   

    Node curr = list->head;
    while(1)    {
        if(strncmp(curr->data->name,name, MAX_NAME_LENGTH) == 0)  {
            //client to send message to.
            
            enqueue(curr->data->messages,create_QNode(message));
            pthread_mutex_unlock(&list->mutex);
            return 1;
        }
        if(curr != list->tail)  {
            curr = curr->next;
        }else {
            break;
        }
    }
    //no clients with that name
    pthread_mutex_unlock(&list->mutex);
    return 0;
}
