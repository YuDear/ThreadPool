#pragma once
//#include<Windows.h>
#include<functional>
#include<vector>
#include<queue>
#include<thread>
#include<mutex>
#include<condition_variable>
using namespace std;

namespace YuDear
{
	//任务结构
	class TaskBase
	{
	public:
		virtual void execute() = 0;
	};

	template<typename Fn, typename ...Args>
	class Task : public TaskBase
	{
		Fn func;
		std::tuple<Args...> args;
	public:
		Task() = default;
		Task(Fn&& _func, Args&&... _args) : func(std::forward<Fn>(_func)), args(std::forward<Args>(_args)...)
		{
		}

		void execute() override
		{
			// 执行具体任务
			std::apply(func, args);
		}
	};

	//线程池结构
	class ThreadPool
	{
	public:
		ThreadPool(int min, int max);
		~ThreadPool();
		//终止当前线程
		void threadExit();

		//添加任务
		void addTask(TaskBase* task);

		template<typename Fn, typename... Args>
		void addTask(Fn&& f, Args&&... args);
		//取出任务
		TaskBase* takeTask();
private:

		//工作线程（任务处理函数）
		void  workerThread();
		//管理者线程
		void  mangerThread();

	private:
		queue<TaskBase*>		 m_task_queue;		//任务队列
		vector<std::thread*> m_worker_threads;	//工作线程
		std::thread			*m_manger_thread;	//管理者线程

		int m_min_count;		//最小线程数
		int m_max_count;		//最大线程数
		int m_actual_count;		//实际的线程数量
		int m_busy_count; 	    //忙碌的线程数量
		int m_exit_count;		//要终止的线程个数

		std::mutex m_mutex_task; //锁任务队列
		std::mutex m_mutex_pool; //锁线程池

		std::condition_variable m_cv_consumer; //消费者条件变量

		//状态
		bool m_is_stop{ false };                //是否停止线程池（将工作线程和管理者线程停止循环退出线程）
	};


	template<typename Fn, typename ...Args>
	inline void ThreadPool::addTask(Fn&& f, Args && ...args)
	{
		std::unique_lock<std::mutex> locker(m_mutex_task);
		m_task_queue.push(new Task(std::forward<Fn>(f), std::forward<Args>(args)...));	 //将任务添加到任务队列中
		m_cv_consumer.notify_one();// 唤醒一个线程
	}
}
