#pragma once

#include <concepts>
#include <coroutine>
#include <utility>


namespace sheep
{
namespace concepts
{

template <typename type>
concept awaiter = requires(type t, std::coroutine_handle<> c)
{
    { t.await_ready() } -> std::same_as<bool>;
    requires std::same_as<decltype(t.await_suspend(c)), void> ||
        std::same_as<decltype(t.await_suspend(c)), bool> ||
        std::same_as<decltype(t.await_suspend(c)), std::coroutine_handle<>>;
    //{ t.await_resume() };
};

template <typename type>
concept awaitable = requires(type t)
{
    { t.operator co_await() } -> awaiter;
};

template <awaitable awaitable_t>
static auto get_awaiter(awaitable_t&& a)
{
    return std::forward<awaitable_t>(a).operator co_await();
}

template <typename awaitable_t, typename = void>
struct awaitable_traits{};

template <typename awaitable_t>
struct awaitable_traits<awaitable_t>
{
    using awaiter_type = decltype(get_awaiter(std::declval<awaitable_t>()));
    using awaiter_return_type = decltype(std::declval<awaiter_type>().await_resume());
};

}

}