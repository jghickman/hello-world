
#include "stdafx.h"
#include "isptech/concurrency/task.hpp"
#include "boost/variant.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <windows.h>


using namespace Isptech::Concurrency;
using namespace std;


#if 0
Task
chain(Send_channel<int> left, Receive_channel<int> right)
{
    const int n = co_await right.receive();
    co_await left.send(n + 1);
}


void
main(int argc, char* argv[])
{
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " count\n";
        exit(1);
    }

    int         result  = 0;
    const int   n       = max(atoi(argv[1]), 0);

    if (n > 0) {
        const Channel<int>  leftmost    = make_channel<int>();
        Channel<int>        right       = leftmost;

        for (int i = 0; i != n; ++i) {
            const Channel<int> left = right;

            right = make_channel<int>(50);
            start(chain, left, right);
        }

        right.sync_send(0);
        result = leftmost.sync_receive();
    }

    cout << "total = " << result << endl;
    char c;
    cin >> c;
}
#elif 0

using Time = double;


template<class T>
struct Future {
    T get();
};


template<>
struct Future<void> {
    void get();
};


class Player {
public:
    Future<void> process();
};



typedef vector<Player>          Player_vector;
typedef vector<Future<void>>    Future_void_vector;


void
main(int argc, char* argv[])
{
    Player_vector       players;
    Future_void_vector  futures;

    for (auto& x : players)
        futures.push_back(x.process());

    sync_wait_for_wall(futures);

    int c;
    cin >> c;
}

// part 2

Task
wait_for_a_or_b(Receive_channel<int> a, Receive_channel<int> b, Send_channel<int> r)
{
    int aval;
    int bval;

    Channel_operation ops[] = {
          a.make_receive(&aval)
        , b.make_receive(&bval)
    };
    const Channel_size pos = co_await select(ops);

    switch(pos) {
    case 0: co_await r.send(aval); break;
    case 1: co_await r.send(bval); break;
    }

    Future<int> future;

//    int x = co_await future.get();
}


void
main(int argc, char* argv[])
{
    Channel<int> a = make_channel<int>(1);
    Channel<int> b = make_channel<int>(1);
    Channel<int> r = make_channel<int>(1);

    start(wait_for_a_or_b, a, b, r);
    Sleep(5000);
    a.sync_send(1);

    cout << "r = " << r.sync_receive() << endl;
    char c;
    cin >> c;
}

#else


int
add_one(int n)
{
    Sleep(5000);
    return n + 1;
}


Task
add_one_task(int n, Send_channel<int> results)
{
    Future<int> f = async(add_one, n);
    int r;

    try {
        r = co_await f.get();
    } catch (...) {
        r = -1;
    }

    co_await results.send(r);
}




void
main(int argc, char* argv[])
{
//    cout << "r = " << r.sync_receive() << endl;

    Channel<int> result = make_channel<int>(1);

    start(add_one_task, 0, result);
    cout << "result = " << result.sync_receive() << endl;
    char c;
    cin >> c;
}

#endif