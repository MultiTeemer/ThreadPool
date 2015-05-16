#include <boost\lexical_cast.hpp>

#include "ThreadPool.h"

ThreadPool::BaseThread::BaseThread()
    :
    task(nullptr),
    last_result(0),
    last_task_id(0)
{
}

ThreadPool::BaseThread::BaseThread(const ThreadPool::BaseThread& obj)
    :
    task(obj.task),
    last_task_id(obj.last_task_id)
{
}

void ThreadPool::BaseThread::performAndReturn(boost::unique_lock<boost::try_mutex>& lock)
{
    last_result = (*task)();
    task = nullptr;

    task_performed.notify_one();

    result_accuired.wait(lock);
}

void ThreadPool::HotThread::performTasks()
{
    boost::unique_lock<boost::try_mutex> lock(mutex);
    
    while (true)
    {
        task_recieved.wait(lock);

        performAndReturn(lock);
    }
}

void ThreadPool::FreeThread::performTasks()
{
    boost::unique_lock<boost::try_mutex> lock(mutex);

    do
    {
        performAndReturn(lock);
    } while (task_recieved.wait_for(lock, boost::chrono::seconds(timeout)) != boost::cv_status::timeout);

    thread_death.notify_all();
}

ThreadPool::FreeThread::FreeThread(unsigned timeout)
    :
    BaseThread(),
    timeout(timeout)
{}

ThreadPool::FreeThread::FreeThread(const FreeThread& other)
    :
    BaseThread(),
    timeout(other.timeout)
{}

ThreadPool::ThreadPool(int _count, int _timeout)
    :
    hotThreads(_count),
    count(_count),
    timeout(_timeout),
    taskCounter(0)
{
    for (int i = 0; i < _count; ++i)
    {
        boost::thread(boost::bind(&ThreadPool::HotThread::performTasks, boost::ref(hotThreads[i])));
    }
}

void ThreadPool::addTask(Callable* task)
{
    for (int i = 0; i < count; ++i)
    {
        if (tryAssignTask(task, hotThreads[i]))
        {
#ifdef TESTING
            string msg = boost::lexical_cast<string>(hotThreads[i].last_task_id) + " h\n";

            output << msg;
#endif
            return;
        }
    }

    int j = 0;
    for (auto i = freeThreads.begin(); i != freeThreads.end(); ++i, ++j)
    {
        if (tryAssignTask(task, *i))
        {
#ifdef TESTING
            string msg = boost::lexical_cast<string>(i->last_task_id) + " f\n";

            output << msg;
#endif
            return;
        }
    }

    boost::mutex::scoped_lock lock(listSync);

    freeThreads.push_front(FreeThread(timeout));

    tryAssignTask(task, freeThreads.front());

#ifdef TESTING
    string msg = boost::lexical_cast<string>(freeThreads.front().last_task_id) + " n\n";

    output << msg;
#endif

    freeThreadsExecutions.push_front(new boost::thread(boost::bind(&ThreadPool::FreeThread::performTasks, freeThreads.front())));
}

void ThreadPool::killTask(unsigned id)
{
    
}

bool ThreadPool::tryAssignTask(Callable* task, ThreadPool::BaseThread& thread)
{
    if (thread.mutex.try_lock())
    {        
        thread.task = task;
        thread.last_task_id = ++taskCounter;
        workingThreads[thread.last_task_id] = &thread;

        boost::thread(boost::bind(&ThreadPool::waitForResult, this, _1), boost::ref(thread));
        
        thread.mutex.unlock();

        return true;
    }

    return false;
}

void ThreadPool::waitForResult(ThreadPool::BaseThread& thread)
{
    boost::unique_lock<boost::try_mutex> lock(thread.mutex);

    thread.task_recieved.notify_one();

    thread.task_performed.wait(lock);

    string msg = boost::lexical_cast<string>(thread.last_task_id) + " " + boost::lexical_cast<string>(thread.last_result) + "\n";

#ifdef TESTING

    output << msg;

#else
    
    cout << msg;

#endif

    workingThreads.erase(thread.last_task_id);

    thread.result_accuired.notify_one();
}

void ThreadPool::waitForFreeThreadDeath(ThreadPool::FreeThread& thread)
{
    boost::unique_lock<boost::try_mutex> lock(thread.mutex);

    thread.thread_death.wait(lock);

    boost::mutex::scoped_lock listLock(listSync);

    auto j = freeThreadsExecutions.begin();
    for (auto i = freeThreads.begin(); i != freeThreads.end(); ++i, ++j)
    {
        if (thread.last_task_id == i->last_task_id)
        {
            freeThreads.erase(i);
            freeThreadsExecutions.erase(j);

            break;
        }
    }
}
