///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/write_only_file.hpp>
#include <cppcoro/read_write_file.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <random>
#include <thread>
#include <cassert>
#include <string>

#include "io_service_fixture.hpp"

#include <ostream>
#include "doctest/doctest.h"

TEST_SUITE_BEGIN("file");

namespace fs = std::filesystem;

namespace
{
	class temp_dir_fixture
	{
	public:

		temp_dir_fixture()
		{
			auto tempDir = fs::temp_directory_path();

			std::random_device random;
			for (int attempt = 1;; ++attempt)
			{
				m_path = tempDir / std::to_string(random());
				try
				{
					fs::create_directories(m_path);
					return;
				}
				catch (const fs::filesystem_error&)
				{
					if (attempt == 10)
					{
						throw;
					}
				}
			}
		}

		~temp_dir_fixture()
		{
			fs::remove_all(m_path);
		}

		const std::filesystem::path& temp_dir()
		{
			return m_path;
		}

	private:

		std::filesystem::path m_path;

	};

	class temp_dir_with_io_service_fixture :
		public io_service_fixture,
		public temp_dir_fixture
	{
	};
}

TEST_CASE_FIXTURE(temp_dir_fixture, "write a file")
{
	auto filePath = temp_dir() / "foo";

	cppcoro::io_service ioService;

	auto write = [&](cppcoro::io_service& ioService) -> cppcoro::task<>
	{
		std::printf(" starting write\n"); std::fflush(stdout);

		auto f = cppcoro::write_only_file::open(ioService, filePath);

		CHECK(f.size() == 0);

		char buffer[1024];
		char c = 'a';
		for (int i = 0; i < sizeof(buffer); ++i, c = (c == 'z' ? 'a' : c + 1))
		{
			buffer[i] = c;
		}

		for (int chunk = 0; chunk < 10; ++chunk)
		{
			co_await f.write(chunk * sizeof(buffer), buffer, sizeof(buffer));
		}
	};

	auto read = [&](cppcoro::io_service& io) -> cppcoro::task<>
	{
		std::printf(" starting read\n"); std::fflush(stdout);

		auto f = cppcoro::read_only_file::open(io, filePath);

		const auto fileSize = f.size();

		CHECK(fileSize == 10240);

		char buffer[20];

		for (std::uint64_t i = 0; i < fileSize;)
		{
			auto bytesRead = co_await f.read(i, buffer, 20);
			for (size_t j = 0; j < bytesRead; ++j, ++i)
			{
				CHECK(buffer[j] == ('a' + ((i % 1024) % 26)));
			}
		}
	};

	cppcoro::sync_wait(cppcoro::when_all(
		[&]() -> cppcoro::task<int>
		{
			auto stopOnExit = cppcoro::on_scope_exit([&] { ioService.stop(); });
			co_await write(ioService);
			co_await read(ioService);
			co_return 0;
		}(),
		[&]() -> cppcoro::task<int>
		{
			ioService.process_events();
			co_return 0;
		}()));
}

TEST_CASE_FIXTURE(temp_dir_with_io_service_fixture, "read write file")
{
	auto run = [&]() -> cppcoro::task<>
	{
		cppcoro::io_work_scope ioScope{ io_service() };
		auto f = cppcoro::read_write_file::open(io_service(), temp_dir() / "foo.txt");

		char buffer1[100];
		std::memset(buffer1, 0xAB, sizeof(buffer1));

		co_await f.write(0, buffer1, sizeof(buffer1));

		char buffer2[50];
		std::memset(buffer2, 0xCC, sizeof(buffer2));

		co_await f.read(0, buffer2, 50);
		CHECK(std::memcmp(buffer1, buffer2, 50) == 0);

		co_await f.read(50, buffer2, 50);
		CHECK(std::memcmp(buffer1 + 50, buffer2, 50) == 0);
	};

	cppcoro::sync_wait(run());
}

TEST_CASE_FIXTURE(temp_dir_with_io_service_fixture, "cancel read")
{
	cppcoro::sync_wait([&]() -> cppcoro::task<>
	{
		cppcoro::io_work_scope ioScope{ io_service() };
		auto f = cppcoro::read_write_file::open(io_service(), temp_dir() / "foo.txt");

		f.set_size(20 * 1024 * 1024);

		cppcoro::cancellation_source canceller;

		try
		{
			(void)co_await cppcoro::when_all(
				[&]() -> cppcoro::task<int>
			{
				const auto fileSize = f.size();
				const std::size_t bufferSize = 64 * 1024;
				auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);
				std::uint64_t offset = 0;
				while (offset < fileSize)
				{
					auto bytesRead = co_await f.read(offset, buffer.get(), bufferSize, canceller.token());
					offset += bytesRead;
				}
				WARN("should have been cancelled");
				co_return 0;
			}(),
				[&]() -> cppcoro::task<int>
			{
				using namespace std::chrono_literals;

				co_await io_service().schedule_after(1ms);
				canceller.request_cancellation();
				co_return 0;
			}());
			WARN("Expected exception to be thrown");
		}
		catch (const cppcoro::operation_cancelled&)
		{
		}
	}());
}

TEST_SUITE_END();
