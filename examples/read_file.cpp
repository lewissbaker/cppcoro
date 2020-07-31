#include <cppcoro/async_generator.hpp>
#include <cppcoro/single_consumer_event.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cppcoro/async_generator.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all_ready.hpp>

#include <aio.h>
#include <filesystem>
#include <future>

#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

struct file
{
    cppcoro::io_service &m_ios;
	int m_fd = -1;
	file(cppcoro::io_service &ios, const std::filesystem::path& path):
	    m_ios{ios}
	{
		m_fd = open(path.c_str(), O_RDONLY);
		if (m_fd < 0)
		{
			throw std::system_error{ errno, std::system_category() };
		}
	}
	~file() noexcept
	{
		if (m_fd > 0)
		{
			close(m_fd);
		}
	}
	ssize_t size()
	{
		struct stat st;

		if (fstat(m_fd, &st) < 0)
			return -1;
		if (S_ISREG(st.st_mode))
		{
			return st.st_size;
		}
		else if (S_ISBLK(st.st_mode))
		{
			unsigned long long bytes;

			if (ioctl(m_fd, BLKGETSIZE64, &bytes) != 0)
				return -1;

			return bytes;
		}

		return -1;
	}

	struct read_operation
	{
		iovec m_data;
		size_t m_offset;
		int m_fd;
        cppcoro::io_service &m_ios;
		cppcoro::detail::lnx::message m_message;
		read_operation(int fd, cppcoro::io_service &ios, std::size_t offset, void* buffer, std::size_t size)
			: m_fd{ fd }
			, m_data{ buffer, size }
			, m_offset{ offset }
		    , m_ios{ ios }
		    , m_message { cppcoro::detail::lnx::message_type::RESUME_TYPE, &m_awaitingCoroutine }
		{
		}

		bool await_suspend(stdcoro::coroutine_handle<> awaitingCoroutine)
		{
            m_awaitingCoroutine = awaitingCoroutine;
            auto sqe = io_uring_get_sqe(m_ios.native_uring_handle());
            io_uring_prep_readv(sqe, m_fd, &m_data, 1, 0);
            io_uring_sqe_set_data(sqe, &m_message);
            io_uring_submit(m_ios.native_uring_handle());
            return false;
		}

        bool await_ready() const noexcept {
            return false;

        }

		decltype(auto) await_resume() {
			return true;
		}

		stdcoro::coroutine_handle<> m_awaitingCoroutine;
	};

	read_operation read(std::uint64_t offset, void* buffer, std::size_t byteCount) const noexcept
	{
		return read_operation{ m_fd, m_ios, offset, buffer, byteCount };
	}
};

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;
	cppcoro::io_service ios;
	(void)cppcoro::sync_wait(cppcoro::when_all(
		[&]() -> cppcoro::task<> {
          auto _ = cppcoro::on_scope_exit([&]{
            ios.stop();
          });
			file f{ios,  __FILE__ };
			auto size = f.size();
			auto content = std::make_unique<char[]>(size);
			co_await f.read(0, content.get(), size);
			std::cout << "got: " << content.get() << '\n';
            co_return;
		}(),
		[&]() -> cppcoro::task<> {
          ios.process_events();
		  co_return;
		}()));
	return 0;
}
