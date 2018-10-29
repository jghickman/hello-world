//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/task.cpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/task.cpp,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


#include "isptech/concurrency/task.hpp"
#include <algorithm>

#pragma warning(disable: 4073)
#pragma init_seg(lib)


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Names/Types
*/
using std::count_if;
using std::find_if;
using std::move;
using std::try_to_lock;


// Names/Types
Scheduler scheduler;


/*
    Task Promise Channel Locks
*/
inline
Task::Promise::Channel_locks::Channel_locks(Channel_operation* begin, Channel_operation* end)
    : first(begin)
    , last(end)
{
    lock(first, last);
}


inline
Task::Promise::Channel_locks::~Channel_locks()
{
    unlock(first, last);
}


/*
    Task Promise Channel Sort
*/
Task::Promise::Channel_sort::Channel_sort(Channel_operation* begin, Channel_operation* end)
    : first(begin)
    , last(end)
{
    save_positions(first, last);
    sort_channels(first, last);
}


inline
Task::Promise::Channel_sort::~Channel_sort()
{
    restore_positions(first, last);
}


inline void
Task::Promise::Channel_sort::dismiss()
{
    first = last;
}


/*
    Task Promise Enqueued Selection

    put() can be called by multiple threads racing to complete a blocking
    channel select operation, so internal synchronization is required.
    But since the task waiting for the selection calls get() only after
    dequeueing the remaining operations, no synchronization is required
    for that.
*/
Channel_size
Task::Promise::Enqueued_selection::get()
{
    const Channel_size pos = *buffer;

    buffer.reset();
    return pos;
}


bool
Task::Promise::Enqueued_selection::put(Channel_size pos)
{
    bool        is_put = false;
    const Lock  lock{mutex, try_to_lock};

    if (lock && !buffer) {
        buffer = pos;
        is_put = true;
    }

    return is_put;
}


/*
    Task Promise
*/
Channel_size
Task::Promise::count_ready(const Channel_operation* first, const Channel_operation* last)
{
    return count_if(first, last, [](auto& co) {
        return co.is_ready();
    });
}


void
Task::Promise::dequeue(Channel_operation* first, Channel_operation* last, Task::Handle task)
{
    for (Channel_operation* cop = first; cop != last; ++cop)
        cop->dequeue(task);
}


void
Task::Promise::enqueue(Channel_operation* first, Channel_operation* last, Task::Handle task)
{
    for (Channel_operation* cop = first; cop != last; ++cop)
        cop->enqueue(task);
}


void
Task::Promise::lock(Channel_operation* first, Channel_operation* last)
{
    Channel_base* prevchanp = nullptr;

    for (Channel_operation* cop = first; cop != last; ++cop) {
        Channel_base* chanp = cop->channel();
        if (chanp && chanp != prevchanp) {
            chanp->lock();
            prevchanp = chanp;
        }
    }
}


Channel_size
Task::Promise::pick_random(Channel_size min, Channel_size max)
{
    using Device        = std::random_device;
    using Engine        = std::default_random_engine;
    using Distribution  = std::uniform_int_distribution<Channel_size>;

    static Device   rand;
    static Engine   engine{rand()};
    Distribution    dist{min, max};

    return dist(engine);
}
    
   
Channel_operation*
Task::Promise::pick_ready(Channel_operation* first, Channel_operation* last, Channel_size nready)
{
    Channel_operation*  readyp  = last;
    Channel_size        n       = pick_random(1, nready);

    for (Channel_operation* cop = first; cop != last; ++cop) {
        if (cop->is_ready() && --n == 0) {
            readyp = cop;
            break;
        }
    }

    return readyp;
}


void
Task::Promise::restore_positions(Channel_operation* first, Channel_operation* last)
{
    using std::sort;

    sort(first, last, [](auto& x, auto& y) {
        return x.position() < y.position();
    });
}


void
Task::Promise::save_positions(Channel_operation* first, Channel_operation* last)
{
    for (Channel_operation* cop = first; cop != last; ++cop)
        cop->position(cop - first);
}


bool
Task::Promise::select(Channel_operation* first, Channel_operation* last, Task::Handle task)
{
    Channel_sort    sortguard{first, last};
    Channel_locks   chanlocks{first, last};

    readyco = select_ready(first, last);
    if (!readyco) {
        enqueue(first, last, task);
        scheduler.suspend(task);

        // keep enqueued operations in lock order until wakeup
        firstenqco = first;
        lastenqco = last;
        sortguard.dismiss();
    }

    return readyco ? true : false;
}


optional<Channel_size>
Task::Promise::select_ready(Channel_operation* first, Channel_operation* last)
{
    optional<Channel_size>  pos;
    const Channel_size      n = count_ready(first, last);

    if (n > 0) {
        Channel_operation* cop = pick_ready(first, last, n);
        cop->execute();
        pos = cop->position();
    }

    return pos;
}

   
Channel_size
Task::Promise::selected()
{
    Channel_size pos;

    if (readyco)
        pos = *readyco;
    else {
        const Handle task = Handle::from_promise(*this);
        dequeue(firstenqco, lastenqco, task);
        restore_positions(firstenqco, lastenqco);
        pos = selenqco.get();
    }

    return pos;
}


inline void
Task::Promise::sort_channels(Channel_operation* first, Channel_operation* last)
{
    using std::sort;

    sort(first, last, [](auto& x, auto& y) {
        return x.channel() < y.channel();
    });
}


void
Task::Promise::unlock(Channel_operation* first, Channel_operation* last)
{
    Channel_base* prevchanp = nullptr;

    for (Channel_operation* cop = first; cop != last; ++cop) {
        Channel_base* chanp = cop->channel();
        if (chanp && chanp != prevchanp) {
            chanp->unlock();
            prevchanp = chanp;
        }
    }
}


/*
    Channel Operation
*/
Channel_operation::Channel_operation()
    : kind{none}
    , chanp{nullptr}
    , rvalp{nullptr}
    , lvalp{nullptr}
{
}


Channel_operation::Channel_operation(Channel_base* channelp, const void* rvaluep)
    : kind{send}
    , chanp{channelp}
    , rvalp{rvaluep}
    , lvalp{nullptr}
{
    assert(channelp != nullptr);
    assert(rvaluep != nullptr);
}


Channel_operation::Channel_operation(Channel_base* channelp, void* lvaluep, Type optype)
    : kind{optype}
    , chanp{channelp}
    , rvalp{nullptr}
    , lvalp{lvaluep}
{
    assert(channelp != nullptr);
    assert(lvaluep != nullptr);
}


inline Channel_base*
Channel_operation::channel()
{
    return chanp;
}


inline const Channel_base*
Channel_operation::channel() const
{
    return chanp;
}


void
Channel_operation::dequeue(Task::Handle task)
{
    if (chanp) {
        switch(kind) {
        case send:
            chanp->dequeue_send(task, pos);
            break;

        case receive:
            chanp->dequeue_receive(task, pos);
            break;
        }
    }
}


void
Channel_operation::enqueue(Task::Handle task)
{
    if (chanp) {
        switch(kind) {
        case send:
            if (rvalp)
                chanp->enqueue_send(task, pos, rvalp);
            else
                chanp->enqueue_send(task, pos, lvalp);
            break;

        case receive:
            chanp->enqueue_receive(task, pos, lvalp);
            break;
        }
    }
}


void
Channel_operation::execute()
{
    if (chanp) {
        switch(kind) {
        case send:
            if (rvalp)
                chanp->send_ready(rvalp);
            else
                chanp->send_ready(lvalp);
            break;

        case receive:
            chanp->receive_ready(lvalp);
            break;
        }
    }
}


bool
Channel_operation::is_ready() const
{
    if (!chanp) return false;

    switch(kind) {
    case send:      return chanp->is_send_ready();
    case receive:   return chanp->is_receive_ready();
    default:        return false;
    }
}


inline void
Channel_operation::position(Channel_size n)
{
    pos = n;
}


inline Channel_size
Channel_operation::position() const
{
    return pos;
}


/*
    Scheduler Task List
*/
inline
Scheduler::Task_list::handle_equal::handle_equal(Task::Handle task)
    : h{task}
{
}


inline bool
Scheduler::Task_list::handle_equal::operator()(const Task& task) const
{
    return task.handle() == h;
}


void
Scheduler::Task_list::insert(Task&& task)
{
    Lock lock{mutex};

    tasks.push_back(move(task));
}


Task
Scheduler::Task_list::release(Task::Handle h)
{
    Task task;
    Lock lock{mutex};
    auto taskp = find_if(tasks.begin(), tasks.end(), handle_equal(h));

    if (taskp != tasks.end()) {
        task = move(*taskp);
        tasks.erase(taskp);
    }

    return task;
}


/*
    Sheduler Task Queue
*/
void
Scheduler::Task_queue::interrupt()
{
    Lock lock{mutex};

    is_interrupt = true;
    ready.notify_one();
}


optional<Task>
Scheduler::Task_queue::pop()
{
    optional<Task>  task;
    Lock            lock{mutex};

    while (tasks.empty() && !is_interrupt)
        ready.wait(lock);

    if (!tasks.empty())
        task = pop_front(&tasks);

    return task;
}


inline Task
Scheduler::Task_queue::pop_front(std::deque<Task>* tasksp)
{
    Task task{move(tasksp->front())};

    tasksp->pop_front();
    return task;
}


void
Scheduler::Task_queue::push(Task&& task)
{
    Lock lock{mutex};

    tasks.push_back(move(task));
    ready.notify_one();
}


optional<Task>
Scheduler::Task_queue::try_pop()
{
    optional<Task>  task;
    Lock            lock{mutex, try_to_lock};

    if (lock && !tasks.empty())
        task = pop_front(&tasks);

    return task;
}


bool
Scheduler::Task_queue::try_push(Task&& task)
{
    bool is_pushed{false};
    Lock lock{mutex, try_to_lock};

    if (lock) {
        tasks.push_back(move(task));
        ready.notify_one();
        is_pushed = true;
    }

    return is_pushed;
}


/*
    Scheduler Task Queue Array
*/
Scheduler::Task_queue_array::Task_queue_array(Size n)
    : qs{n}
{
}


void
Scheduler::Task_queue_array::interrupt()
{
    for (auto& q : qs)
        q.interrupt();
}


optional<Task>
Scheduler::Task_queue_array::pop(Size qpref)
{
    const auto      nqs = qs.size();
    optional<Task>  task;

    // Try to dequeue a task without waiting.
    for (Size i = 0; i < nqs && !task; ++i) {
        auto pos = (qpref + i) % nqs;
        task = qs[pos].try_pop();
    }

    // If that failed, wait on the preferred queue.
    if (!task)
        task = qs[qpref].pop();

    return task;
}


void
Scheduler::Task_queue_array::push(Task&& task)
{
    const auto  nqs     = qs.size();
    const auto  qpref   = nextq++ % nqs;
    bool        done    = false;

    // Try to enqueue the task without waiting.
    for (Size i = 0; i < nqs && !done; ++i) {
        auto pos = (qpref + i) % nqs;
        if (qs[pos].try_push(move(task)))
            done = true;
    }

    // If we failed, wait on the preferred queue.
    if (!done)
        qs[qpref].push(move(task));
}


inline Scheduler::Task_queue_array::Size
Scheduler::Task_queue_array::size() const
{
    return qs.size();
}


/*
    Scheduler
*/
Scheduler::Scheduler(int nthreads)
    : queues{nthreads > 0 ? nthreads : Thread::hardware_concurrency()}
{
    const auto nqs = queues.size();

    threads.reserve(nqs);
    for (unsigned q = 0; q != nqs; ++q)
        threads.emplace_back([&,q]{ run(q); });
}


/*
    TODO:  Could this destructor should be rendered unnecessary because
    by arranging for (a) queues to shutdown implicitly (in their
    destructors) and (b) queues to be joined implicitly (in their
    destructors)?
*/
Scheduler::~Scheduler()
{
    queues.interrupt();
    for (auto& thread : threads)
        thread.join();
}


void
Scheduler::resume(Task::Handle h)
{
    queues.push(suspended.release(h));
}


/*
    TODO: The coroutine mechanism can automatically detect and store
    exceptions in the promise, so is a try-block even necessary?  Need
    to think through how exceptions thrown by tasks should be handled.
*/
void
Scheduler::run(unsigned qpos)
{
    while (optional<Task> taskp = queues.pop(qpos)) {
        try {
            taskp->run();
        } catch (...) {
            queues.interrupt();
        }
    }
}


void
Scheduler::submit(Task&& task)
{
    queues.push(move(task));
}


void
Scheduler::suspend(Task::Handle h)
{
    suspended.insert(Task(h));
}


}   // Concurrency
}   // Isptech
