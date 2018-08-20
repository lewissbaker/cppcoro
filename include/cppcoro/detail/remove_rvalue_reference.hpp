///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_REMOVE_RVALUE_REFERENCE_HPP_INCLUDED
#define CPPCORO_DETAIL_REMOVE_RVALUE_REFERENCE_HPP_INCLUDED

namespace cppcoro
{
	namespace detail
	{
		template<typename T>
		struct remove_rvalue_reference
		{
			using type = T;
		};

		template<typename T>
		struct remove_rvalue_reference<T&&>
		{
			using type = T;
		};

		template<typename T>
		using remove_rvalue_reference_t = typename remove_rvalue_reference<T>::type;
	}
}

#endif
