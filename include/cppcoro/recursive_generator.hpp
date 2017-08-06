///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_GENERATOR_HPP_INCLUDED
#define CPPCORO_GENERATOR_HPP_INCLUDED

#include <cppcoro/detail/dummy_coroutine.hpp>

#include <experimental/coroutine>
#include <type_traits>
#include <utility>
#include <cassert>

namespace cppcoro
{
	template<typename T>
	class recursive_generator
	{
	public:

		class promise_type final
		{
		public:

			promise_type() noexcept
				: m_value(nullptr)
				, m_exception(nullptr)
				, m_root(this)
				, m_parentOrLeaf(this)
			{}

			promise_type(const promise_type&) = delete;
			promise_type(promise_type&&) = delete;

			auto get_return_object() noexcept
			{
				return recursive_generator<T>{ *this };
			}

			std::experimental::suspend_always initial_suspend() noexcept
			{
				return {};
			}

			auto final_suspend() noexcept
			{
				class awaitable
				{
				public:

					awaitable(promise_type* promise) noexcept
						: m_promise(promise)
					{}

					constexpr bool await_ready() noexcept { return false; }

					std::experimental::coroutine_handle<> await_suspend(
						std::experimental::coroutine_handle<> coroutine)
					{
						// If we are the root generator then just suspend without
						// resuming anything else to return control back to operator++().
						// Otherwise, we resume the parent generator.
						auto* const root = m_promise->m_root;
						if (m_promise == root)
						{
							return detail::dummy_coroutine::handle();
						}

						// Set the leaf of the root generator to be the parent generator
						// we're about to resume.
						auto* parent = m_promise->m_parentOrLeaf;
						root->m_parentOrLeaf = parent;

						// Reset our root/leaf to be self-contained.
						m_promise->m_root = m_promise;
						m_promise->m_parentOrLeaf = m_promise;

						// Resume the parent now that we're suspended.
						return parent->handle();
					}

					void await_resume() noexcept {}

				private:

					promise_type* m_promise;

				};

				return awaitable{ this };
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void return_void() noexcept {}

			std::experimental::suspend_always yield_value(T& value) noexcept
			{
				m_root->m_value = std::addressof(value);
				return {};
			}

			std::experimental::suspend_always yield_value(T&& value) noexcept
			{
				m_root->m_value = std::addressof(value);
				return {};
			}

			auto yield_value(recursive_generator&& generator) noexcept
			{
				return yield_value(generator);
			}

			auto yield_value(recursive_generator& generator) noexcept
			{
				struct awaitable
				{

					awaitable(promise_type* childPromise)
						: m_childPromise(childPromise)
					{}

					bool await_ready() noexcept
					{
						return m_childPromise == nullptr || m_childPromise->is_complete();
					}

					std::experimental::coroutine_handle<> await_suspend(
						std::experimental::coroutine_handle<promise_type> parentHandle) noexcept
					{
						auto& parentPromise = parentHandle.promise();
						auto* rootPromise = parentPromise.m_root;
						m_childPromise->m_root = rootPromise;
						m_childPromise->m_parentOrLeaf = &parentPromise;
						rootPromise->m_parentOrLeaf = m_childPromise;
						return m_childPromise->handle();
					}

					void await_resume()
					{
						if (m_childPromise != nullptr)
						{
							m_childPromise->throw_if_exception();
						}
					}

				private:
					promise_type* m_childPromise;
				};

				return awaitable{ generator.m_promise };
			}

			// Don't allow any use of 'co_await' inside the recursive_generator coroutine.
			template<typename U>
			std::experimental::suspend_never await_transform(U&& value) = delete;

			void destroy() noexcept
			{
				handle().destroy();
			}

			void throw_if_exception()
			{
				if (m_exception != nullptr)
				{
					std::rethrow_exception(std::move(m_exception));
				}
			}

			bool is_complete() noexcept
			{
				return handle().done();
			}

			T& value() noexcept
			{
				assert(this == m_root);
				assert(!is_complete());
				return *m_value;
			}

			void pull() noexcept
			{
				assert(this == m_root);
				assert(!m_parentOrLeaf->is_complete());
				m_parentOrLeaf->resume();
			}

		private:

			auto handle() noexcept
			{
				return std::experimental::coroutine_handle<promise_type>::from_promise(*this);
			}

			void resume() noexcept
			{
				handle().resume();
			}

			T* m_value;
			std::exception_ptr m_exception;

			promise_type* m_root;

			// If this is the promise of the root generator then this field
			// is a pointer to the leaf promise.
			// For non-root generators this is a pointer to the parent promise.
			promise_type* m_parentOrLeaf;

		};

		recursive_generator() noexcept
			: m_promise(nullptr)
		{}

		recursive_generator(promise_type& promise) noexcept
			: m_promise(&promise)
		{}

		recursive_generator(recursive_generator&& other) noexcept
			: m_promise(other.m_promise)
		{
			other.m_promise = nullptr;
		}

		recursive_generator(const recursive_generator& other) = delete;
		recursive_generator& operator=(const recursive_generator& other) = delete;

		~recursive_generator()
		{
			if (m_promise != nullptr)
			{
				m_promise->destroy();
			}
		}

		recursive_generator& operator=(recursive_generator&& other) noexcept
		{
			if (this != &other)
			{
				if (m_promise != nullptr)
				{
					m_promise->destroy();
				}

				m_promise = other.m_promise;
				other.m_promise = nullptr;
			}

			return *this;
		}

		class iterator
		{
		public:

			using iterator_category = std::input_iterator_tag;
			// What type should we use for counting elements of a potentially infinite sequence?
			using difference_type = std::size_t;
			using value_type = std::remove_reference_t<T>;
			using reference = value_type&;
			using pointer = value_type*;

			iterator(promise_type* promise) noexcept
				: m_promise(promise)
			{}

			bool operator==(const iterator& other) const noexcept
			{
				return m_promise == other.m_promise;
			}

			bool operator!=(const iterator& other) const noexcept
			{
				return m_promise != other.m_promise;
			}

			iterator& operator++()
			{
				assert(m_promise != nullptr);
				assert(!m_promise->is_complete());

				m_promise->pull();
				if (m_promise->is_complete())
				{
					auto* temp = m_promise;
					m_promise = nullptr;
					temp->throw_if_exception();
				}

				return *this;
			}

			reference operator*() const noexcept
			{
				assert(m_promise != nullptr);
				return m_promise->value();
			}

			pointer operator->() const noexcept
			{
				return std::addressof(operator*());
			}

		private:

			promise_type* m_promise;

		};

		iterator begin()
		{
			if (m_promise != nullptr)
			{
				m_promise->pull();
				if (!m_promise->is_complete())
				{
					return iterator(m_promise);
				}

				m_promise->throw_if_exception();
			}

			return iterator(nullptr);
		}

		iterator end() noexcept
		{
			return iterator(nullptr);
		}

		void swap(recursive_generator& other) noexcept
		{
			std::swap(m_promise, other.m_promise);
		}

	private:

		friend class promise_type;

		promise_type* m_promise;

	};

	template<typename T>
	void swap(recursive_generator<T>& a, recursive_generator<T>& b) noexcept
	{
		a.swap(b);
	}
}

#endif
