#pragma once

#define TESTING

#include <vector>
#include <list>
#include <fstream>
#include <boost\thread.hpp>

using namespace std;

class Callable
{
public:
    Callable() {}
    virtual int operator() () { return -1; }
};

class ThreadPool
{
public:
    
    ThreadPool(int _count, int _timeout);

    void addTask(Callable* task);
    void killTask(unsigned id);

private:

    class BaseThread
    {
    public:
        BaseThread();
        BaseThread(const BaseThread& thread);

        virtual void performTasks() = 0;

        friend class ThreadPool;
    protected:
        int last_result;
        unsigned last_task_id;

        boost::try_mutex mutex;
        boost::condition_variable task_recieved;
        boost::condition_variable task_performed;
        boost::condition_variable result_accuired;
        
        Callable* task;

        void performAndReturn(boost::unique_lock<boost::try_mutex>& lock);
    };

    class HotThread : public BaseThread
    {
    public:
        friend class ThreadPool;

        void performTasks();
    };

    class FreeThread : public BaseThread
    {
    public:
        friend class ThreadPool;

        FreeThread(unsigned timeout);
        FreeThread(const FreeThread& other);

        void performTasks();
    private:
        unsigned timeout;
        
        boost::condition_variable thread_death;
    };

    int count;
    int timeout;
    unsigned taskCounter;

    vector<HotThread> hotThreads;
    list<FreeThread> freeThreads;
    map< unsigned, BaseThread* > workingThreads;
    list<boost::thread*> freeThreadsExecutions;

    boost::mutex listSync;

    bool tryAssignTask(Callable* task, BaseThread& thread);
    void waitForResult(BaseThread& thread);
    void waitForFreeThreadDeath(FreeThread& thread);

#ifdef TESTING

    ofstream output;

    friend void base_tests();

#endif

};