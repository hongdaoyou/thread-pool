#ifndef _TASK_QUEUE_
#define _TASK_QUEUE_
#endif

#include <stdio.h>
#include <unistd.h>

// typedef void*(*TASK_FU)(void *);
typedef struct {
    void*(*task_work)(void *);
    void *arg;
} TASK;

#define QUEUE_SIZE 6 // 其实,只能存储5个

typedef struct {
    int front; // 指向, 当前要出队列的元素
    int end ; // 指向,下一个要插入的地方

    TASK data[QUEUE_SIZE];
} Queue;

void init_queue(Queue *queue);
int push_queue(Queue *queue, TASK fun) ;
TASK pop_queue(Queue *queue );
int getSize_queue(Queue *queue);

int isFull_queue(Queue *queue ) ;
int isEmpty_queue(Queue *queue ) ;


void init_queue(Queue *queue) {
    queue->front = 0;
    queue->end = 0;
}

// 入队
int push_queue(Queue *queue, TASK fun) {
    if (! isFull_queue(queue)) {
        queue->data[queue->end] = fun; // 插入
    
        queue->end = (queue->end + 1)% QUEUE_SIZE; // 指向,下一个要插入的地方
    }
}

// 出队
TASK pop_queue(Queue *queue ) {
    if (! isEmpty_queue(queue)) {
        TASK tmp = queue->data[queue->front]; // 取出,队尾的元素

        queue->front = (queue->front + 1) % QUEUE_SIZE;
        // printf("222\n");
        
        return tmp;
    } else {
        // printf("111\n");
        TASK empty_task = {NULL, NULL};
        return empty_task;
        // return NULL;
    }
}


int getSize_queue(Queue *queue) {
    return (queue->end - queue->front + QUEUE_SIZE )% QUEUE_SIZE;

}

// 任务队列,是否满的
int isFull_queue(Queue *queue ) {
    // 队尾加1, 与队头相同
    if ( (queue->end + 1)% QUEUE_SIZE == queue->front ) {
        return 1;
    } else {
        return 0;
    }
}

// 任务队列,是否空的
int isEmpty_queue(Queue *queue ) {
    if (queue->end == queue->front ) {
        return 1;
    } else {
        return 0;
    }
}

void *f1(void *arg) {
    int *num = (int *)arg;
    printf("任务开始 f1: %d\n", *num);
    sleep(100);
    printf("任务结束 f1\n");

}

void *f2(void *arg) {
    char *str = (char *)arg;
    printf("f2: %s\n", str);

}


// int main() {
//     Queue queue;
//     init_queue(&queue);

//     int a = 10;

//     TASK task;
//     task.task_work=f1;
//     task.arg = &a;

//     for (int i = 0; i < QUEUE_SIZE+1; i++) {
   
//         // 入队列
//         push_queue(&queue, task);

//         // 队列中的元素个数
//         int ret =  getSize_queue(&queue);
//         printf("%d\n", ret);

//     }
//     task = pop_queue(&queue);
//     if (task.task_work == f1) {
//         printf("11\n" );
//     }
//     task.task_work(task.arg);

// }

