#pragma once 

#include <cstdint>
#include <set>
#include <thread>
#include <vector>
#include <condition_variable>

#include "task.hpp"
#include "types.hpp"
#include "mpmc_queue.hpp"
#include "io_service_pool.hpp"

namespace sheep {



class thread_pool
{
public:
    thread_pool(int n_threads, io_service_pool& ios_pool)
        : session_queue_(1024)
        , io_services_(ios_pool)
    {
        assert(n_threads == ios_pool.size());
        work_threads_.reserve(n_threads);
        thread_local_coros_.resize(n_threads);
    }

    ~thread_pool() noexcept {
        stop_work_thread();
    }

    void submit(session_wrapper session) {
        session_queue_.emplace(std::move(session));
        avaliable_cv_.notify_one();
    }

    void start() noexcept {
        for (uint16_t i=0; i<thread_local_coros_.size(); ++i) {
            work_threads_.emplace_back(
                [this, i](auto stop_token) {
                    this_thread_ = thread_meta{i};
                    start_work_thread(stop_token);
                }
            );
        }
    }

private:
    std::set<std::coroutine_handle<>>& get_coro_list(thread_meta thread) noexcept {
        assert(thread.thread_id < thread_local_coros_.size());
        return thread_local_coros_[thread.thread_id];
    }

    std::set<std::coroutine_handle<>>& get_coro_list() noexcept {
        return get_coro_list(this_thread());
    }

    thread_meta this_thread() noexcept { return this_thread_; }

    void resume_coroutine() {
        auto& coro_list = get_coro_list();
        auto& ios = io_services_.get_io_service(this_thread());
        session_wrapper session;
        while (session_queue_.try_pop(session))
        {
            if (session.coro == nullptr) [[unlikely]] continue;
            session.conn->set_io_service(&ios);
            session.coro.resume();
            if (!session.coro.done())
                coro_list.insert(session.coro);
        }
    }

    void start_work_thread(std::stop_token st) noexcept {
        auto& ios = io_services_.get_io_service(this_thread());
        auto& coro_list = get_coro_list(this_thread());

        while (!st.stop_requested())
        {
            {
                std::unique_lock<std::mutex> lk{idle_mutex_};
                avaliable_cv_.wait(lk, [this](){
                    return !session_queue_.empty() || request_stop_;
                });
            }

            resume_coroutine();

            while (!coro_list.empty())
            {
                // resume coroutine which io is ready
                ios.wait_io_and_resume_coroutine();
                for (auto it = coro_list.begin(); it != coro_list.end(); ) {
                    if (it->done()) {
                        it->destroy();
                        it = coro_list.erase(it);
                    } else {
                        ++it;
                    }
                }
                // resume coroutine submitted by user
                resume_coroutine();
            }
        }
    }

    void stop_work_thread() noexcept {
        request_stop_ = true;
        for (auto& thr: work_threads_) 
            thr.request_stop();

        avaliable_cv_.notify_all();
        for (auto& thr: work_threads_) {
            if (thr.joinable())
                thr.join();
        }
    }

    inline static thread_local thread_meta this_thread_;
    io_service_pool& io_services_;
    std::vector<std::jthread> work_threads_;
    MPMCQueue<session_wrapper> session_queue_;
    std::vector<std::set<std::coroutine_handle<>>> thread_local_coros_;

    bool request_stop_{false};
    std::condition_variable avaliable_cv_;
    std::mutex idle_mutex_;
};



}