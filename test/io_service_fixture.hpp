///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_TESTS_IO_SERVICE_FIXTURE_HPP_INCLUDED
#define CPPCORO_TESTS_IO_SERVICE_FIXTURE_HPP_INCLUDED

#include <cppcoro/io_service.hpp>

#include <thread>
#include <vector>

/// \brief
/// Test fixture that creates an io_service and starts up a background thread
/// to process I/O completion events.
///
/// Thread and io_service are shutdown on destruction.
struct io_service_fixture
{
public:

	io_service_fixture(std::uint32_t threadCount = 1, std::uint32_t maxEvents = 32)
		: m_ioService(maxEvents)
	{
		m_ioThreads.reserve(threadCount);
		try
		{
			for (std::uint32_t i = 0; i < threadCount; ++i)
			{
				m_ioThreads.emplace_back([this] { m_ioService.process_events(); });
			}
		}
		catch (...)
		{
			stop();
			throw;
		}
	}

	~io_service_fixture()
	{
		stop();
	}

	cppcoro::io_service& io_service() { return m_ioService; }

private:

	void stop()
	{
		m_ioService.stop();
		for (auto& thread : m_ioThreads)
		{
			thread.join();
		}
	}

	cppcoro::io_service m_ioService;
	std::vector<std::thread> m_ioThreads;

};

template<std::uint32_t thread_count, std::uint32_t max_events = 32>
struct io_service_fixture_with_threads : io_service_fixture
{
	io_service_fixture_with_threads()
		: io_service_fixture(thread_count, max_events)
	{}
};

#endif
