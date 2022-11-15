#ifndef LIST_H
#define LIST_H
#include <pthread.h>
#include <unistd.h>

#define MAX_NAME_LENGTH 60
#define MESSAGE_CHUNK_SIZE 8192
#define MAX_INT_STRING_LENGTH 16
typedef struct client_node *Node;

struct client_list  {
    Node head, tail;
    pthread_mutex_t  mutex;
    int length;
};


struct client_data  {
    int id;
    int sock_num;
    int logged_in_times;
    u_int8_t is_online;

    time_t last_online;
    time_t last_query;
    //the time that the client is allowed to attempt
    //logging in again
    time_t blocked_until;

    u_int32_t addr;
    u_int16_t port;

    pthread_t sender;
    pthread_t receiver;

    char * name;
    char * password;
    struct queue * messages;
};

struct client_node  {
    struct client_data * data;
    Node next;
};
struct client_list *  list_create();
void list_add(Node client, struct client_list * client_list);
void list_remove(struct client_data * client, struct client_list * client_list);
struct client_data * get_client_from_socknum(struct  client_list * list,int sock);
Node create_Node(char * username, char *password);
Node create_not_logged_in_Node(int sock_num, u_int32_t address, u_int16_t port);
Node create_client_node(int sock_number, char* name, int server);
//returns 0 if no client with that name exists
//returns 1 if client exists but is offline
struct client_data * get_client_from_name(struct  client_list * list, char * name);
struct client_data * get_client_by_id(struct  client_list * this_list,int id);
struct client_data * get_client_by_thread(struct  client_list * this_list,pthread_t thread, int send);
//returns 0 if no client with that name exists
//returns 1 if successful
//only used if client is offline, when the client comes online the message queue will be emptied.
int add_message_to_client(struct client_list * list,char * name, char * message);
#endif