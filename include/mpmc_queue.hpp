#pragma once

#include <atomic>
#include <cassert>
#include <limits>
#include <memory>
#include <thread>

#ifndef __cpp_aligned_new
#ifdef _WIN32
#include <malloc.h> // _aligned_malloc
#else
#include <stdlib.h> // posix_memalign
#endif
#endif

namespace sheep {


template <auto num>
struct IsPowerOf2 
{
    static_assert(std::is_integral_v<decltype(num)>, "num must be an integral type");
    static constexpr bool value = (num & num-1) == 0;
};

template<typename T, T Cap, std::size_t BitPos=1>
struct RoundUpToNextPowerOf2
{
    static_assert(std::is_integral_v<T>, "T must be an integral type");
    static constexpr T nextCap = Cap | Cap>>1;
    static constexpr std::size_t nextPos = BitPos << 1 ;
    static constexpr T value 
        = RoundUpToNextPowerOf2< T, nextCap, nextPos >::value;
};

template <typename T, T Cap>
struct RoundUpToNextPowerOf2<T, Cap, 1>
{
    static constexpr T nextCap = (Cap-1) | (Cap-1)>>1;
    static constexpr T value
        = IsPowerOf2<Cap>::value ? Cap : 
        RoundUpToNextPowerOf2<T, nextCap, 2>::value;
};

template <typename T, T Cap>
struct RoundUpToNextPowerOf2<T, Cap, sizeof(T)*8>
{
    static constexpr T value = Cap + 1;
};


template <auto num, decltype(num) min, decltype(num) max>
struct Clip 
{
    static constexpr auto value = (
        num <= min ? min : (
            num >= max ? max : num
        )
    );
};

static constexpr std::size_t cacheLineSize = 64;

#if defined(__cpp_aligned_new)
template <typename T> using AlignAllocator = std::allocator<T>;
#else
template <typename T> struct AlignAllocator {
  using value_type = T;

  T *allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      throw std::bad_array_new_length();
    }
#ifdef _WIN32
    auto p = static_cast<T *>(_aligned_malloc(sizeof(T) * n, alignof(T)));
    if (p == nullptr) {
      throw std::bad_alloc();
    }
#else
    T *p;
    if (posix_memalign(reinterpret_cast<void **>(&p), alignof(T),
                       sizeof(T) * n) != 0) {
      throw std::bad_alloc();
    }
#endif
    return p;
  }

  void deallocate(T *p, std::size_t) {
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
  }
};
#endif

template <typename Iterator>
struct IsOutputIterator
{
    constexpr static bool value 
        = std::is_convertible< typename std::iterator_traits<Iterator>::iterator_category,
                               typename std::output_iterator_tag
                             >::value;
};

template <typename Iterator>
struct IsInputIterator
{
    constexpr static bool value 
        = std::is_convertible< typename std::iterator_traits<Iterator>::iterator_category,
                               typename std::input_iterator_tag
                             >::value;
};

template <typename Iterator>
static bool IsInputIterator_v = IsInputIterator<Iterator>::value;

template <typename Iterator>
static bool IsOutputIterator_v = IsOutputIterator<Iterator>::value;

template <typename T>
struct EnableIfMoveConstructible
{
    using type = typename std::enable_if<std::is_move_constructible_v<T>>::type;
};

template <typename T>
using EnableIfMoveConstructible_t = typename EnableIfMoveConstructible<T>::type;

template <typename Iterator>
struct EnableIfOutputIterator
{
    using type = typename std::enable_if<IsOutputIterator_v<Iterator>>::type;
};

template <typename Iterator>
struct EnableIfInputIterator
{
    using type = typename std::enable_if<IsInputIterator_v<Iterator>>::type;
};

template <typename Iterator>
using EnableIfInputIterator_t = typename EnableIfInputIterator<Iterator>::type;

template <typename Iterator>
using EnableIfOutputIterator_t = typename EnableIfOutputIterator<Iterator>::type;

template <typename T, typename... Args>
struct EnableIfConstructible
{
    using type = typename std::enable_if_t<std::is_constructible_v<T, Args...>>;
};

template <typename T, typename... Args>
using EnableIfConstructible_t = typename EnableIfConstructible<T, Args...>::type;

// only use for template argument
// std::is_invocable<Functor, Args...>::value
template <typename Functor, typename... Args>
struct EnableIfInvocable
{
    using type = typename std::enable_if_t<std::is_invocable_v<Functor, Args...>>;
};

template <typename Fn, typename... Args>
using EnableIfInvocable_t = typename EnableIfInvocable<Fn, Args...>::type;


template <typename T>
struct Slot
{
    Slot()
        : turn(0)
    {}

    ~Slot()
    {
        if (turn & 1) {
            destroy();
        }

    }

    template <typename... Args>
    void construct(Args&&... args) {
        new (&data) T(std::forward<Args>(args)...);
    }

    T&& move() {
        return reinterpret_cast<T&&>(data);
    }

    void destroy() {
        reinterpret_cast<T*>(&data)->~T();
    }

    alignas(64) std::atomic<uint64_t> turn;
    typename std::aligned_storage<sizeof(T), alignof(T)>::type data;
};

template <typename T, typename Alloc = AlignAllocator<Slot<T>>>
class MPMCQueue
{

public:
    MPMCQueue(uint64_t capacity, const Alloc& alloc = Alloc())
        : capacity_(capacity), mask_(capacity-1), alloc_(alloc)
        , slots_(nullptr), head_(0), tail_(0)
    {
        assert((capacity & (capacity-1)) == 0);
        slots_ = alloc_.allocate(capacity_);

        // 使用nested aligned_storage 之前，必须手动初始化
        for (size_t i = 0; i < capacity_; ++i) {
            new (&slots_[i]) Slot<T>();
        }
    }

    ~MPMCQueue()
    {
        for (uint64_t i=0; i<capacity_; ++i) {
            slots_[i].~Slot();   
        }
        alloc_.deallocate(slots_, capacity_);
    }

    void push(const T& v) {
        emplace(v);
    }

    template <typename P, typename = EnableIfMoveConstructible<P>>
    void push (P&& v) {
        emplace(std::forward<P>(v));
    }

    bool try_push(const T& v) {
        return try_emplace(v);
    }

    template <typename P, typename = EnableIfMoveConstructible<P>>
    bool try_push(P&& v) {
        return try_emplace(std::forward<P>(v));
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        uint64_t old_head = head_.fetch_add(1);
        auto& slot = get_slot(old_head);
        while (turn(old_head) * 2 != slot.turn.load(std::memory_order_acquire))
            std::this_thread::yield();
        
        slot.construct(std::forward<Args>(args)...);
        auto new_turn = turn(old_head) * 2 + 1;
        slot.turn.store(new_turn, std::memory_order_release);
    }

    template <typename Fn, typename... Args, typename = EnableIfInvocable<Fn, Args...>>
    void emplace_with(Fn& f, Args&&... args) {
        uint64_t old_head = head_.fetch_add(1);
        auto& slot = get_slot(old_head);
        while (turn(old_head) * 2 != slot.turn.load(std::memory_order_acquire))
            std::this_thread::yield();

        f(reinterpret_cast<T*>(&slot.data), std::forward<Args>(args)...);
        auto new_turn = turn(old_head) * 2 + 1;
        slot.turn.store(new_turn, std::memory_order_release);
    }


    // 一次性push n个元素，在队列中的位置是连续的，
    // 在空间不足时会等待，每成功push一个就可以供消费者消费
    template <typename Iterator, typename = EnableIfInputIterator<Iterator>>
    void bulk_push(const Iterator begin, const Iterator end, std::size_t n) {
        uint64_t old_head = head_.fetch_add(n);
        auto it = begin;
        for (uint64_t i=0; i<n; ++i) {
            auto& slot = get_slot(old_head + i);
            while (turn(old_head+i) * 2 != slot.turn.load(std::memory_order_acquire))
                std::this_thread::yield();
            
            slot.construct(std::forward<T>(*it++));
            auto new_turn = turn(old_head+i) * 2 + 1;
            slot.turn.store(new_turn, std::memory_order_release);
        }
    }

    template <typename... Args>
    bool try_emplace(Args&&... args) {
        uint64_t old_head = head_.load(std::memory_order_acquire);
        for (;;) {
            auto& slot = get_slot(old_head);
            if (turn(old_head) * 2 == slot.turn.load(std::memory_order_acquire)) 
            {
                if (head_.compare_exchange_strong(old_head, old_head+1))
                {
                    slot.construct(std::forward<Args>(args)...);
                    auto new_turn = turn(old_head)*2 + 1;
                    slot.turn.store(new_turn, std::memory_order_release);
                    return true;
                }
            } else {
                auto now_head = head_.load(std::memory_order_acquire);
                if (now_head == old_head)
                    // its not my turn to write
                    return false;
            }
        }
    }

    void pop(T& v) {
        uint64_t old_tail = tail_.fetch_add(1);
        auto& slot = get_slot(old_tail);
        while (turn(old_tail) * 2 + 1 != slot.turn.load(std::memory_order_acquire))
            std::this_thread::yield();
        
        v = slot.move();
        slot.destroy();
        auto new_turn = turn(old_tail) * 2 + 2;
        if (turn(old_tail) == turn(std::numeric_limits<uint64_t>::max()))
            new_turn = 0;
        slot.turn.store(new_turn, std::memory_order_release);
    }

    bool try_pop(T& v) {
        uint64_t old_tail = tail_.load(std::memory_order_acquire);
        for (;;) {
            auto& slot = get_slot(old_tail);
            if (turn(old_tail) * 2 + 1 == slot.turn.load(std::memory_order_acquire))
            {
                if (tail_.compare_exchange_strong(old_tail, old_tail+1))
                {
                    v = slot.move();
                    slot.destroy();
                    auto new_turn = turn(old_tail) * 2 + 2;
                    if (turn(old_tail) == turn(std::numeric_limits<uint64_t>::max()))
                        new_turn = 0;
                    slot.turn.store(new_turn, std::memory_order_release);
                    return true;
                }
            } else {
                auto now_tail = tail_.load(std::memory_order_acquire);
                if (now_tail == old_tail)
                    return false;
            }
        }
    }

    // 提供一个输出的迭代器，尽可能多得输出元素至该迭代器上
    template <typename Iterator, typename = EnableIfOutputIterator<Iterator>>
    uint64_t bulk_pop(Iterator out) {
        uint64_t old_tail = tail_.load(std::memory_order_acquire);
        auto pop_cnt = 0;
        for (;;) {
            auto& slot = get_slot(old_tail);
            if (turn(old_tail) * 2 + 1 == slot.turn.load(std::memory_order_acquire))
            {
                if (tail_.compare_exchange_strong(old_tail, old_tail+1))
                {
                    *out = slot.move();
                    slot.destroy();
                    auto new_turn = turn(old_tail) * 2 + 2;
                    if (turn(old_tail) == turn(std::numeric_limits<uint64_t>::max()))
                        new_turn = 0;
                    slot.turn.store(new_turn, std::memory_order_release);
                    ++pop_cnt;
                    ++old_tail;
                    continue;
                }
            } else {
                auto prev_old_tail = old_tail;
                old_tail= tail_.load(std::memory_order_acquire);
                if (prev_old_tail == old_tail)
                    return pop_cnt;
                continue;
            }
        }        
    }

    template <typename Fn, typename... Args, typename = EnableIfInvocable<Fn, Args...>>
    void consume(Fn& f, Args&&... args) {
        uint64_t old_tail = tail_.fetch_add(1);
        auto& slot = get_slot(old_tail);
        while (turn(old_tail) * 2 + 1 != slot.turn.load(std::memory_order_acquire))
            std::this_thread::yield();
        
        f(reinterpret_cast<T*>(&slot.data), std::forward<Args>(args)...);
        slot.destroy();
        auto new_turn = turn(old_tail) * 2 + 2;
        if (turn(old_tail) == turn(std::numeric_limits<uint64_t>::max()))
            new_turn = 0;
        slot.turn.store(new_turn, std::memory_order_release);
    }

    template <typename Fn, typename... Args, typename = EnableIfInvocable<Fn, Args...>>
    uint64_t try_consume_all(Fn& f, Args&&... args) {
        uint64_t old_tail = tail_.load(std::memory_order_acquire);
        uint64_t consume_cnt = 0;
        for (;;) {
            auto& slot = get_slot(old_tail);
            
            if (turn(old_tail) * 2 + 1 == slot.turn.load(std::memory_order_acquire))
            {
                // 1.多个线程在此抢夺cas，最终只会有一个成功，而其他的线程则会进入下一次的turn比较
                if (tail_.compare_exchange_strong(old_tail, old_tail+1))
                {
                    f(reinterpret_cast<T*>(&slot.data), std::forward<Args>(args)...);
                    slot.destroy();
                    auto new_turn = turn(old_tail) * 2 + 2;
                    if (turn(old_tail) == turn(std::numeric_limits<uint64_t>::max()))
                        new_turn = 0;
                    slot.turn.store(new_turn, std::memory_order_release);
                    ++old_tail;
                    ++consume_cnt;
                    continue;
                }
            // 2.不是可pop的turn，说明生产者还没来得及push
            } //else return consume_cnt; // 如果不是我们的turn，就直接退出，会失去本来可以pop的机会
            else {
                auto prev_old_tail = old_tail;
                // 3.再次获取tail，并且和之前tail进行比较
                // 3.1 tail相等，说明生产者确实还没有push，如果继续等待，可能会产生很长时间的等待
                // 3.2 tail不等，说明生产者刚生产的元素，就被其他消费者pop了，
                //     当前队列处于一种快速生产、快速消费的繁忙状态，
                //     因此当前线程可以进入下一次turn的比较（这里需要注意必须更新old_tail，才能获取最新pop位置）
                // 3.3 当然也可以不进行判断，直接return，这样系统也会正常运行，
                //     但是会有线程失去大量原本可以消费，却放弃消费的情况，
                // 一有略微的竞争，你就放弃了，都不愿意再努力一下，最终成绩当然不会好，下面是两种机制的对比：
                // +---------+----------+----------+----------+----------+          
                // |   ---   |  数据量  |  总次数  | 平均POP量 |  标准差  |          
                // +---------+----------+----------+----------+----------+          
                // | return  |  10000   |    89    |   113    |    209   |          
                // +---------+----------+----------+----------+----------+          
                // |  wait   |  10000   |    11    |   902    |   1038   |          
                // +---------+----------+----------+----------+----------+     
                // wait的机制会减少操作量，数据竞争，而且一次性pop的总量更大
                old_tail = tail_.load(std::memory_order_acquire);
                if (prev_old_tail == old_tail){
                    // 3.1 tail相等，说明一段时间内生产者没有生产，当前队列不是很繁忙
                    // 生产者下一次什么时候生产，也不可预料，如果继续等待，则等待时间不确定
                    // 因此，直接返回，避免潜在的长时间等待。
                    return consume_cnt;
                }
                // 如果tail变化了，则可以通过刚刚获取的tail值，对其所在位置的slot进行pop
                continue;
            }
        }
    }

    uint64_t size() const {
        return static_cast<ptrdiff_t>(head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed));
    }

    bool empty() const {
        return size()<=0;
    }

private:
    Slot<T>& get_slot(uint64_t i) {
        return slots_[i & mask_];
    }

    constexpr uint64_t turn(uint64_t i) const {
        return (i / capacity_);
    }
    

private:
#if defined(__has_cpp_attribute) && __has_cpp_attribute(no_unique_address)
    Alloc alloc_ [[no_unique_address]];
#else
    Alloc alloc_;
#endif
    uint64_t capacity_;
    uint64_t mask_;
    Slot<T>* slots_;

    alignas(64) std::atomic<uint64_t> head_;
    alignas(64) std::atomic<uint64_t> tail_;
};


}