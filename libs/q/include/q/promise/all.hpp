/*
 * Copyright 2013 Gustaf Räntilä
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBQ_PROMISE_ALL_HPP
#define LIBQ_PROMISE_ALL_HPP

#include <q/temporarily_copyable.hpp>
#include <q/type_traits.hpp>

#include <algorithm>

namespace q {

template< typename... Promises >
struct merge_promise_arguments;

template< typename First, typename... Rest >
struct merge_promise_arguments< First, Rest... >
: std::conditional<
	sizeof...( Rest ) == 0,
	typename std::decay< First >::type::argument_types,
	typename merge<
		arguments,
		typename std::decay< First >::type::argument_types,
		typename std::decay< Rest >::type::argument_types...
	>::type
>::type
{ };

template< typename Only >
struct merge_promise_arguments< Only >
: std::decay< Only >::type::argument_types
{ };

template< >
struct merge_promise_arguments< >
: q::arguments< >
{ };

static inline promise< std::tuple< > > all( )
{
	return with( );
}

// TODO: Consider a different, less recursive, design
template< typename First, typename... Rest >
typename std::enable_if<
	are_promises<
		typename std::decay< First >::type,
		typename std::decay< Rest >::type...
	>::value,
	promise<
		typename merge_promise_arguments< First, Rest... >::tuple_type
	>
>::type
all( First&& first, Rest&&... rest )
{
	auto when_rest = all( std::forward< Rest >( rest )... );
	auto when_rest_tmp = Q_MOVE_TEMPORARILY_COPYABLE( when_rest );

	typedef typename std::decay< First >::type::tuple_type
		first_tuple_type;
	typedef typename merge_promise_arguments< Rest... >::tuple_type
		rest_tuple_type;
	typedef typename merge_promise_arguments< First, Rest... >::tuple_type
		full_tuple_type;

	return first.then( [ when_rest_tmp ]( first_tuple_type&& data ) mutable
	-> promise< full_tuple_type >
	{
		auto rest_promise = when_rest_tmp.consume( );
		auto data_tmp = Q_MOVE_TEMPORARILY_COPYABLE( data );

		auto failer = [ ]( std::exception_ptr&& e ) mutable
		{
			std::rethrow_exception( e );
		};

		auto completer = [ data_tmp ]
		                 ( rest_tuple_type&& rest_data ) mutable
		{
			return std::tuple_cat(
				data_tmp.consume( ),
				std::move( rest_data ) );
		};

		auto full_value = rest_promise.fail( failer ).then( completer );

		return std::move( full_value );
	} );
}

/**
 * Combines a std::vector of promises (of the same type) into one promise
 * which resolves to a std::vector of the combined result types, in order.
 *
 * If the list of promises contain std::tuples with zero or one elements, the
 * resulting promise will be a promise of a std::vector of the values within
 * the tuples (or a promise without a value) wrapped in q::expect.
 * If the promises contain std::tuples with two or more elements, the
 * resulting promise will contain a list of std::tuples wrapped in q::expect.
 */
template< typename List >
typename std::enable_if<
	is_vector< List >::value &&
	is_promise< typename std::decay< List >::type::value_type >::value &&
	( std::tuple_size<
		typename std::decay< List >::type::value_type::tuple_type
	>::value >= 2 ),
	promise<
		std::tuple<
			std::vector<
				typename std::decay< List >::type::value_type::tuple_type
			>
		>
	>
>::type
all( List&& list )
{
	typedef typename std::decay< List >::type          list_type;
	typedef typename list_type::value_type::tuple_type element_type;
	typedef expect< element_type >                     expect_type;
	typedef std::vector< element_type >                return_type;
	typedef std::vector< expect_type >                 expect_return_type;
	typedef combined_promise_exception< element_type > exception_type;

	auto deferred = detail::defer< return_type >::make( );

	std::size_t num = list.size( );
	auto expect_returns = std::make_shared< expect_return_type >( num );
	auto counter = std::make_shared< std::atomic< std::size_t > >( num );
	auto any_failure = std::make_shared< std::atomic< bool > >( false );

	auto resolver = [ deferred, expect_returns, any_failure ]( )
	{
		if ( any_failure->load( std::memory_order_seq_cst ) )
		{
			// At least one element's promise failed
			deferred->set_exception(
				std::make_exception_ptr(
					exception_type(
						std::move( *expect_returns )
					)
				)
			);
		}
		else
		{
			auto returns = return_type( expect_returns.size( ) );
			std::transform(
				expect_returns->begin( ),
				expect_returns->end( ),
				returns.begin( ),
				[ ]( expect_type& data )
				{
					return std::move( data.consume( ) );
				} );

			deferred->set_value( std::move( returns ) );
		}
	};

	auto part_completion = [ expect_returns, counter, resolver ]
	                       ( std::size_t index, expect_type&& data ) mutable
	{
		expect_returns->at( index ) = std::move( data );

		auto prev = counter->fetch_sub( 1, std::memory_order_seq_cst );

		if ( prev == 1 ) // Last element
			resolver( );
	};

	for ( std::size_t i = 0; i < num; ++i )
		list[ i ]
		.fail( [ i, part_completion, any_failure ]
		       ( std::exception_ptr&& e ) mutable
		{
			any_failure->store( true, std::memory_order_seq_cst );
			part_completion(
				i,
				refuse< element_type >( std::move( e ) ) );
		} )
		.then( [ i, part_completion ]
		       ( element_type&& data ) mutable
		{
			part_completion(
				i,
				fulfill< element_type >( std::move( data ) ) );
		} );

	return deferred->get_promise( );
}

template< typename List >
typename std::enable_if<
	is_vector< List >::value &&
	is_promise< typename std::decay< List >::type::value_type >::value &&
	std::tuple_size<
		typename std::decay< List >::type::value_type::tuple_type
	>::value == 1,
	promise< std::tuple< std::vector<
		typename std::tuple_element<
			0,
			typename std::decay<
				List
			>::type::value_type::tuple_type
		>::type
	> > >
>::type
all( List&& list )
{
	typedef typename std::decay< List >::type                  list_type;
	typedef typename list_type::value_type::tuple_type         tuple_type;
	typedef typename std::tuple_element< 0, tuple_type >::type element_type;
	typedef expect< element_type >                             expect_type;
	typedef std::vector< element_type >                        return_type;
	typedef std::vector< expect_type >                         expect_return_type;
	typedef combined_promise_exception< element_type >         exception_type;

	auto deferred = detail::defer< return_type >::make( );

	std::size_t num = list.size( );
	auto expect_returns = std::make_shared< expect_return_type >( num );
	auto counter = std::make_shared< std::atomic< std::size_t > >( num );
	auto any_failure = std::make_shared< std::atomic< bool > >( false );

	auto resolver = [ deferred, expect_returns, any_failure ]( )
	{
		if ( any_failure->load( std::memory_order_seq_cst ) )
		{
			// At least one element's promise failed
			deferred->set_exception(
				std::make_exception_ptr(
					exception_type(
						std::move( *expect_returns )
					)
				)
			);
		}
		else
		{
			auto returns = return_type( expect_returns->size( ) );
			std::transform(
				expect_returns->begin( ),
				expect_returns->end( ),
				returns.begin( ),
				[ ]( expect_type& data )
				{
					return std::move( data.consume( ) );
				} );

			deferred->set_value( std::move( returns ) );
		}
	};

	auto part_completion = [ expect_returns, counter, resolver ]
	                       ( std::size_t index, expect_type&& data ) mutable
	{
		expect_returns->at( index ) = std::move( data );

		auto prev = counter->fetch_sub( 1, std::memory_order_seq_cst );

		if ( prev == 1 ) // Last element
			resolver( );
	};

	for ( std::size_t i = 0; i < num; ++i )
		list[ i ]
		.fail( [ i, part_completion, any_failure ]
		       ( std::exception_ptr&& e ) mutable
		{
			any_failure->store( true, std::memory_order_seq_cst );
			part_completion(
				i,
				refuse< element_type >( std::move( e ) ) );
		} )
		.then( [ i, part_completion ]
		       ( element_type&& data ) mutable
		{
			part_completion(
				i,
				fulfill< element_type >( std::move( data ) ) );
		} );

	return deferred->get_promise( );
}

template< typename List >
typename std::enable_if<
	is_vector< List >::value &&
	is_promise< typename std::decay< List >::type::value_type >::value &&
	std::tuple_size<
		typename std::decay< List >::type::value_type::tuple_type
	>::value == 0,
	promise< std::tuple< > >
>::type
all( List&& list )
{
	typedef expect< void >                     expect_type;
	typedef std::vector< expect_type >         expect_return_type;
	typedef combined_promise_exception< void > exception_type;

	auto deferred = ::q::make_shared< detail::defer< > >( );

	std::size_t num = list.size( );
	auto expect_returns = std::make_shared< expect_return_type >( num );
	auto counter = std::make_shared< std::atomic< std::size_t > >( num );
	auto any_failure = std::make_shared< std::atomic< bool > >( false );

	auto resolver = [ deferred, expect_returns, any_failure ]( )
	{
		if ( any_failure->load( std::memory_order_seq_cst ) )
		{
			// At least one element's promise failed
			deferred->set_exception(
				std::make_exception_ptr(
					exception_type(
						std::move( *expect_returns )
					)
				)
			);
		}
		else
		{
			deferred->set_value( );
		}
	};

	auto part_completion = [ expect_returns, counter, resolver ]
	                       ( std::size_t index, expect_type&& data ) mutable
	{
		expect_returns->at( index ) = std::move( data );

		auto prev = counter->fetch_sub( 1, std::memory_order_seq_cst );

		if ( prev == 1 ) // Last element
			resolver( );
	};

	for ( std::size_t i = 0; i < num; ++i )
		list[ i ]
		.fail( [ i, part_completion, any_failure ]
		       ( std::exception_ptr&& e ) mutable
		{
			any_failure->store( true, std::memory_order_seq_cst );
			part_completion( i, refuse< void >( std::move( e ) ) );
		} )
		.then( [ i, part_completion ]( ) mutable
		{
			part_completion( i, fulfill< void >( ) );
		} );

	return deferred->get_promise( );
}

} // namespace q

#endif // LIBQ_PROMISE_ALL_HPP
