#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <pthread.h>
struct queue * create_queue() {
    struct queue * q = malloc(sizeof(struct queue));
    pthread_mutex_init(&q->mutex,NULL);
    q->head = NULL;
    q->tail = NULL;
    q->length = 0;
    return q;
}

QNode create_QNode(char *message)   {
    QNode node = malloc(sizeof(struct queue_node));
    char * our_message = malloc(4096);
    memset(our_message,0,4096);
    strcpy(our_message,message);
    node->message = our_message;
    node->next = NULL;
 
    return node;
}

void enqueue(struct queue * q,QNode node)    {
    pthread_mutex_lock(&q->mutex);
    if(q->length == 0) {
        q->length++;
        q->head = node;
        q->tail = node;
        pthread_mutex_unlock(&q->mutex);

        return;
    }
    q->length++;
    q->tail->next = node;
    q->tail = node;
    pthread_mutex_unlock(&q->mutex);
}

QNode dequeue(struct queue *q)  {
   // printf("message:%s\n\n",q->head->message);
    pthread_mutex_lock(&q->mutex);
    if(q->length == 0)  {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
     QNode temp = q->head;
    if(q->length == 1)  {
        q->head = NULL;
        q->tail = NULL;
        q->length = 0;
        pthread_mutex_unlock(&q->mutex);
        return temp;
    }
    q->length--;
    q->head = q->head->next;
    pthread_mutex_unlock(&q->mutex);
    return temp;
    
}

void free_QNode(QNode node) {
    free(node->message);
    node->message = NULL;
    node->next = NULL;
    free(node);
}