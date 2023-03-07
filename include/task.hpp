#pragma once 

#include <coroutine>
#include <exception>
#include <type_traits>
#include <utility>

namespace sheep
{

template <typename T>
struct task;
namespace detail
{





struct task_promise_base
{
    task_promise_base() noexcept = default;
    ~task_promise_base() noexcept = default;

struct final_awaitable
{
    bool await_ready() const noexcept { return false; }    

    template <std::derived_from<task_promise_base> promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<promise> coro) const noexcept {
        if (coro.promise().continue_ != nullptr)
            return coro.promise().continue_;
        else
            return std::noop_coroutine();
    }

    void await_resume() const noexcept {}

};

    std::suspend_always initial_suspend() noexcept { return {}; }
    final_awaitable final_suspend() noexcept { return {}; }
    void unhandled_exception() { exception_ = std::current_exception(); }
    void set_continue(std::coroutine_handle<> coro) noexcept { continue_ = coro; }

protected:
    friend struct final_awaitable;
    std::coroutine_handle<> continue_{nullptr};
    std::exception_ptr exception_;
};


template <typename T>
struct task_promise final : public task_promise_base
{
    task<T> get_return_object() noexcept;
    
    void return_value(const T& value) { value_ = value; }
    template <typename U>
    void return_value(U&& value) { value_ = std::move(value); }

    T& result() {
        if (exception_ != nullptr) [[unlikely]]
            std::rethrow_exception(exception_);
        return value_;
    }
private:
    T value_;
};

template <>
struct task_promise<void> : public task_promise_base
{
    task<void> get_return_object() noexcept;

    void return_void() {}
    void result() { if (exception_) [[unlikely]] std::rethrow_exception(exception_);}
};

} // namespace detail



template <typename T = void>
struct [[nodiscard]] task
{
    using promise_type = detail::task_promise<T>;
    using coroutine_handle = std::coroutine_handle<promise_type>;

    struct task_awaitable
    {
        coroutine_handle coro_;
        task_awaitable(coroutine_handle coro) noexcept 
            : coro_(coro)
        {}

        bool await_ready() const noexcept { return !coro_ || coro_.done(); }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
            coro_.promise().set_continue(caller);
            return coro_;
        }

    };

    task() noexcept : coro_(nullptr) {}

    task(std::coroutine_handle<promise_type> coro) noexcept
        : coro_(coro)
    {}

    task(task&& t) noexcept
        : coro_(std::exchange(t.coro_, nullptr))
    {}

    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task& operator==(task&& other) noexcept {
        if (std::addressof(other) != this){
            if (coro_ != nullptr) {
                coro_.destroy();
            }
            coro_ = std::exchange(other.coro_, nullptr);
        }
        return *this;
    }    

    ~task() {
        if (coro_) coro_.destroy();
    }

    bool done() const noexcept {
        return !coro_ || coro_.done();
    }

    coroutine_handle detach() noexcept {
        auto ret = coro_;
        coro_ = nullptr;
        return ret;
    }

    auto operator co_await() {
        struct awaiter: task_awaitable
        {
            decltype(auto) await_resume() {
                if constexpr (std::is_void_v<T>) {
                    this->coro_.promise().result();
                    return;
                } else {
                    return this->coro_.promise().result();
                }
            }
        };

        return awaiter{coro_};
    }

    auto get_result()
    {
        if constexpr (std::is_void_v<T>) coro_.promise().result();
        else return coro_.promise().result();
    }

    void resume()
    {
        coro_.resume();
    }

private:
    coroutine_handle coro_;
};


namespace detail
{

template <typename T>
inline task<T> task_promise<T>::get_return_object() noexcept {
    return {std::coroutine_handle<task_promise<T>>::from_promise(*this)};
}


inline task<void> task_promise<void>::get_return_object() noexcept {
    return task<void>(std::coroutine_handle<task_promise<void>>::from_promise(*this));
}

} // namespace detail

} // namespace sheep