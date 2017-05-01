///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_CANCELLATION_SOURCE_HPP_INCLUDED
#define CPPCORO_CANCELLATION_SOURCE_HPP_INCLUDED

namespace cppcoro
{
	class cancellation_token;

	namespace detail
	{
		class cancellation_state;
	}

	class cancellation_source
	{
	public:

		/// Construct to a new cancellation source.
		cancellation_source();

		/// Create a new reference to the same underlying cancellation
		/// source as \p other.
		cancellation_source(const cancellation_source& other) noexcept;

		cancellation_source(cancellation_source&& other) noexcept;

		~cancellation_source();

		cancellation_source& operator=(const cancellation_source& other) noexcept;

		cancellation_source& operator=(cancellation_source&& other) noexcept;

		/// Query if this cancellation source can be cancelled.
		///
		/// A cancellation source object will not be cancellable if it has
		/// previously been moved into another cancellation_source instance
		/// or was copied from a cancellation_source that was not cancellable.
		bool can_be_cancelled() const noexcept;

		/// Obtain a cancellation token that can be used to query if
		/// cancellation has been requested on this source.
		///
		/// The cancellation token can be passed into functions that you
		/// may want to later be able to request cancellation.
		cancellation_token token() const noexcept;

		/// Request cancellation of operations that were passed an associated
		/// cancellation token.
		///
		/// Any cancellation callback registered via a cancellation_registration
		/// object will be called inside this function by the first thread to
		/// call this method.
		///
		/// This operation is a no-op if can_be_cancelled() returns false.
		void request_cancellation();

		/// Query if some thread has called 'request_cancellation()' on this
		/// cancellation_source.
		bool is_cancellation_requested() const noexcept;

	private:

		detail::cancellation_state* m_state;

	};
}

#endif
