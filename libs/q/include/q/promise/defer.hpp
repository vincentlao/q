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

#ifndef LIBQ_PROMISE_DEFER_HPP
#define LIBQ_PROMISE_DEFER_HPP

namespace q {

namespace detail {

template< typename... T >
class defer
: public std::enable_shared_from_this< defer< T... > >
{
public:
	typedef std::tuple< T... >                              tuple_type;
	typedef expect< tuple_type >                            expect_type;
	typedef detail::promise_state_data< tuple_type, false > state_data_type;
	typedef detail::promise_state< tuple_type, false >      state_type;
	typedef promise< tuple_type >                           promise_type;
	typedef shared_promise< tuple_type >                    shared_promise_type;

	void set_expect( expect_type&& exp )
	{
		if ( exp.has_exception( ) )
			set_exception( exp.exception( ) );
		else
			set_value( exp.consume( ) );
	}

	inline void set_value( tuple_type&& tuple )
	{
		auto value = ::q::fulfill< tuple_type >( std::move( tuple ) );
		promise_.set_value( std::move( value ) );
		signal_->done( );
	}

	inline void set_value( const tuple_type& tuple )
	{
		auto value = ::q::fulfill< tuple_type >( tuple );
		promise_.set_value( std::move( value ) );
		signal_->done( );
	}

	template< typename... Args >
	typename std::enable_if<
		::q::is_argument_same_or_convertible<
			q::arguments< Args... >,
			q::arguments< T... >
		>::value
	>::type
	set_value( Args&&... args )
	{
		set_value( std::forward_as_tuple( std::forward< Args >( args )... ) );
	}

	void set_exception( const std::exception_ptr& e )
	{
		promise_.set_value( ::q::refuse< tuple_type >( e ) );
		signal_->done( );
	}

	/**
	 * fn( args... ), set_value( void )
	 */
	template< typename Fn, typename... Args >
	typename std::enable_if<
		sizeof...( T ) == 0 &&
		Q_RESULT_OF_AS_ARGUMENT( Fn )::size::value == 0 &&
		Q_ARGUMENTS_ARE_CONVERTIBLE_FROM( Fn, Args... )::value
	>::type
	set_by_fun( Fn&& fn, Args&&... args )
	{
		try
		{
			::q::call_with_args(
				std::forward< Fn >( fn ),
				std::forward< Args >( args )...
			);
			set_value( std::tuple< >( ) );
		}
		catch ( ... )
		{
			set_exception( std::current_exception( ) );
		}
	}

	/**
	 * set_value( fn( args... ) )
	 */
	template< typename Fn, typename... Args >
	typename std::enable_if<
		( sizeof...( T ) > 0 ) &&
		Q_RESULT_OF_AS_ARGUMENT( Fn )::template equals<
			::q::arguments< T... >
		>::value &&
		Q_ARGUMENTS_ARE_CONVERTIBLE_FROM( Fn, Args... )::value
	>::type
	set_by_fun( Fn&& fn, Args&&... args )
	{
		try
		{
			set_value(
				::q::call_with_args(
					std::forward< Fn >( fn ),
					std::forward< Args >( args )...
				)
			);
		}
		catch ( ... )
		{
			set_exception( std::current_exception( ) );
		}
	}

	/**
	 * fn( tuple< args >... ), set_value( void )
	 */
	template< typename Fn, typename Args >
	typename std::enable_if<
		sizeof...( T ) == 0 &&
		Q_RESULT_OF_AS_ARGUMENT( Fn )::template equals<
			::q::arguments< T... >
		>::value &&
		::q::tuple_arguments< Args >::template is_convertible_to<
			Q_ARGUMENTS_OF( Fn )
		>::value
	>::type
	set_by_fun( Fn&& fn, Args&& args )
	{
		try
		{
			::q::call_with_args_by_tuple(
				std::forward< Fn >( fn ),
				std::forward< Args >( args )
			);
			set_value( std::tuple< >( ) );
		}
		catch ( ... )
		{
			set_exception( std::current_exception( ) );
		}
	}

	/**
	 * set_value( fn( tuple< args >... ) )
	 */
	template< typename Fn, typename Args >
	typename std::enable_if<
		( sizeof...( T ) > 0 ) &&
		Q_RESULT_OF_AS_ARGUMENT( Fn )::template equals<
			::q::arguments< T... >
		>::value &&
		::q::tuple_arguments< Args >::template is_convertible_to<
			Q_ARGUMENTS_OF( Fn )
		>::value
	>::type
	set_by_fun( Fn&& fn, Args&& args )
	{
		try
		{
			set_value(
				::q::call_with_args_by_tuple(
					std::forward< Fn >( fn ),
					std::forward< Args >( args )
				)
			);
		}
		catch ( ... )
		{
			set_exception( std::current_exception( ) );
		}
	}

	/**
	 * Sets the promise (i.e. value or exception) by a function.
	 *
	 * inner promise = fn( args... )
	 */
	template< typename Fn, typename... Args >
	typename std::enable_if<
		Q_ARITY_OF( Fn ) == sizeof...( Args ) &&
		(
			Q_ARITY_OF( Fn ) != 1
			||
			is_same_type<
				Q_FIRST_ARGUMENT_OF( Fn ),
				typename q::arguments< Args... >::first_type
			>::value
		)
	>::type
	satisfy_by_fun( Fn&& fn, Args&&... args )
	{
		try
		{
			satisfy(
				::q::call_with_args(
					std::forward< Fn >( fn ),
					std::forward< Args >( args )...
				)
			);
		}
		catch ( ... )
		{
			set_exception(
				std::make_exception_ptr(
					broken_promise_exception(
						std::current_exception( )
					)
				)
			);
		}
	}

	/**
	 * inner promise = fn( tuple< args >... )
	 */
	template< typename Fn, typename Args >
	typename std::enable_if<
		is_tuple< Args >::value &&
		Q_ARITY_OF( Fn ) == tuple_arguments< Args >::size::value
	>::type
	satisfy_by_fun( Fn&& fn, Args&& args )
	{
		try
		{
			satisfy(
				::q::call_with_args_by_tuple(
					std::forward< Fn >( fn ),
					std::forward< Args >( args )
				)
			);
		}
		catch ( ... )
		{
			set_exception(
				std::make_exception_ptr(
					broken_promise_exception(
						std::current_exception( )
					)
				)
			);
		}
	}

	void satisfy( promise_type&& promise )
	{
		auto _this = this->shared_from_this( );

		promise
		.fail( [ _this ]( std::exception_ptr&& e )
		{
			_this->set_exception( std::move( e ) );
		} )
		.then( [ _this ]( tuple_type&& tuple )
		{
			_this->set_value( std::move( tuple ) );
		} );

		/*
		 promise_ = std::move( promise );
		 signal_->done( );
		 /* */

	}

	void satisfy( shared_promise_type promise )
	{
		auto _this = this->shared_from_this( );

		promise
		.fail( [ _this ]( std::exception_ptr&& e )
		{
			_this->set_exception( std::move( e ) );
		} )
		.then( [ _this ]( const tuple_type& tuple )
		{
			_this->set_value( tuple );
		} );
	}

	/*
	 void satisfy( const shared_promise_type& promise )
	 {
	 promise_ = std::move( promise.privatize( ) );
	 signal_->done( );
	 }
	 */

	// Moves the promise out of the defer
	promise_type get_promise( )
	{
		return std::move( deferred_ );
	}

	template< typename Promise >
	typename std::enable_if<
		Promise::shared_type::value &&
		std::is_same<
			tuple_type,
			typename Promise::tuple_type
		>::value,
		Promise
	>::type
	get_suitable_promise( )
	{
		return get_promise( ).share( );
	}


	template< typename Promise >
	typename std::enable_if<
		!Promise::shared_type::value &&
		std::is_same<
			tuple_type,
			typename Promise::tuple_type
		>::value,
		Promise
	>::type
	get_suitable_promise( )
	{
		return get_promise( );
	}

	static std::shared_ptr< defer< T... > > construct( )
	{
		typedef typename state_data_type::future_type future_type;

		std::promise< expect_type > std_promise;
		future_type future = std_promise.get_future( );

		state_data_type state_data( std::move( future ) );
		state_type state( std::move( state_data ) );

		auto signal = state.signal( );

		promise_type q_promise( std::move( state ) );

		return ::q::make_shared_using_constructor< defer< T... > >(
			std::move( std_promise ),
			std::move( signal ),
			std::move( q_promise ) );
	}
	
protected:
	defer( ) = delete;
	
	defer( std::promise< expect_type >&& promise,
	       promise_signal_ptr&& signal,
	       promise_type&& deferred )
	: promise_( std::move( promise ) )
	, signal_( std::move( signal ) )
	, deferred_( std::move( deferred ) )
	{ }
	
private:
	std::promise< expect_type > promise_;
	promise_signal_ptr          signal_;
	
	promise_type                deferred_;
};

template< typename... T >
class defer< std::tuple< T... > >
: public defer< T... >
{ };

} // namespace detail

} // namespace q

#endif // LIBQ_PROMISE_DEFER_HPP
