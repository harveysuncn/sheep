#pragma once

#include <coroutine>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <iostream>
#include <cassert>

#include "concepts.hpp"

namespace sheep {

namespace detail
{

struct sync_wait_event
{
    explicit sync_wait_event(bool done = false)
        : done_(done)
    {}
    ~sync_wait_event() = default;
    sync_wait_event(const sync_wait_event&) = delete;
    sync_wait_event(sync_wait_event&&) = delete;
    sync_wait_event& operator=(const sync_wait_event&) = delete;
    sync_wait_event& operator=(sync_wait_event&&) = delete;

    void set() noexcept {
        std::lock_guard<std::mutex> g{mutex_};
        done_ = true;
        cv_.notify_all();
    }
    void reset() noexcept {
        std::lock_guard<std::mutex> g{mutex_};
        done_ = false;
    }
    void wait() noexcept {
        std::unique_lock<std::mutex> lk{mutex_};
        cv_.wait(lk, [this] { return done_; });
    }
    bool done() noexcept {
        std::lock_guard<std::mutex> g{mutex_};
        return done_;
    }

private:
    bool done_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

struct sync_wait_promise_base
{
    sync_wait_promise_base() noexcept = default;
    ~sync_wait_promise_base() = default;

    std::suspend_always initial_suspend() noexcept { return {}; }
    void unhandled_exception() noexcept { exception_ptr_ = std::current_exception(); }

protected:
    std::exception_ptr exception_ptr_;
    sync_wait_event* ev_{nullptr};
};

template <typename T>
struct sync_wait_promise final : public sync_wait_promise_base
{
    using coroutine_handle = std::coroutine_handle<sync_wait_promise<T>>;

    sync_wait_promise() noexcept = default;

    auto start(sync_wait_event& ev) {
        ev_ = std::addressof(ev);
        coroutine_handle::from_promise(*this).resume();
    }

    void set_event(sync_wait_event* p) {
        ev_ = p;
    }

    auto get_return_object() noexcept { return coroutine_handle::from_promise(*this); }

    auto yield_value(T&& value) noexcept {
        value_ = std::addressof(value);
        return final_suspend();
    }

    auto final_suspend() noexcept {
        struct final_awaitable
        {
            bool await_ready() const noexcept { return false; }

            void await_suspend(coroutine_handle waiter) const noexcept {
                waiter.promise().ev_->set();
            } 

            void await_resume() noexcept {};

        };

        return final_awaitable{};
    }

    void return_void() noexcept {}

    auto result() -> T&& {
        if (exception_ptr_ != nullptr) std::rethrow_exception(exception_ptr_);
        assert(value_ != nullptr);
        return static_cast<T&&>(*value_);
    }

private:
    std::remove_reference_t<T>* value_{nullptr};
};


template <>
struct sync_wait_promise<void> : public sync_wait_promise_base
{
    using coroutine_handle = std::coroutine_handle<sync_wait_promise<void>>;
    sync_wait_promise() = default;

    auto start(sync_wait_event& ev) {
        ev_ = std::addressof(ev);
        coroutine_handle::from_promise(*this).resume();
    }

    auto get_return_object() noexcept { return coroutine_handle::from_promise(*this); }
    
    void return_void() noexcept {}
    
    auto final_suspend() noexcept {
        struct final_awaitable
        {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(coroutine_handle waiter) const noexcept {
                waiter.promise().ev_->set();
                return std::noop_coroutine();
            }
            void await_resume() noexcept {};
        };
        return final_awaitable{};
    }

    void result() {
        if (exception_ptr_ != nullptr) std::rethrow_exception(exception_ptr_);
    }
};

template <typename T>
struct sync_wait_task
{
    using promise_type = sync_wait_promise<T>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

    sync_wait_task(coroutine_handle coro) noexcept
        : coro_(coro)
    {}

    sync_wait_task(const sync_wait_task&) = delete;
    sync_wait_task& operator=(const sync_wait_task&) = delete;
    sync_wait_task(sync_wait_task&& other) noexcept
        : coro_(std::exchange(other.coro_, coroutine_handle{}))
    {}
    sync_wait_task& operator=(sync_wait_task&& other)
    {
        if (std::addressof(other) == this) return *this;
        coro_ = std::exchange(other.coro_, coroutine_handle{});
        return *this;
    }

    ~sync_wait_task()
    {
        if (coro_) coro_.destroy();
    }

    void start(sync_wait_event& event) noexcept 
    {
        auto p = coro_.promise();
        coro_.promise().start(event);
    }

    void set_event(sync_wait_event& event) noexcept
    {
        coro_.promise().set_event(&event);
    }

    std::coroutine_handle<> get_handle() noexcept
    {
        return coro_;
    }

    decltype(auto) get_result()
    {
        if constexpr (std::is_void_v<T>) coro_.promise().result();
        else return coro_.promise().result();
    }

private:
    coroutine_handle coro_;
};


template <concepts::awaitable awaitable_type, typename T = typename concepts::awaitable_traits<awaitable_type>::awaiter_return_type>
static sync_wait_task<T> make_sync_wait_task(awaitable_type&& a) 
{
    if constexpr (std::is_void_v<T>) {
        co_await std::forward<awaitable_type>(a);
        co_return;
    } else {
        co_yield co_await std::forward<awaitable_type>(a);
    }
}


} // namespace detail

template <concepts::awaitable awaitable_type>
auto sync_wait(awaitable_type&& a)
{
    detail::sync_wait_event ev{};
    auto task = detail::make_sync_wait_task(std::forward<awaitable_type>(a));
    task.start(ev);
    ev.wait();
    return task.get_result();
}


} // namespace sheep