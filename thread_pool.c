#include <stdio.h>
#include <pthread.h>
#include "task_queue.c"

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <string.h>


typedef struct {
    Queue queue;

    pthread_t *thId_head;

    pthread_t monitorTh; // 管理线程id
    pthread_t acceptTaskTh; // 接收任务的线程id

    pthread_mutex_t mutex; 
    pthread_cond_t  cond;  // 没必要,弄成 *cond

    char *mq_key ; // 消息队列的关键字
    int mqId; 

    int stopFlag; // 中止
    int thNum; // 线程池的数量

    int coreNum; // 核心线程数
    int minNum; // 最少的线程数
    int maxNum; // 最多的线程数

    int waitKillNum;  // 等待,自我终结的线程数
    int busyNum; // 繁忙的线程数

} th_pool;

// 线程池的全局变量
th_pool pool;

void destory_pool(th_pool *pool);
void *thread_fun(void *arg) ;

void *monitor_thread_num(void *arg);
void *accept_task_thread(void *arg);


// 信号函数
void sig_fun(int sigNum) {
    printf("I get signal %d\n", sigNum);

    if (sigNum == SIGUSR1 || sigNum == SIGINT || sigNum == SIGTERM) {
        destory_pool(&pool);
    }

    
}


// 线程池的初始化
void init_pool(th_pool * pool, int minNum, int coreNum, int maxNum, char *mq_key) {

    // 初始化, 任务队列
    init_queue(&pool->queue);

    pthread_mutex_init(&pool->mutex, NULL );

    pthread_cond_init(&pool->cond ,NULL);


    pool->busyNum = 0;
    pool->stopFlag = 0;

    // 创建,n个线程id
    pthread_t *thId_head = (pthread_t *)malloc(sizeof(pthread_t) * maxNum);

    pool->thId_head = thId_head;

    // 最开始,创建, minNum个线程
    for (int i = 0; i < minNum; i++) {
        // 创建,线程函数
        pthread_create(&thId_head[i], NULL,  thread_fun, (void *)pool );
    }

    pool->thNum = minNum;
    pool->minNum = minNum;
    pool->coreNum = coreNum;
    pool->maxNum = maxNum;


    // 初始化,管理线程
    pthread_create(&pool->monitorTh, NULL, monitor_thread_num, (void *)pool);

    if (mq_key == NULL) {
        pool->mq_key = "/thread_pool"; // 消息队列的键
    } else {
        pool->mq_key = mq_key;
    }

    // 初始化消息队列
    pool->mqId = mq_open(pool->mq_key, O_CREAT | O_RDWR, 0777, NULL);
    if (pool->mqId == -1) {
        perror("mq_open ");
        return ;
    }

    // 初始化, 接收任务的线程
    pthread_create(&pool->acceptTaskTh, NULL, accept_task_thread, (void *)pool);

    // 注册,信号函数
    signal(SIGUSR1, sig_fun);
}


// 线程自杀
void thread_own_kill(th_pool *pool) {
    
    // 线程数-1
    --pool->thNum;
    --pool->waitKillNum;

    // 获取,线程id
    int thId = pthread_self();

    pthread_t *thHead = pool->thId_head;
    
    // 遍历线程id, 将指定的id, 删除掉
    for (int i = 0; i < pool->maxNum; i++) {
        if (thHead[i] == thId) {
            thHead[i] = 0;
            break; 
        }
    }
    printf("I finish thread\n");
    pthread_mutex_unlock(&pool->mutex);
    pthread_exit(NULL);
    
}


// 从任务池中,取出,任务
void *thread_fun(void *arg) {
    th_pool *pool = (th_pool *)arg;

    // printf("%d\n", pool->thNum);

    while (1) {
        // sleep(100);
        // printf("进入\n");
        pthread_mutex_lock(&pool->mutex);

        // 空的,并且, 不终止, 就进行等待 (之前,还以为,不空,就进行等待)
        while ( isEmpty_queue(&pool->queue) && ! pool->stopFlag ) {
            pthread_cond_wait(&pool->cond, &pool->mutex);

            // 自杀
            if (pool->waitKillNum) {
                thread_own_kill(pool);
            }
        }

        // 中止
        if (pool->stopFlag == 1) {
            thread_own_kill(pool);
        }

        // 从消息队列中,取出任务
        // void *(*fun)(void *);
        TASK task = pop_queue(&pool->queue);

        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutex);

        // 执行任务
        task.task_work(task.arg);

        // 繁忙的线程
        pool->busyNum--;

    }

}

// 结束,线程池
void destory_pool(th_pool *pool) {

    pool->stopFlag = 1;

    // 先获取,锁. 对任务队列,进行操作
    pthread_mutex_lock(&pool->mutex);

    // 通知,所有人,退出
    pthread_cond_broadcast(&pool->cond);

    pthread_mutex_unlock(&pool->mutex);

    // 等待,线程的结束

    for(int i=0; i < pool->thNum; i++) {
        pthread_join(pool->thId_head[i], NULL);
    }
    pool->thNum = 0;

    // 销毁,锁
    pthread_mutex_destroy(&pool->mutex);

    // 销毁,条件变量
    pthread_cond_destroy(&pool->cond);

    // 销毁,线程id
    free(pool->thId_head);
    
    printf("I finish 线程池\n");
    exit(-1);

}


// 获取,空闲的线程id
pthread_t *get_free_thId(pthread_t * thHead, int thSize) {
    for (int i = 0; i < thSize; i++) {
        if (thHead[i] == 0) { // 该位置,为空闲线程
            return thHead + i;
        }
    }
}

// 管理,线程的数量
void *monitor_thread_num(void *arg) {
    
    th_pool *pool = (th_pool *)arg;

    while (1) {
        printf("check time: %ld\n", pthread_self());
        int taskNum = getSize_queue(&pool->queue);

        if (taskNum > 0 ) {
            int totalNum = 0; // 总共的线程总数

            // 任务队列中,有任务
            if (isFull_queue(&pool->queue)) { // 如果,是满的
                // 创建,非核心进程
                totalNum = pool->maxNum;

                printf("full 满了\n");

            } else {
                // 创建, 核心进程, 进行处理
                totalNum = pool->coreNum;
            }

            for (int i=pool->thNum; i< totalNum && taskNum >0; i++ ) {
                // 获取,空闲的id
                pthread_t * freeId = get_free_thId(pool->thId_head, pool->maxNum);

                pthread_create(freeId, NULL,  thread_fun, (void *)pool );
                
                // 当前,线程数+1
                ++pool->thNum;

            }
        } else {
            // 任务队列中,没有任务
            int killNum = 0; // 杀死的线程数
            
            // 如果,有繁忙的线程. 将线程数,调整至,核心线程数
            if (pool->busyNum ) {
                killNum = pool->thNum - pool->coreNum; 
            } else { // 都是空闲的, 将线程数,保持到最小
                killNum = pool->thNum - pool->minNum; 
            }
            
            if (killNum < 0) {
                killNum = 0;
            }
            pool->waitKillNum = killNum;

            // 通知,自杀
            for (int i = 0; i < killNum; i++) {
                pthread_cond_signal(&pool->cond);
            }
        }
        printf("当前线程数: %d\n", pool->thNum);
        printf("当前任务数: %d\n", getSize_queue(&pool->queue));

        printf("繁忙的线程数: %d\n", pool->busyNum);
        printf("待杀死的线程数: %d\n\n", pool->waitKillNum);

        sleep(5); // 每隔5秒,查看一次

    }
}

// 添加到,任务队列中
void add_task_pool(th_pool *pool, TASK task){
    pthread_mutex_lock(&pool->mutex);

    push_queue(&pool->queue, task);
    printf("任务数: %d\n", getSize_queue(&pool->queue)) ;

    // 通知, 已经有任务,添加了
    pthread_cond_signal(&pool->cond);
    
    pthread_mutex_unlock(&pool->mutex);

}

// 接收,用户,发送的任务
void *accept_task_thread(void *arg) {
    th_pool *pool = (th_pool *)arg;

    char recvData[10240] = {'\0'};

    TASK task;
    int num = 11;
    task.arg = &num;

    while (1) {
        printf("I wait task now\n");

        int ret = mq_receive(pool->mqId, recvData, sizeof(recvData), 0);
        // printf("%d\n", pool->mqId);
        if (ret == -1) {
            perror("mq_receive ");
            return NULL;
        }

        // 判断,调用的是,哪个方法
        if (strcmp(recvData, "fun1") == 0) {
            task.task_work = f1;

        } else if (strcmp(recvData, "fun2") == 0) {
            task.task_work = f2;
        } else {
            continue;
        }
        printf("I recv task: %s\n", recvData);

        // pthread_mutex_lock(&pool->mutex);
        if (! isFull_queue(& pool->queue)) {
            add_task_pool(pool, task);
        
        } else {
            printf("任务队列,满了\n" );
        }
        // pthread_mutex_unlock(&pool->mutex);
    
    }

}


int main(int argc, char** argv) {

    int coreNum = 8; // 核心线程数
    int minNum = 3;
    int maxNum = 10;

    char *mq_key = NULL;
    if (argc == 2) { // 传入了,消息队列的键
        mq_key = argv[1];
    }
    
    init_pool(&pool, minNum,coreNum, maxNum, mq_key);

    
    pthread_join(pool.monitorTh, NULL);

}


