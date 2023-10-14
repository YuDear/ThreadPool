#include "thread_pool.h"
#pragma warning(disable: 6387) //���� 6387 ����



namespace YuDear
{
	ThreadPool::ThreadPool(int min, int max)
	{
		m_min_count = min;		//��С�߳���
		m_max_count = max;		//����߳���
		m_actual_count = min;	//ʵ�ʵ��߳�����
		m_busy_count = 0; 		//æµ���߳�����
		m_exit_count = 0;	    //Ҫ��ֹ���̸߳���

		//�����������߳�
		m_manger_thread = new std::thread(std::bind(&ThreadPool::mangerThread,this));

		//���������߳�
		m_worker_threads.resize(max);
		for (int i = 0; i < min; i++)
			m_worker_threads[i] = new std::thread(std::bind(&ThreadPool::workerThread, this));
	}

	ThreadPool::~ThreadPool()
	{
		m_is_stop = true;

		//�����������������������ϵ��߳�
		m_cv_consumer.notify_all();

		//�ȴ������߳��˳�
		m_manger_thread->join();

		for (auto& Item : m_worker_threads)
		{
			if (Item != 0)
			{
				Item->join();
			}
		};
	}

	void ThreadPool::addTask(TaskBase* task)
	{
		std::unique_lock<std::mutex> locker(m_mutex_pool);
		m_task_queue.push(task);// ��������ӵ����������
		m_cv_consumer.notify_one();// ����һ���߳�
	}

	TaskBase* ThreadPool::takeTask()
	{
		TaskBase* t = nullptr;
		std::unique_lock<std::mutex> locker(m_mutex_task);
		if (m_task_queue.empty() == false)
		{
			t = m_task_queue.front(); //��ͷ��ȡ
			m_task_queue.pop();	   //ȡ��֮���ڴ����������ɾ��
		}
		return t;
	}

	void ThreadPool::workerThread()
	{
		std::unique_lock<std::mutex> locker(m_mutex_pool);
		locker.unlock();
		// �߳̽��뵽�ص���������Ҫ��ͣ��һֱ��������д���������������һֱѭ������
		while (m_is_stop != true)
		{
			// ÿ���̶߳��ܷ��ʵ�������У���������Ӱ�������һ��
			locker.lock();
			// �����ǰ�������Ϊ�գ���ô�������߳�
			while (m_task_queue.empty())
			{
				m_cv_consumer.wait(locker);
				//��� m_nExitCount ���� 0 ��˵����Ҫ��ֹ�߳�
				if (m_exit_count > 0)
				{
					--m_exit_count;
					//��������̴߳�����С�߳̾���ֹһ���̣߳�����Ͳ���ֹ
					if (m_actual_count > m_min_count)
					{
						--m_actual_count; //�̳߳��е��߳�����-1
						threadExit();
						return;
					}
				}

				//����̳߳�ֹͣ�ˣ���ô�����������߳̽��ָ���Ҳû��Ҫִ�������ˣ�������ֱ���˳�
				if (m_is_stop == true)
					return;
			}
			auto task = takeTask(); //�����������ȡ��һ������,��ʼ��������
			++m_busy_count;         //��ʼҪִ�������ˣ�ʹæµ�߳� +1
			locker.unlock();  //һ������ͬʱֻ�ܱ�һ���߳�ִ�У����Բ���д�������棬�������

			task->execute();      //ִ������
			delete task;

			//ִ��������ϣ���ôæµ���߳̾�����һ��������æµ�̸߳� -1
			locker.lock();
			--m_busy_count;
			locker.unlock();
		}
		locker.lock();
	}

	void ThreadPool::mangerThread()
	{
		// �������߳�Ҳ��Ҫ��ͣ�ĸɻ��������Ҳ��дһ��ѭ��
		while (m_is_stop != true)
		{
			m_mutex_pool.lock();
			auto TaskCount = m_task_queue.size();
			auto BusyCount = m_busy_count;
			auto ActualCount = m_actual_count;

			//����̣߳�ÿ�����һ���̣߳�
			//����ĸ��� > ʵ�ʵ��߳� * 2 && ʵ�ʵ��߳��� < ����߳���������ѡ����õĲ��ԣ�
			if (TaskCount > ActualCount * 2 && ActualCount < m_max_count)
			{
				for (int i = 0; i < m_max_count; i++)
				{
					if (m_worker_threads[i] == 0)
					{
						m_worker_threads[i] = new std::thread(std::bind(&ThreadPool::workerThread, this));
						++m_actual_count;
						break;
					}
				}
			}

			//��ֹ�̣߳�ÿ����ֹһ���̣߳�
			//æµ���߳� * 2 < ʵ�ʵ��߳� && ʵ�ʵ��߳��� > ��С�߳���������ѡ����õĲ��ԣ�
			if (BusyCount * 2 < ActualCount && ActualCount > m_min_count)
			{
				//�� m_exit_count +1 Ȼ����һ�������̣߳��ڹ����߳������ж��Ƿ���ֹ�̣߳�Ȼ��ʹ���Լ��˳���
				++m_exit_count;
				m_cv_consumer.notify_one();
			}
			m_mutex_pool.unlock();

			//ÿ��������һ�Σ��߳����� 3 �룩
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
	}

	void ThreadPool::threadExit()
	{
		auto tid = std::this_thread::get_id(); // ��ȡ��ǰ�߳� id
		for (int i = 0; i < m_max_count; i++)
		{
			//��ֹ��ǰ�̣߳��� m_worker_threads ���ҵ���ǰ�̸߳��޳���
			if (m_worker_threads[i]->get_id() == tid)
			{
				m_worker_threads[i]->detach();
				m_worker_threads[i] = 0;
				break;
			}
		}
	}
}