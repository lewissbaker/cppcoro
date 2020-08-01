#include <cppcoro/async_generator.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/read_only_file.hpp>

#include <sys/ioctl.h>

#include <iostream>

namespace cppcoro
{
	struct test_file
	{
		io_service& m_ios;
		detail::safe_handle m_fd;
		test_file(cppcoro::io_service& ios, const std::filesystem::path& path)
			: m_ios{ ios }
			, m_fd{ open(path.c_str(), O_RDONLY) }
		{
			if (m_fd.fd() < 0)
			{
				throw std::system_error{ errno, std::system_category() };
			}
		}

		ssize_t size()
		{
			struct stat st;

			if (fstat(m_fd.fd(), &st) < 0)
				return -1;
			if (S_ISREG(st.st_mode))
			{
				return st.st_size;
			}
			else if (S_ISBLK(st.st_mode))
			{
				unsigned long long bytes;

				if (ioctl(m_fd.fd(), BLKGETSIZE64, &bytes) != 0)
					return -1;

				return bytes;
			}

			return -1;
		}

		struct read_operation
		{
			iovec m_data;
			size_t m_offset;
			detail::safe_handle& m_fd;
			cppcoro::io_service& m_ios;
			cppcoro::detail::lnx::message m_message;
			read_operation(
				detail::safe_handle& fd,
				cppcoro::io_service& ios,
				std::size_t offset,
				void* buffer,
				std::size_t size)
				: m_fd{ fd }
				, m_data{ buffer, size }
				, m_offset{ offset }
				, m_ios{ ios }
				, m_message{ cppcoro::detail::lnx::message_type::RESUME_TYPE, &m_awaitingCoroutine }
			{
			}

			bool await_suspend(stdcoro::coroutine_handle<> awaitingCoroutine)
			{
				m_awaitingCoroutine = awaitingCoroutine;
				auto sqe = io_uring_get_sqe(m_ios.native_uring_handle());
				io_uring_prep_readv(sqe, m_fd.fd(), &m_data, 1, 0);
				io_uring_sqe_set_data(sqe, &m_message);
				io_uring_submit(m_ios.native_uring_handle());
				return false;
			}

			bool await_ready() const noexcept { return false; }

			decltype(auto) await_resume() { return true; }

			stdcoro::coroutine_handle<> m_awaitingCoroutine;
		};

		read_operation read(std::uint64_t offset, void* buffer, std::size_t byteCount) noexcept
		{
			return read_operation{ m_fd, m_ios, offset, buffer, byteCount };
		}
	};
}  // namespace cppcoro

int main(int argc, char** argv)
{
	using namespace std::chrono_literals;
	using namespace cppcoro;
	io_service ios;
	std::string content;
	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto _ = on_scope_exit([&] { ios.stop(); });
			//			auto tf = test_file{ios, __FILE__};
			//            content.resize(tf.size());
			//            co_await tf.read(0, content.data(), content.size());
			//            std::cout << "got: " << content << '\n';
			//
			//			std::fill(begin(content), end(content), '\0');

//          co_await ios.schedule();
//          std::cout << "2\n";
//          co_await ios.schedule_after(1s);
//			std::cout << "1\n";
		  auto f = read_only_file::open(ios, __FILE__);
		  content.resize(f.size());
		  co_await f.read(0, content.data(), content.size());

		  std::cout << "got: " << content << '\n';

		  co_return;
		}(),
		[&]() -> task<> {
			ios.process_events();
			co_return;
		}()));
	return 0;
}
