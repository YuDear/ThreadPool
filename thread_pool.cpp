#include "thread_pool.h"
#pragma warning(disable: 6387) //屏蔽 6387 警告



namespace YuDear
{
	ThreadPool::ThreadPool(int min, int max)
	{
		m_min_count = min;		//最小线程数
		m_max_count = max;		//最大线程数
		m_actual_count = min;	//实际的线程数量
		m_busy_count = 0; 		//忙碌的线程数量
		m_exit_count = 0;	    //要终止的线程个数

		//创建管理者线程
		m_manger_thread = new std::thread(std::bind(&ThreadPool::mangerThread,this));

		//创建工作线程
		m_worker_threads.resize(max);
		for (int i = 0; i < min; i++)
			m_worker_threads[i] = new std::thread(std::bind(&ThreadPool::workerThread, this));
	}

	ThreadPool::~ThreadPool()
	{
		m_is_stop = true;

		//唤醒所有阻塞在条件变量上的线程
		m_cv_consumer.notify_all();

		//等待所有线程退出
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
		m_task_queue.push(task);// 将任务添加到任务队列中
		m_cv_consumer.notify_one();// 唤醒一个线程
	}

	TaskBase* ThreadPool::takeTask()
	{
		TaskBase* t = nullptr;
		std::unique_lock<std::mutex> locker(m_mutex_task);
		if (m_task_queue.empty() == false)
		{
			t = m_task_queue.front(); //从头部取
			m_task_queue.pop();	   //取出之后在从任务队列中删除
		}
		return t;
	}

	void ThreadPool::workerThread()
	{
		std::unique_lock<std::mutex> locker(m_mutex_pool);
		locker.unlock();
		// 线程进入到回调函数后，需要不停的一直读任务队列处理任务，所以这里一直循环（）
		while (m_is_stop != true)
		{
			// 每个线程都能访问到任务队列，所以这里加把锁保护一下
			locker.lock();
			// 如果当前任务队列为空，那么就阻塞线程
			while (m_task_queue.empty())
			{
				m_cv_consumer.wait(locker);
				//如果 m_nExitCount 大于 0 就说明需要终止线程
				if (m_exit_count > 0)
				{
					--m_exit_count;
					//如果存活的线程大于最小线程就终止一个线程，否则就不终止
					if (m_actual_count > m_min_count)
					{
						--m_actual_count; //线程池中的线程数量-1
						threadExit();
						return;
					}
				}

				//如果线程池停止了，那么阻塞的所有线程将恢复，也没必要执行任务了，在这里直接退出
				if (m_is_stop == true)
					return;
			}
			auto task = takeTask(); //从任务队列中取出一个任务,开始处理任务
			++m_busy_count;         //开始要执行任务了，使忙碌线程 +1
			locker.unlock();  //一个任务同时只能被一个线程执行，所以不用写到锁里面，这里解锁

			task->execute();      //执行任务
			delete task;

			//执行任务完毕，那么忙碌的线程就少了一个，所以忙碌线程该 -1
			locker.lock();
			--m_busy_count;
			locker.unlock();
		}
		locker.lock();
	}

	void ThreadPool::mangerThread()
	{
		// 管理者线程也需要不停的干活，所以这里也是写一个循环
		while (m_is_stop != true)
		{
			m_mutex_pool.lock();
			auto TaskCount = m_task_queue.size();
			auto BusyCount = m_busy_count;
			auto ActualCount = m_actual_count;

			//添加线程（每次添加一个线程）
			//任务的个数 > 实际的线程 * 2 && 实际的线程数 < 最大线程数（可以选择更好的策略）
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

			//终止线程（每次终止一个线程）
			//忙碌的线程 * 2 < 实际的线程 && 实际的线程数 > 最小线程数（可以选择更好的策略）
			if (BusyCount * 2 < ActualCount && ActualCount > m_min_count)
			{
				//让 m_exit_count +1 然后唤醒一个工作线程（在工作线程里面判断是否终止线程，然后使其自己退出）
				++m_exit_count;
				m_cv_consumer.notify_one();
			}
			m_mutex_pool.unlock();

			//每隔三秒检测一次（线程休眠 3 秒）
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
	}

	void ThreadPool::threadExit()
	{
		auto tid = std::this_thread::get_id(); // 获取当前线程 id
		for (int i = 0; i < m_max_count; i++)
		{
			//终止当前线程，从 m_worker_threads 中找到当前线程给剔除掉
			if (m_worker_threads[i]->get_id() == tid)
			{
				m_worker_threads[i]->detach();
				m_worker_threads[i] = 0;
				break;
			}
		}
	}
}