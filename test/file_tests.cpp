///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <cppcoro/io_service.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/write_only_file.hpp>
#include <cppcoro/read_write_file.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/lazy_task.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <random>
#include <thread>
#include <cassert>

#include "doctest/doctest.h"

TEST_SUITE_BEGIN("file");

namespace fs = std::experimental::filesystem;

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

	const std::experimental::filesystem::path& temp_dir()
	{
		return m_path;
	}

private:

	std::experimental::filesystem::path m_path;

};

TEST_CASE_FIXTURE(temp_dir_fixture, "write a file")
{
	cppcoro::io_service ioService;

	std::thread ioThread([&]
	{
		ioService.process_events();
	});

	auto waitForThreadOnExit = cppcoro::on_scope_exit([&]
	{
		ioThread.join();
	});

	auto filePath = temp_dir() / "foo";

	auto write = [&](cppcoro::io_context io) -> cppcoro::lazy_task<>
	{
		auto f = cppcoro::write_only_file::open(io, filePath);

		CHECK(f.size() == 0);

		char buffer[1024];
		for (int i = 0; i < sizeof(buffer); ++i)
		{
			buffer[i] = 'a' + (i % 26);
		}

		for (int chunk = 0; chunk < 10; ++chunk)
		{
			co_await f.write(chunk * sizeof(buffer), buffer, sizeof(buffer));
		}
	};

	auto read = [&](cppcoro::io_context io) -> cppcoro::lazy_task<>
	{
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

	auto run = [&]() -> cppcoro::task<>
	{
		try
		{
			auto io = ioService.get_context();
			co_await write(io);
			co_await read(io);
		}
		catch (...)
		{
			FAIL("threw an exception");
			throw;
		}
	};

	auto t = run();

	waitForThreadOnExit.call_now();

	REQUIRE(t.is_ready());
}

TEST_CASE_FIXTURE(temp_dir_fixture, "read write file")
{
	cppcoro::io_service ioService;

	auto run = [&]() -> cppcoro::task<>
	{
		auto ioContext = ioService.get_context();
		auto f = cppcoro::read_write_file::open(ioContext, temp_dir() / "foo.txt");

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

	auto t = run();

	ioService.process_events();

	CHECK(t.is_ready());
}

TEST_SUITE_END();
