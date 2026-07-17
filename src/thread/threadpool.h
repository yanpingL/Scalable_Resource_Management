#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <list>
#include <exception>
#include <iostream>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

#include "utils/logger.h"

// Fixed-size worker pool for connection tasks.
template<typename T>
class threadpool {
public:
    // Creates worker threads and prepares the bounded task queue.
    threadpool(int thread_number = 8, int max_requests = 10000);

    // Releases the worker thread handle array and marks the pool as stopped.
    ~threadpool();

    // Adds a connection task to the queue and wakes one waiting worker.
    bool append(T *request);

private:
    // Worker loop: waits for queued tasks and processes them.
    void run();

private:
    int m_thread_number;
    std::vector<std::thread> m_threads;
    int m_max_requests;

    std::list<T*> m_workqueue;

    std::mutex m_queuelocker;
    std::counting_semaphore<> m_queuestat;

    std::atomic_bool m_stop;
};


template<typename T>
// Starts the fixed set of worker threads.
threadpool<T>::threadpool(int thread_number, int max_requests) : 
m_thread_number(thread_number), 
m_max_requests(max_requests),
m_queuestat(0),
m_stop(false) {

        if ((thread_number <= 0) || (max_requests <= 0)){
            throw std::exception();
        }
        m_threads.reserve(m_thread_number);

        for (int i = 0; i < thread_number; ++i){
            Logger::get_instance()->log(INFO, "Create thread " + std::to_string(i));
            m_threads.emplace_back(&threadpool<T>::run, this);
        }
}


template<typename T>
// Marks the pool stopped.
threadpool<T>::~threadpool(){
    m_stop = true;
    m_queuestat.release(m_thread_number);
    for (std::thread& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

template<typename T>
// Pushes a task into the queue if capacity allows.
bool threadpool<T>::append(T *request){
    {
        std::lock_guard<std::mutex> lock(m_queuelocker);
        if(m_workqueue.size() >= static_cast<std::size_t>(m_max_requests)){
            Logger::get_instance()->log(ERROR, "thread pool queue is full; request rejected");
            return false;
        }
        m_workqueue.push_back(request);
    }

    m_queuestat.release();
    return true;
}


template<typename T>
// Repeatedly pulls tasks from the queue and invokes request->process().
void threadpool<T>::run(){
    while(!m_stop) {
        m_queuestat.acquire();
        if (m_stop) {
            break;
        }

        T* request = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_queuelocker);
            if(m_workqueue.empty()) {
                continue;
            }

            request = m_workqueue.front();
            m_workqueue.pop_front();
        }

        if(!request){
            continue;
        }
        request->process();
    }
}

#endif
