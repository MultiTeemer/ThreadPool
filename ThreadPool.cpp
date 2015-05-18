#include <boost\lexical_cast.hpp>

#include "ThreadPool.h"

ThreadPool::BaseThread::BaseThread()
    :
    task(nullptr),
    last_result(0),
    last_task_id(0),
    watcher_thread(nullptr),
    execution_thread(nullptr)
{
}

ThreadPool::BaseThread::BaseThread(const ThreadPool::BaseThread& obj)
    :
    task(obj.task),
    last_task_id(obj.last_task_id),
    watcher_thread(nullptr),
    execution_thread(nullptr)
{
}

void ThreadPool::BaseThread::performInParallel()
{
    last_result = (*task)();
    task = nullptr;
}

void ThreadPool::BaseThread::performAndReturn(boost::unique_lock<boost::try_mutex>& lock)
{
    try
    {
        execution_thread = new boost::thread(boost::bind(&BaseThread::performInParallel, this));

        execution_thread->join();

        execution_thread = nullptr;

        task_performed.notify_one();

        result_accuired.wait(lock);
    }
    catch (boost::thread_interrupted&)
    {
        execution_thread = nullptr;
        task = nullptr;
    }
}

void ThreadPool::BaseThread::interrupt()
{
    auto to_interrupt = execution_thread != nullptr ? execution_thread : watcher_thread;
    
    to_interrupt->interrupt();
}

void ThreadPool::HotThread::performTasks()
{
    try
    {
        boost::unique_lock<boost::try_mutex> lock(mutex);

        while (true)
        {
            task_recieved.wait(lock);

            performAndReturn(lock);
        }
    }
    catch (boost::thread_interrupted&)
    {
    }
}

void ThreadPool::HotThread::run()
{
    watcher_thread = new boost::thread(boost::bind(&HotThread::performTasks, this));
}

void ThreadPool::FreeThread::performTasks()
{
    try
    {
        boost::unique_lock<boost::try_mutex> lock(mutex);

        do
        {
            if (task != nullptr) // when previous task has been killed and this function restarted
            {
                performAndReturn(lock);
            }
        } while (task_recieved.wait_for(lock, boost::chrono::seconds(timeout)) != boost::cv_status::timeout);

        thread_death.notify_all();
    }
    catch (boost::thread_interrupted&)
    {
    }
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

void ThreadPool::FreeThread::run()
{
    watcher_thread = new boost::thread(boost::bind(&FreeThread::performTasks, this));
}

ThreadPool::ThreadPool(int _count, int _timeout)
    :
    hotThreads(_count),
    count(_count),
    timeout(_timeout),
    taskCounter(0)
{
    for (auto& thread : hotThreads)
    {
        thread.run();
    }
}

ThreadPool::~ThreadPool()
{
    for (auto& i : hotThreads)
    {
        i.interrupt();
    }

    for (auto& i : freeThreads)
    {
        i.interrupt();
    }

    for (auto& i : watchersForResult)
    {
        i.second->interrupt();
    }

    for (auto& i : watchersForDeaths)
    {
        i.second->interrupt();
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

    for (auto i = freeThreads.begin(); i != freeThreads.end(); ++i)
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

    auto& new_thread = freeThreads.front();

    assignTask_unsafe(task, new_thread);

    watchersForDeaths[new_thread.last_task_id] = new boost::thread(boost::bind(&ThreadPool::waitForFreeThreadDeath, this, _1), boost::ref(new_thread));

    new_thread.run();

#ifdef TESTING
    string msg = boost::lexical_cast<string>(new_thread.last_task_id) + " n\n";

    output << msg;
#endif
}

void ThreadPool::killTask(unsigned id)
{
    auto thread = workingThreads[id];

    thread->interrupt();
    watchersForResult[id]->interrupt();

    thread->run();

#ifdef TESTING

    string msg = boost::lexical_cast<string>(id)+" k\n";

    output << msg;

#endif

    boost::mutex::scoped_lock lock(listSync);

    workingThreads.erase(id);
    watchersForResult.erase(id);
}

bool ThreadPool::tryAssignTask(Callable* task, ThreadPool::BaseThread& thread)
{
    if (thread.mutex.try_lock())
    {        
        boost::mutex::scoped_lock lock(listSync);
 
        assignTask_unsafe(task, thread);

        thread.mutex.unlock();

        return true;
    }

    return false;
}

void ThreadPool::assignTask_unsafe(Callable* task, ThreadPool::BaseThread& thread)
{
    thread.task = task;
    thread.last_task_id = ++taskCounter;
    workingThreads[thread.last_task_id] = &thread;
    watchersForResult[thread.last_task_id] = new boost::thread(boost::bind(&ThreadPool::waitForResult, this, _1), boost::ref(thread));
}

void ThreadPool::waitForResult(ThreadPool::BaseThread& thread)
{
    try
    {
        boost::unique_lock<boost::try_mutex> lock(thread.mutex);

        thread.task_recieved.notify_one();

        thread.task_performed.wait(lock);

        string msg = boost::lexical_cast<string>(thread.last_task_id) + " ";

#ifdef TESTING

        msg += "r\n";

        output << msg;

#else
        msg += boost::lexical_cast<string>(thread.last_result) + "\n";

        cout << msg;

#endif

        boost::mutex::scoped_lock listLock(listSync);

        workingThreads.erase(thread.last_task_id);
        watchersForResult.erase(thread.last_task_id);

        thread.result_accuired.notify_one();
    }
    catch (boost::thread_interrupted&)
    {
    }
}

void ThreadPool::waitForFreeThreadDeath(ThreadPool::FreeThread& thread)
{
    try
    {
        boost::unique_lock<boost::try_mutex> lock(thread.mutex);

        thread.thread_death.wait(lock);

        boost::mutex::scoped_lock listLock(listSync);

        for (auto i = freeThreads.begin(); i != freeThreads.end(); ++i)
        {
            if (thread.last_task_id == i->last_task_id)
            {
                freeThreads.erase(i);

                break;
            }
        }

        watchersForDeaths.erase(thread.last_task_id);

#ifdef TESTING
        string msg = boost::lexical_cast<string>(thread.last_task_id) + " t\n";

        output << msg;
#endif
    }
    catch (boost::thread_interrupted&)
    {
    }
}
