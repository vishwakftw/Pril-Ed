// Function definitions for the function prototypes in ThreadPool.hpp
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include "ThreadPool.hpp"
namespace para {

    // Only 4 threads will be used. Reason behind this is that most of the machines in which this is tested will be a quad-core.
    constexpr const auto num_threads=4;

    // Condition variable to signal
    std::condition_variable signaller;

    // Standard mutex
    std::mutex signal_lock;

    // Maintain a queue to accomodate waiting tasks
    std::queue<task> waiting;

    // Atomic variable to see if there are any functions to be paused
    std::atomic<bool> stop_flag(false);

    // Condition variable to terminate
    std::condition_variable termin;

    // Another standard mutex for termination purposes
    std::mutex termin_lock;

    // Atomic variable to keep track of number of threads paused
    std::atomic<unsigned> stop_count(0);

    // Function to start the threads in the thread pool
    void start_threads() {
        auto thread_job=[](){
            while(true) {
                std::unique_lock<std::mutex> locker(signal_lock);
                if (waiting.empty()&&(!stop_flag.load())) { // If the queue is empty and there are no functions stopped, then we wait for a function
                    signaller.wait(locker,[](){return stop_flag.load()||(!waiting.empty());});
                }
                if (stop_flag.load()) {     // if termination is required
                    stop_count.fetch_add(1);
                    termin.notify_one();
                    break;
                }
                // We then pop out the first function in the queue, and displace the queue
                auto func=std::move(waiting.front());
                waiting.pop();
                func();
            }
        };

        // Create 4 threads to perform the above job asynchronously
        for (unsigned i=0;i!=num_threads;++i) {
            auto t=std::thread(thread_job);
            t.detach();
        }
    }
    void schedule(task&& t) {       // Function to schedule threads for proper operation

        // pushing the function to queue is a critical section problem
        std::unique_lock<std::mutex> locker(signal_lock);

        // push the function to the queue and unlock the lock
        waiting.push(t);
        locker.unlock();

        // Unblock any one waiting thread
        signaller.notify_one();
    }
    void stop_threads() {   // Function to pause a function running on a particular thread
        stop_flag.store(true);      // Set the stop_flag to be true
        signaller.notify_all();     // Unblock all waiting threads
        if (stop_count.load()!=num_threads) {   // If number of threads stopped is less than the max permitted i.e., 4, then we block
            std::unique_lock<std::mutex> locker(termin_lock);
            termin.wait(locker,[](){return stop_count.load()==num_threads;});
        }
    }
}
