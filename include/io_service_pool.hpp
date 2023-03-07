#pragma once

#include <vector>

#include "io_service.hpp"
#include "types.hpp"

namespace sheep {


class io_service_pool
{
public:
    explicit io_service_pool(std::size_t init_size) noexcept
    {
        pool_.reserve(init_size);
        pool_.emplace_back(io_service());
        pool_[0].init(io_service::kDEFAULT_URING_QUEUE_DEPTH);
        for (auto i=1; i!=init_size; ++i) {
            pool_.emplace_back(io_service());
            pool_[i].init(io_service::kDEFAULT_URING_QUEUE_DEPTH, pool_[0].get_uring_fd());
        }
    }

    io_service& get_io_service(thread_meta thread) noexcept {
        assert(thread.thread_id < pool_.size());
        return pool_[thread.thread_id];
    }
    
    io_service& get_io_service() noexcept { return get_io_service(this_thread()); }
    thread_meta this_thread() noexcept { return this_thread_; }

    std::size_t size() const noexcept { return pool_.size(); }
    
private:
    inline static thread_local thread_meta this_thread_;
    std::vector<io_service> pool_;

};

}