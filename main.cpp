#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <boost\lexical_cast.hpp>
#include <boost\thread.hpp>
#include <boost\algorithm\string.hpp>
#include <boost\chrono\chrono.hpp>

#include "ThreadPool.h"

using namespace std;

class Timer : public Callable
{
public:

    Timer(unsigned d) : duration(d) {}

    virtual int operator() ()
    {
        boost::chrono::seconds sleep_time(duration);

        boost::this_thread::sleep_for(sleep_time);
        
        return rand();
    }

private:
    unsigned duration;
};

#ifdef TESTING

typedef void(*test)();

struct TestersAction
{
    typedef enum {
        ADD,
        KILL,
        SLEEP,
        INITIALIZE_POOL,
    } ActionsT;

    ActionsT action;

    union {
        unsigned arg; // add - task duration (in seconds), kill - task id, sleep - sleep duration (in milliseconds)
        
        struct {
            unsigned N;
            unsigned T;
            
            string filename;
        };
    };

    TestersAction(ActionsT action, unsigned arg)
        :
        action(action),
        arg(arg)
    {};

    TestersAction(unsigned count, unsigned timeout, string filename)
        :
        action(INITIALIZE_POOL),
        N(count),
        T(timeout),
        filename(filename + ".txt")
    {}

};

struct ThreadPoolsAction
{
    typedef enum {
        ASSIGN_TO_HOT_THREAD,
        ASSIGN_TO_FREE_THREAD,
        CREATE_FREE_THREAD,
        TERMINATE_FREE_THREAD,
        RETURN_RESULT,
        KILL_TASK,
    } ActionsT;

    ActionsT action;
    
    unsigned task_id;

    ThreadPoolsAction(ActionsT action, unsigned task_id)
        :
        action(action),
        task_id(task_id)
    {}

    ThreadPoolsAction(const string& str)
    {
        vector<string> parts;

        boost::split(parts, str, boost::is_any_of(" "));

        task_id = boost::lexical_cast<unsigned>(parts[0]);

        map< char, ActionsT > mapping = {
            { 'h', ASSIGN_TO_HOT_THREAD },
            { 'f', ASSIGN_TO_FREE_THREAD },
            { 'n', CREATE_FREE_THREAD },
            { 'r', RETURN_RESULT },
            { 't', TERMINATE_FREE_THREAD },
            { 'k', KILL_TASK }
        };

        action = mapping[parts[1][0]];
    }

    bool operator==(const ThreadPoolsAction& rhs)
    {
        return action == rhs.action && task_id == rhs.task_id;
    }

    bool operator!=(const ThreadPoolsAction& rhs)
    {
        return !(*this == rhs);
    }

    operator string() const
    {
        map< ActionsT, string > mapping = {
            { ASSIGN_TO_HOT_THREAD, "assign to hot thread" },
            { ASSIGN_TO_FREE_THREAD, "assign to free thread" },
            { CREATE_FREE_THREAD, "create free thread" },
            { TERMINATE_FREE_THREAD, "terminate free thread" },
            { RETURN_RESULT, "return result" },
            { KILL_TASK, "kill task" },
        };

        return boost::lexical_cast<string>(task_id) + " " + mapping[action] + "\n";
    }
};

template <typename Type>
class Vector : public vector<Type>
{
public:

    Vector<Type>& push_back(const Type& val)
    {
        vector<Type>::push_back(val);

        return *this;
    }

    bool contains(const Type& val)
    {
        return find(cbegin(), cend(), val) != cend();
    }

    bool operator==(const Vector<Type>& rhs)
    {
        bool res = size() == rhs.size();

        for (int i = 0; i < size() && res; ++i)
        {
            res &= (*this)[i] == rhs[i];
        }

        return res;
    }
};

Vector<ThreadPoolsAction> run_test_case(const Vector<TestersAction>& actions)
{
    typedef TestersAction::ActionsT ta;
    typedef ThreadPoolsAction::ActionsT tpa;

    ThreadPool* pool;
    string filename;
    
    for (const auto& action : actions)
    {
        switch (action.action)
        {
        case ta::INITIALIZE_POOL:
            pool = new ThreadPool(action.N, action.T);
            pool->setOutput(action.filename);
            filename = action.filename;

            break;
    
        case ta::ADD:
            pool->addTask(new Timer(action.arg));
            
            break;

        case ta::KILL:
            pool->killTask(action.arg);

            break;

        case ta::SLEEP:
            boost::this_thread::sleep_for(boost::chrono::milliseconds(action.arg));

            break;
        }
    }

    delete pool;

    ifstream in(filename);

    vector<string> lines;

    while (!in.eof())
    {
        string line;

        getline(in, line);

        if (line.length() > 0)
        {
            lines.push_back(line);
        }
    }

    Vector<ThreadPoolsAction> res;

    for (const string& line : lines)
    {
        res.push_back(ThreadPoolsAction(line));
    }

    return res;
}

struct TestCase
{
    Vector<TestersAction> actions;
    Vector<ThreadPoolsAction> expected;
};

void run_tests(const vector<TestCase>& tests, string tests_name)
{
    int i = 0;
    
    for (const TestCase& tc : tests)
    {
        ++i;

        auto runs_results = run_test_case(tc.actions);

        if (runs_results == tc.expected)
        {
            cout << tests_name << "#" << i << " completed\n";
        }
        else
        {
            cout << "Failed " << tests_name << "#" << i << "\n";
            cout << "Expected size: " << tc.expected.size() << endl;
            cout << "Actual actual: " << runs_results.size() << endl;

            int s1 = tc.expected.size();
            int s2 = runs_results.size();

            for (int i = 0; i < s1 || i < s2; ++i)
            {
                if (i < s1)
                {
                    cout << ("Expected: " + (string)(tc.expected[i]) + " ");
                    cout << endl;
                }

                if (i < s2)
                {
                    cout << ("Actual: " + (string)(runs_results[i]) + "\n");
                }

                cout << endl;
            }
        }
    }
}


#define START_TESTCASE_DESCRIPTION { Vector<TestersAction> actions; Vector<ThreadPoolsAction> expected;
#define END_TESTCASE_DESCRIPTION tests.push_back({ actions, expected }); }

void add_tasks_tests()
{
    vector<TestCase> tests;
    
    typedef TestersAction::ActionsT ta;
    typedef ThreadPoolsAction::ActionsT tpa;

    START_TESTCASE_DESCRIPTION
    actions
        .push_back(TestersAction(1, 1, "add_tasks_1"))
        .push_back(TestersAction(ta::ADD, 1))
        .push_back(TestersAction(ta::SLEEP, 2000))
        ;

    expected
        .push_back(ThreadPoolsAction(tpa::ASSIGN_TO_HOT_THREAD, 1))
        .push_back(ThreadPoolsAction(tpa::RETURN_RESULT, 1))
        ;
    END_TESTCASE_DESCRIPTION
        
    START_TESTCASE_DESCRIPTION
    actions
        .push_back(TestersAction(3, 1, "add_tasks_2"))
        .push_back(TestersAction(ta::ADD, 1))
        .push_back(TestersAction(ta::SLEEP, 250))
        .push_back(TestersAction(ta::ADD, 1))
        .push_back(TestersAction(ta::SLEEP, 250))
        .push_back(TestersAction(ta::ADD, 1))
        .push_back(TestersAction(ta::SLEEP, 2000))
    ;

    expected
        .push_back(ThreadPoolsAction(tpa::ASSIGN_TO_HOT_THREAD, 1))
        .push_back(ThreadPoolsAction(tpa::ASSIGN_TO_HOT_THREAD, 2))
        .push_back(ThreadPoolsAction(tpa::ASSIGN_TO_HOT_THREAD, 3))
        .push_back(ThreadPoolsAction(tpa::RETURN_RESULT, 1))
        .push_back(ThreadPoolsAction(tpa::RETURN_RESULT, 2))
        .push_back(ThreadPoolsAction(tpa::RETURN_RESULT, 3))
    ;
    END_TESTCASE_DESCRIPTION

    START_TESTCASE_DESCRIPTION
    actions
        .push_back(TestersAction(0, 2, "add_tasks_3"))
        .push_back(TestersAction(ta::ADD, 1))
        .push_back(TestersAction(ta::SLEEP, 2000))
    ;

    expected
        .push_back(ThreadPoolsAction(tpa::CREATE_FREE_THREAD, 1))
        .push_back(ThreadPoolsAction(tpa::RETURN_RESULT, 1))
        .push_back(ThreadPoolsAction(tpa::TERMINATE_FREE_THREAD, 1))
    ;
    END_TESTCASE_DESCRIPTION

    run_tests(tests, "add tasks");
}

void kill_tasks_tests()
{
    
}

#endif

int main(int argc, char** argv)
{
#ifndef TESTING

    int N = boost::lexical_cast<int>(argv[1]);
    int T = boost::lexical_cast<int>(argv[2]);

    ThreadPool pool(N, T);

    while (1)
    {
        string cmd;

        getline(cin, cmd);

        vector<string> parts;

        boost::split(parts, cmd, boost::is_any_of(" "));

        string command = parts[0];

        if (parts.size() > 1)
        {
            int param = boost::lexical_cast<int>(parts[1]);

            if (command == "add")
            {
                pool.addTask(new Timer(param));
            }
            else if (command == "kill")
            {
                pool.killTask(param);
            }
        }
        else
        {
            if (command == "exit")
            {
                break;
            }
        }
    }

#else

    vector<test> tests = {
        add_tasks_tests,
        kill_tasks_tests
    };

    for_each(tests.begin(), tests.end(), [](test f) { f(); });

    getchar();

#endif

    return 0;
}
