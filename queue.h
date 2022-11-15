#ifndef QUEUE_H
#define QUEUE_H
#include <string.h>
typedef struct queue_node * QNode;
struct queue {
    QNode head,tail;
    pthread_mutex_t  mutex;
    int length;
};

struct queue_node {
    char *message;
    QNode next;
};

struct queue * create_queue();

QNode create_QNode(char *message);

void enqueue(struct queue * q,QNode node);

QNode dequeue(struct queue *q);

void free_QNode(QNode node);

#endif