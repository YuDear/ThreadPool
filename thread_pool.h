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
	//����ṹ
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
			// ִ�о�������
			std::apply(func, args);
		}
	};

	//�̳߳ؽṹ
	class ThreadPool
	{
	public:
		ThreadPool(int min, int max);
		~ThreadPool();
		//��ֹ��ǰ�߳�
		void threadExit();

		//�������
		void addTask(TaskBase* task);

		template<typename Fn, typename... Args>
		void addTask(Fn&& f, Args&&... args);
		//ȡ������
		TaskBase* takeTask();
private:

		//�����̣߳�����������
		void  workerThread();
		//�������߳�
		void  mangerThread();

	private:
		queue<TaskBase*>		 m_task_queue;		//�������
		vector<std::thread*> m_worker_threads;	//�����߳�
		std::thread			*m_manger_thread;	//�������߳�

		int m_min_count;		//��С�߳���
		int m_max_count;		//����߳���
		int m_actual_count;		//ʵ�ʵ��߳�����
		int m_busy_count; 	    //æµ���߳�����
		int m_exit_count;		//Ҫ��ֹ���̸߳���

		std::mutex m_mutex_task; //���������
		std::mutex m_mutex_pool; //���̳߳�

		std::condition_variable m_cv_consumer; //��������������

		//״̬
		bool m_is_stop{ false };                //�Ƿ�ֹͣ�̳߳أ��������̺߳͹������߳�ֹͣѭ���˳��̣߳�
	};


	template<typename Fn, typename ...Args>
	inline void ThreadPool::addTask(Fn&& f, Args && ...args)
	{
		std::unique_lock<std::mutex> locker(m_mutex_task);
		m_task_queue.push(new Task(std::forward<Fn>(f), std::forward<Args>(args)...));	 //��������ӵ����������
		m_cv_consumer.notify_one();// ����һ���߳�
	}
}
