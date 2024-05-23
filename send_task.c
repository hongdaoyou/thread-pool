/* 发送,任务请求 */

#include <mqueue.h>
#include <stdio.h>


int main(int argc, char**argv) {

    char *mq_key = "/thread_pool";

    // 初始化消息队列
    int mqId = mq_open(mq_key, O_CREAT | O_RDWR, 0777, NULL);
    if (mqId == -1) {
        perror("mq_open ");
        return -1;
    }

    char *funName = argv[1]; // 要发送的函数名

    int ret = mq_send(mqId, funName, sizeof(funName), 0);
    if (ret == -1) {
        perror("send fail");  // 发送失败
    } else {
        printf("发送成功\n");
    }

}
