#ifndef THREADPOOL_H
#define THREADPOOL_H

void pool_init(int max_thread_num);
void pool_add_worker(void *(*process) (void *arg), void *arg);
void pool_destroy();

#endif