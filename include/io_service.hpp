#pragma once

#include <cassert>
#include <coroutine>
#include <cstring>
#include <deque>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>
#include <set>
#include <stop_token>
#include <sys/socket.h>
#include <type_traits>
#include <vector>

#include "task.hpp"

namespace sheep {

struct resume_handle {
	int result{0}; // should largger than 0
	std::coroutine_handle<> coro;

	void resume(int res) noexcept {
		result = res;
		coro.resume();
	}
};

struct [[nodiscard]] io_awaitable {
	io_awaitable(io_uring_sqe *sqe) noexcept : sqe_(sqe) {}

	struct await_uring {
		explicit await_uring(io_uring_sqe *sqe) : sqe_(sqe) {}
		io_uring_sqe *sqe_;
		resume_handle resume_handler_;

		constexpr bool await_ready() const noexcept { return false; }

		void await_suspend(std::coroutine_handle<> coro_handle) noexcept {
			resume_handler_.coro = coro_handle;
			io_uring_sqe_set_data(sqe_, &resume_handler_);
		}

		int await_resume() const noexcept { return resume_handler_.result; }
	};

	await_uring operator co_await() { return await_uring{sqe_}; }

	private:
	io_uring_sqe *sqe_;
};

class io_service {
public:
	static constexpr int kDEFAULT_URING_QUEUE_DEPTH = 64;

	io_service(int entries = kDEFAULT_URING_QUEUE_DEPTH, int uring_fd = -1)
		: ring_(std::make_unique<struct io_uring>()) {}

	~io_service() noexcept {
		if (ring_.get())
		io_uring_queue_exit(ring_.get());
	}

	io_service(const io_service &) = delete;
	io_service &operator=(const io_service &) = delete;
	io_service(io_service &&other) noexcept : ring_(std::move(other.ring_)) {}
	io_service &operator=(io_service &&other) noexcept {
		ring_ = std::move(other.ring_);
		return *this;
	}

	int get_uring_fd() noexcept { return ring_->ring_fd; }

	void init(int entries = kDEFAULT_URING_QUEUE_DEPTH,
				int uring_fd = -1) noexcept {
		struct io_uring_params param;
		std::memset(&param, 0, sizeof(param));
		if (uring_fd > 0) {
			param.flags |= IORING_SETUP_ATTACH_WQ;
			param.wq_fd = uring_fd;
		}

		int ret = io_uring_queue_init_params(entries, ring_.get(), &param);
		if (ret < 0) {
			std::cerr << "Failed to init io_uring, reason: " << std::strerror(errno);
			std::cerr << ". Abort!";
			abort();
		}
	}

public:
	template <typename T> T run_task(task<T> &t) {
		t.resume();
		while (!t.done()) 
		{
			io_uring_submit_and_wait(ring_.get(), /*completion numbers*/ 1);
			int cqe_num = 0;
			io_uring_cqe *cqe;
			auto head{0};
			io_uring_for_each_cqe(ring_.get(), head, cqe) {
				++cqe_num;
				// obtain user data: pointer to resume_handler
				auto resume_handler =
					static_cast<resume_handle *>(io_uring_cqe_get_data(cqe));
				if (resume_handler)
				resume_handler->resume(/*result code*/ cqe->res);
			}
			/*
			* Must be called after io_uring_for_each_cqe()
			*/
			io_uring_cq_advance(ring_.get(), cqe_num);
		}

		return t.get_result();
	}

	void wait_io_and_resume_coroutine() {
		io_uring_submit_and_wait(ring_.get(), 1);
		int cqe_num = 0;
		io_uring_cqe *cqe;
		auto head{0};
		io_uring_for_each_cqe(ring_.get(), head, cqe) 
		{
			++cqe_num;
			auto resume_handler =
				static_cast<resume_handle *>(io_uring_cqe_get_data(cqe));
			if (resume_handler) {
				resume_handler->resume(cqe->res);
			}
		}

		io_uring_cq_advance(ring_.get(), cqe_num);
	}

	void run_single_coro(std::coroutine_handle<> t) {
		t.resume();
		while (!t.done()) 
		{
			io_uring_submit_and_wait(ring_.get(), /*completion numbers*/ 1);
			int cqe_num = 0;
			io_uring_cqe *cqe;
			auto head{0};
			io_uring_for_each_cqe(ring_.get(), head, cqe) {
				++cqe_num;
				// obtain user data: pointer to resume_handler
				auto resume_handler =
					static_cast<resume_handle *>(io_uring_cqe_get_data(cqe));
				if (resume_handler)
				resume_handler->resume(/*result code*/ cqe->res);
			}
			/*
			* Must be called after io_uring_for_each_cqe()
			*/
			io_uring_cq_advance(ring_.get(), cqe_num);
		}

		return;
	}

	template <typename Functor, typename... Args>
	void wait_consume(Functor &fn, Args &&...args) {
		// wait_nr = 0, in case there is no completion.
		io_uring_submit_and_wait(ring_.get(), 0);
		int cqe_num = 0;
		io_uring_cqe *cqe;
		auto head{0};
		io_uring_for_each_cqe(ring_.get(), head, cqe) 
		{
			++cqe_num;
			auto resume_handler =
				static_cast<resume_handle *>(io_uring_cqe_get_data(cqe));
			if (resume_handler != nullptr) {
				int res = cqe->res;
				fn(resume_handler, res, std::forward<Args>(args)...);
			}
		}
		io_uring_cq_advance(ring_.get(), cqe_num);
	}

	io_uring_sqe *get_sqe() noexcept {
		auto *sqe = io_uring_get_sqe(ring_.get());
		if (sqe == nullptr) {
			io_uring_submit(ring_.get());
			sqe = io_uring_get_sqe(ring_.get());
		}
		return sqe;
	}


public: // syscalls / io interfaces
	io_awaitable nop() noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_nop(sqe);
		return io_awaitable{sqe};
	}

	/// submit read operation
	/// \param fd the file descriptor to read from.
	/// \param buf the buffer to copy the read data into.
	/// \param nbytes number of bytes to read.
	/// \param offset absolute offset of the file to read from.
	io_awaitable read(int fd, void *buf, unsigned nbytes, off_t offset) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_read(sqe, fd, buf, nbytes, offset);
		return io_awaitable{sqe};
	}

	/// submit "scatter" read operation
	/// \param fd the file descriptor to read from.
	/// \param iovecs pointer to an array of iovec structures.
	/// \param nv_vecs number of iovec instances in the array pointed to by the
	/// iovecs arg. \param offset absolute offset fo the file to read from
	io_awaitable readv(int fd, const struct iovec *iovecs, unsigned nr_vecs,
						off_t offset) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_readv(sqe, fd, iovecs, nr_vecs, offset);
		return io_awaitable{sqe};
	}

	/// submit read operation, read data to
	/// fixed set of pre-allocated buffers registered via io_uring_register()
	/// \param fd the file descriptor to read from.
	/// \param buf the buffer to copy the read data into.
	/// \param nbytes number of bytes to read.
	/// \param offset absolute offset of the file to read from.
	/// \param buf_index index of the set of pre-allocated buffers to use.
	io_awaitable read_fixed(int fd, void *buf, unsigned nbytes, off_t offset,
							int buf_index) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_read_fixed(sqe, fd, buf, nbytes, offset, buf_index);
		return io_awaitable{sqe};
	}

	/// submit write operation
	/// \param fd the file descriptor to write to.
	/// \param buf the buffer to write data from.
	/// \param nbytes number of bytes to write.
	/// \param offset absolute offset of the file to write.
	io_awaitable write(int fd, const void *buf, unsigned nbytes,
						off_t offset) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_write(sqe, fd, buf, nbytes, offset);
		return io_awaitable{sqe};
	}

	/// submit "gather" write operation
	/// \param fd the file descriptor to write to.
	/// \param iovecs pointer to an array of iovec structures.
	/// \param nv_vecs number of iovec instances in the array pointed to by the
	/// iovecs arg. \param offset absolute offset of the file to write to.
	io_awaitable writev(int fd, const struct iovec *iovecs, unsigned nr_vecs,
						off_t offset) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_writev(sqe, fd, iovecs, nr_vecs, offset);
		return io_awaitable{sqe};
	}

	/// submit write operation, write data to fixed
	/// set of pre-allocated buffers registered via io_uring_register()
	/// \param fd the file descriptor to write to.
	/// \param buf the buffer to write data from.
	/// \param nbytes number of bytes to write.
	/// \param offset absolute offset of the file to write.
	/// \param buf_index index of the set of pre-allocated buffers to use.
	io_awaitable write_fixed(int fd, const void *buf, unsigned nbytes,
							off_t offset, int buf_index) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_write_fixed(sqe, fd, buf, nbytes, offset, buf_index);
		return io_awaitable{sqe};
	}

	/// submit fsync operation, flush/sync buffers of file's data and metadata to
	/// disk \param fd the file descriptor to sync. \param fsync_flags either be 0
	/// or IORING_FSYNC_DATASYNC, makes it act like fdatasync.
	io_awaitable fsync(int fd, unsigned fsync_flags) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_fsync(sqe, fd, fsync_flags);
		return io_awaitable{sqe};
	}

	/// submit close operation
	/// \param fd the file descriptor to close.
	io_awaitable close(int fd) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_close(sqe, fd);
		return io_awaitable{sqe};
	}

	/// submit openat operation, open file in a path
	/// relative to the directory(represented by dfd)
	/// \param dfd directory file descriptor.
	/// \param path path name of the file to be opened.
	/// \param flags access mode flags, same as open.
	/// \param mode file permission bits applied when creating new file, same as
	/// open.
	io_awaitable openat(int dfd, const char *path, int flags,
						mode_t mode) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_openat(sqe, dfd, path, flags, mode);
		return io_awaitable{sqe};
	}

	/// submit statx operation, the statx syscall gets meta information of a file.
	/// \param dfd if path is empty and AT_EMPTY_PATH flag is specified
	/// then the target file is specified by the dfd.
	/// \param path can either be aboluste path or relative path combine with dfd.
	/// \param mask The mask argument to statx() is used to tell the kernel which
	/// fields the caller is interested in.
	/// \param flags influence how the path name is looked up.
	/// \param statxbuf the buf to write data to.
	io_awaitable statx(int dfd, const char *path, int flags, unsigned mask,
						struct statx *statxbuf) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_statx(sqe, dfd, path, flags, mask, statxbuf);
		return io_awaitable{sqe};
	}

	/// submit splice operation, the splice syscall copies data
	/// between tow file descriptors without copying data.
	/// \param fd_in the file descriptor to read from.
	/// \param off_in offset of fd_in.
	/// \param nbytes number of bytes to copy.
	/// \param splice_flags bit mask influences the copy.
	io_awaitable splice(int fd_in, loff_t off_in, int fd_out, loff_t off_out,
						unsigned nbytes, unsigned splice_flags) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_splice(sqe, fd_in, off_in, fd_out, off_out, nbytes,
							splice_flags);
		return io_awaitable{sqe};
	}

	/// read data from a socket
	/// \param fd the socket to read from.
	/// \param msg pointer to an msghdr structure.
	/// \param flags bit mask influcens the read.
	io_awaitable recvmsg(int fd, struct msghdr *msg, unsigned flags) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_recvmsg(sqe, fd, msg, flags);
		return io_awaitable{sqe};
	}

	/// same as recvmsg, but for writing to a socket.
	io_awaitable sendmsg(int fd, const struct msghdr *msg,
						unsigned flags) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_sendmsg(sqe, fd, msg, flags);
		return io_awaitable{sqe};
	}

	/// read data from a socket, works with both tcp and udp sockets.
	/// \param sockfd the socket to read from.
	/// \param buf pinter to a buffer to read data into.
	/// \param len count of bytes to read.
	/// \param flags bit mask influences the read.
	io_awaitable recv(int sockfd, void *buf, size_t len, int flags) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_recv(sqe, sockfd, buf, len, flags);
		return io_awaitable{sqe};
	}

	/// same ad recv, but for writing to a socket.
	io_awaitable send(int sockfd, const void *buf, size_t len,
						int flags) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_send(sqe, sockfd, buf, len, flags);
		return io_awaitable{sqe};
	}

	/// accept connection-oriented(TCP) socket.
	/// \param fd the listening socket.
	/// \param addr pointer to the listening address.
	/// \param addrlen pointer to the size of sockaddr.
	/// \param flags bit mask that influences the listening socket.
	io_awaitable accept(int fd, sockaddr *addr, socklen_t *addrlen,
						int flags = 0) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_accept(sqe, fd, addr, addrlen, flags);
		return io_awaitable{sqe};
	}

	/// connect the socket.
	/// \param fd the listening socket.
	/// \param addr pointer to the address of the peer.
	/// \param addrlen pointer to the size of sockaddr.
	io_awaitable connect(int fd, sockaddr *addr, socklen_t addrlen) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_connect(sqe, fd, addr, addrlen);
		return io_awaitable{sqe};
	}

	/// wait for specified duration asynchronously
	/// \param ts expiration timespec.
	io_awaitable timeout(__kernel_timespec *ts, unsigned count = 0,
						unsigned flags = 0) noexcept {
		io_uring_sqe *sqe = get_sqe();
		io_uring_prep_timeout(sqe, ts, count, flags);
		return io_awaitable{sqe};
	}

private:
  	std::unique_ptr<io_uring> ring_;
};

} // namespace sheep
