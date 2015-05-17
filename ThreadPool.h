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
    ~ThreadPool();

    void addTask(Callable* task);
    void killTask(unsigned id);

private:

    class BaseThread
    {
    public:
        BaseThread();
        BaseThread(const BaseThread& thread);

        friend class ThreadPool;
    protected:
        int last_result;
        unsigned last_task_id;

        boost::try_mutex mutex;

        boost::condition_variable task_recieved;
        boost::condition_variable task_performed;
        boost::condition_variable result_accuired;
        
        boost::thread* watcher_thread;
        boost::thread* execution_thread;

        Callable* task;

        void performAndReturn(boost::unique_lock<boost::try_mutex>& lock);
        void performInParallel();
        virtual void interrupt();

        virtual void performTasks() = 0;
        virtual void run() = 0;
    };

    class HotThread : public BaseThread
    {
    public:
        friend class ThreadPool;

        void performTasks();
        virtual void run();
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

        virtual void interrupt();
        virtual void run();
    };

    int count;
    int timeout;
    unsigned taskCounter;

    vector<HotThread> hotThreads;
    list<FreeThread> freeThreads;
    map< unsigned, BaseThread* > workingThreads;
    map< unsigned, boost::thread* > watchersForResult;
    map< unsigned, boost::thread* > watchersForDeaths;

    boost::mutex listSync;

    bool tryAssignTask(Callable* task, BaseThread& thread);
    void assignTask_unsafe(Callable* task, BaseThread& thread);
    void waitForResult(BaseThread& thread);
    void waitForFreeThreadDeath(FreeThread& thread);

#ifdef TESTING

public:

    ofstream output;

    void setOutput(string filename) { output = ofstream(filename); }

#endif

};