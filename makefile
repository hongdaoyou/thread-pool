
ALL:th_pool send_task


# 线程池
th_pool:thread_pool.c
	gcc $^ -o $@ -lpthread -lrt -g


send_task:send_task.c
	gcc $^ -o $@ -lrt


