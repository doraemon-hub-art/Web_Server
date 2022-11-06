# 线程同步机制封装类

## 信号量类-sem
>- 原函数相关方法
>   - sem_init()        初始化信号量
>       - 要指定的信号量，是否为该进程内的局部信号量，信号量的初始值
>   - sem_destory()     销毁信号量
>   - sem_wait()        原子操作将信号量-1
>   - sem_post()        原子操作将信号量+1
> - 本项目中用来控制任务数量
***
## 互斥锁量类-locker
>- 原函数相关方法
>   - pthread_mutex_init()     互斥量初始化
>   - pthread_mutex_lock()     获取互斥量p
>   - pthread_mutex_unlock()   释放互斥量v
>   - pthread_destory()        销毁互斥量
***
## 条件变量类-cond
>- 原函数相关方法
>   - pthread_cond_init         初始化条件变量
>   - pthread_cond_signal       唤醒一个等待者
>   - pthread_cond_broadcast     唤醒多个等待者-广播
>   - pthread_cond_timewait     超时唤醒
>   - pthread_cond_destory      销毁条件变量