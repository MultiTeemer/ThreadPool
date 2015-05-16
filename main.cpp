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

typedef enum {
    ADD,
    KILL,
    SLEEP,
} ActionsT;


typedef void(*test)();

struct test_case
{
    ActionsT action;

    union {
        struct {
            unsigned task_duration;
            unsigned expected;
        };
        unsigned task_id;
        unsigned sleep_duration;
    };
};

unsigned urand()
{
    return static_cast<unsigned>(rand());
}

template <typename Type>
bool contains(const vector<Type>& coll, const Type& val)
{
    return find(coll.cbegin(), coll.cend(), val) != coll.cend();
}

void base_tests()
{
    unsigned seed = time(NULL);

    test_case cases[] = {
        { ADD, { 1u, urand() } },
        { SLEEP, 1500u }
    };

    auto outfile_name = "base_tests.txt";

    srand(seed);
    
    ThreadPool* pool = new ThreadPool(1, 2);

    pool->output = ofstream(outfile_name);

    for (auto i = cases; i < cases + 2; ++i)
    {
        switch (i->action)
        {
        case ADD:
            pool->addTask(new Timer(i->task_duration));
            break;
        case KILL:
            pool->killTask(i->task_id);
            break;
        case SLEEP:
            boost::this_thread::sleep_for(boost::chrono::milliseconds(i->sleep_duration));
            break;
        default:
            throw "Wrong action";
        }
    }

    delete pool;

    ifstream in(outfile_name);

    vector<string> lines;

    while (!in.eof())
    {
        string line;

        getline(in, line, '\n');

        lines.push_back(line);
    }

    assert(contains(lines, string("1 h")));
    assert(contains(lines, string("1 ") + boost::lexical_cast<string>(cases[0].expected)));
}

int main(int argc, char** argv)
{
    //int N = boost::lexical_cast<int>(argv[1]);
    //int T = boost::lexical_cast<int>(argv[2]);

    //ThreadPool pool(N, T);

    //while (1)
    //{
    //    string cmd;

    //    getline(cin, cmd);

    //    vector<string> parts;

    //    boost::split(parts, cmd, boost::is_any_of(" "));

    //    string command = parts[0];

    //    if (parts.size() > 1)
    //    {
    //        int param = boost::lexical_cast<int>(parts[1]);

    //        if (command == "add")
    //        {
    //            pool.addTask(new Timer(param));
    //        }
    //        else if (command == "kill")
    //        {

    //        }
    //    }
    //    else
    //    {
    //        if (command == "exit")
    //        {
    //            break;
    //        }
    //    }
    //}

    vector<test> tests = { base_tests };

    for_each(tests.begin(), tests.end(), [](test f) { f(); });

    return 0;
}
