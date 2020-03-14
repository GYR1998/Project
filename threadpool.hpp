
//线程池的实现：线程安全的任务队列+线程
//任务类：
//   class MyTask
//线程池类：
//   class ThreadPool
#ifndef _M_POOL_H_
#define _M_POOL_H_
#include<iostream>
#include<thread>
#include<queue>
#include<vector>
#include<pthread.h>
#include<sstream>
#include<time.h>
#include<stdlib.h>
#include<unistd.h>
using namespace std;

#define MAX_THREAD 5        //最大的线程数量
#define MAX_QUEUE 10        //最大的结点数量，任务队列的结点数量

//任务类中一共有两个成员：要处理的数据、要处理的方法
//这两个成员都需要用户传入
//一个线程中用啥方法处理都需要用户写

typedef void (*handler_t)(int);      //定义函数指针类型
class ThreadTask
{
      private:
        int _data;
        handler_t _handler;    
      public:
        ThreadTask(int data,handler_t handle):_data(data),_handler(handle)  //handler是用户传入的处理函数
        {}

        void SetTask(int data,handler_t handle)
        {
              _data=data;
              _handler=handle;
              return;
        }
        

        //Run调用实际的任务处理函数handler_t
        void TaskRun()
        {
              return _handler(_data);
        }
};


//线程池
class ThreadPool
{
      private:
        //用于实现线程安全的用户队列
         queue<ThreadTask> _queue;
         int _capacity;
         pthread_mutex_t _mutex;
         pthread_cond_t _cond_pro;
         pthread_cond_t _cond_con;
         
         
         int _thr_max;        //控制线程的最大数量

      private:
         void thr_start()
         {

              //1.线程安全的任务出队（若没有任务且退出标志位为真，就退出）
              //2.处理任务
              while(1)
              {
                     pthread_mutex_lock(&_mutex);
                     while(_queue.empty())
                     {
                            pthread_cond_wait(&_cond_con,&_mutex);
                     }
                     ThreadTask tt=_queue.front();
                     _queue.pop();
                     pthread_mutex_unlock(&_mutex);
                     pthread_cond_signal(&_cond_pro);

                     //任务处理应该放在解锁之后，否则会造成同一时间只有一个线程在处理任务，其他线程却在等待
                     tt.TaskRun();
                    
              }
         }
      public:
        ThreadPool(int maxq=MAX_QUEUE,int maxt=MAX_THREAD):_capacity(maxq),_thr_max(maxt)
        {
              pthread_mutex_init(&_mutex,NULL);
              pthread_cond_init(&_cond_con,NULL);
              pthread_cond_init(&_cond_pro,NULL);
        }

        ~ThreadPool()
        {
              pthread_mutex_destroy(&_mutex);
              pthread_cond_destroy(&_cond_con);
              pthread_cond_destroy(&_cond_pro);
        }


        //线程池的初始化：创建指定数量的线程
        bool PoolInit()
        {   
              //thr_start()是一个入口函数
              for(int i=0;i<_thr_max;i++)
              {
                    thread thr(&ThreadPool::thr_start,this);   //thread(thr_start)这样是不行的，这样会报错，需要改成这样
                    thr.detach();      //因为我们不关心线程的返回值，所以直接分离
              }
              return true;
        }

        //添加任务,其实就是入队,入队就要保证线程安全
        bool TaskPush(ThreadTask &tt)
        {
              pthread_mutex_lock(&_mutex);
              while(_queue.size()==_capacity)
              {
                    pthread_cond_wait(&_cond_pro,&_mutex);
              }
              _queue.push(tt);
              pthread_mutex_unlock(&_mutex);
              pthread_cond_signal(&_cond_con);
              
              return true;
        }

};

#endif
