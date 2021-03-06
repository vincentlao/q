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

#ifndef LIBQ_FUNCTIONAL_HPP
#define LIBQ_FUNCTIONAL_HPP

#include <iostream>
#include <q/type_traits.hpp>

// TODO: For C++14, alias the types to get rid of typename in the macros,
// since it requires the caller to know it's preceded with typename (hence
// cannot continue with ::size e.g.)

#define Q_FUNCTIONTRAITS( Fn ) \
	::q::function_traits< Fn >

#define Q_RESULT_OF( Fn ) \
	typename ::q::function_traits< Fn >::result_type

#define Q_RESULT_OF_AS_ARGUMENT( Fn ) \
	::q::function_traits< Fn >::result_argument_type

#define Q_RESULT_OF_AS_TUPLE_TYPE( Fn ) \
	typename Q_RESULT_OF_AS_ARGUMENT( Fn )::tuple_type

// TODO: Moving to C++14 will remove this
#define Q_RESULT_OF_AS_ARGUMENT_TYPE( Fn ) \
	typename Q_RESULT_OF_AS_ARGUMENT( Fn )

#define Q_ARITY_OF( Fn ) \
	::q::function_traits< Fn >::arity::value

#define Q_ARGUMENTS_OF( Fn ) \
	typename ::q::function_traits< Fn >::argument_types

#define Q_FIRST_ARGUMENT_OF( Fn ) \
	typename ::q::function_traits< Fn >::argument_types::first_type

#define Q_ARGUMENTS_ARE( Fn, ... ) \
	::q::is_argument_same< \
		Q_ARGUMENTS_OF( Fn ), \
		::q::arguments< __VA_ARGS__ > \
	>

#define Q_ARGUMENTS_ARE_CONVERTIBLE_FROM( Fn, ... ) \
	::q::is_argument_same_or_convertible< \
		::q::arguments< __VA_ARGS__ >, \
		Q_ARGUMENTS_OF( Fn ) \
	>

#define Q_MEMBERCLASS_OF( Fn ) \
	typename ::q::function_traits< Fn >::memberclass_type

#define Q_IS_MEMBERFUNCTION( Fn ) \
	::q::bool_type< !std::is_void< Q_MEMBERCLASS_OF( Fn ) >::value >


namespace q {

namespace detail {

struct valid_t
{
	typedef std::true_type valid;
};

struct invalid_t
{
	typedef std::false_type valid;
};

template< typename Signature >
struct fn_match
: public invalid_t
{ };

template< typename R, typename... A >
struct fn_match< R( & )( A... ) >
: public fn_match< R( A... ) >
{ };

template< typename R, typename... A >
struct fn_match< R( * )( A... ) >
: public fn_match< R( A... ) >
{ };

template< typename R, typename... A >
struct fn_match< R( A... ) >
: public valid_t
{
	typedef R result_type;
	typedef arguments< A... > argument_types;
	typedef R( signature )( A... );
	typedef R( *signature_ptr )( A... );
	typedef void memberclass_type;
	typedef void member_signature_ptr;
};

template< typename R, typename C, typename... A >
struct fn_match< R( C::* )( A... ) >
: public fn_match< R( C::* )( A... ) const >
{ };

template< typename R, typename C, typename... A >
struct fn_match< R( C::* )( A... ) const >
: public valid_t
{
	typedef R result_type;
	typedef arguments< A... > argument_types;
	typedef R( signature )( A... );
	typedef R( *signature_ptr )( A... );
	typedef C memberclass_type;
	typedef R( C::*member_signature_ptr )( A... );
};


template< typename Fn >
fn_match< Fn > fn_type( Fn&& );

template< typename Fn, bool Valid = fn_match< Fn >::valid::value >
struct identity
{
	typedef typename std::decay< Fn >::type decayed_type;
	typedef decltype( fn_type( std::declval< decayed_type >( ) ) ) match;
	typedef std::false_type using_call_operator;
};

template< typename Fn >
struct identity< Fn, false >
{
	typedef typename std::decay< Fn >::type decayed_type;
	typedef decltype( fn_type( &decayed_type::operator( ) ) ) match;
	typedef std::true_type using_call_operator;
};

template< typename Fn >
struct function_traits
{
	typedef Fn type;
	typedef identity< type > ident;
	typedef typename ident::match match;
	typedef typename ident::using_call_operator using_call_operator;
};

} // namespace detail

template< typename Fn >
struct function_traits
: public detail::function_traits< Fn >
{
	using typename detail::function_traits< Fn >::type;

	using typename detail::function_traits< Fn >::using_call_operator;

	// TODO: GCC Seems to not allow this using statement, which is why the
	// typedef is necessary.
	//using typename detail::function_traits< Fn >::match;
	typedef typename detail::function_traits< Fn >::match match;

	/**
	 * std::true_type or std::false_type depending on whether the type is
	 * understood to be a valid function or function-like type or not
	 */
	typedef typename match::valid                valid;

	/**
	 * The result type of the function. Same as @c std::result_type
	 */
	typedef typename match::result_type          result_type;

	/**
	 * The result type of the function capsulated in a q::arguments. This will
	 * treat a void return type (which really isn't a type) as an empty type
	 * list of the q::arguments. Hence this type becomes q::arguments<>.
	 */
	typedef typename std::conditional<
		std::is_same< result_type, void >::value,
		arguments< >,
		typename ::q::tuple_arguments< result_type >::this_type
	>::type                                      result_argument_type;

	/**
	 * A @c q::arguments type for the argument types
	 */
	typedef typename match::argument_types       argument_types;

	/**
	 * The type of the class for which this function is a member, for member
	 * functions, or void for non-member function types.
	 */
	typedef typename match::memberclass_type     memberclass_type;

	/**
	 * The plain function type on the form result_type( argument types... )
	 */
	typedef typename match::signature            signature;

	/**
	 * Function pointer type on the form result_type( * )( argument types... )
	 */
	typedef typename match::signature_ptr        signature_ptr;

	/**
	 * Member function pointer type on the form:
	 * result_type( memberclass_type::* )( argument types... )
	 */
	typedef typename match::member_signature_ptr member_signature_ptr;

	/**
	 * The arity of the function, i.e. the number of arguments it expects
	 */
	typedef typename argument_types::size        arity;
};


template< typename Fn, typename... Args >
typename std::enable_if<
	!Q_IS_MEMBERFUNCTION( Fn )::value ||
	Q_FUNCTIONTRAITS( Fn )::using_call_operator::value,
	Q_RESULT_OF( Fn )
>::type
call_with_args( Fn&& fn, Args&&... args )
{
	return fn( std::forward< Args >( args )... );
}

template< typename Fn, typename Class, typename... Args >
typename std::enable_if<
	Q_IS_MEMBERFUNCTION( Fn )::value &&
	(
		std::is_same<
			Q_MEMBERCLASS_OF( Fn )*,
			typename std::decay< Class >::type
		>::value ||
		::q::is_base_of<
			Q_MEMBERCLASS_OF( Fn )*,
			typename std::decay< Class >::type
		>::value
	) &&
	is_pointer_like< typename std::decay< Class >::type >::value,
	Q_RESULT_OF( Fn )
>::type
call_with_args( Fn&& fn, Class&& obj, Args&&... args )
{
	return ( obj->*fn )( std::forward< Args >( args )... );
}

template< typename Fn, typename Class, typename... Args >
typename std::enable_if<
	Q_IS_MEMBERFUNCTION( Fn )::value &&
	(
		std::is_same<
			Q_MEMBERCLASS_OF( Fn ),
			typename std::decay< Class >::type
		>::value ||
		::q::is_base_of<
			Q_MEMBERCLASS_OF( Fn ),
			typename std::decay< Class >::type
		>::value
	) &&
	!is_pointer_like< typename std::decay< Class >::type >::value,
	Q_RESULT_OF( Fn )
>::type
call_with_args( Fn&& fn, Class&& obj, Args&&... args )
{
	return ( obj.*fn )( std::forward< Args >( args )... );
}

namespace detail {

template< typename Fn, class Tuple, std::size_t... Indexes >
Q_RESULT_OF( Fn )
call_with_args_by_tuple( Fn&& fn, Tuple&& tuple, q::index_tuple< Indexes... > )
{
	return call_with_args(
		std::forward< Fn >( fn ),
		std::forward<
			typename std::tuple_element< Indexes, Tuple >::type
		>( std::get< Indexes >( tuple ) )... );
}

} // namespace detail

template< typename Fn, class Tuple >
Q_RESULT_OF( Fn )
call_with_args_by_tuple( Fn&& fn, Tuple&& tuple )
{
	typedef q::tuple_arguments< Tuple > arguments;
	typedef typename q::make_index_tuple< arguments::size::value >::type indexes;
	return detail::call_with_args_by_tuple(
		std::forward< Fn >( fn ),
		std::forward< Tuple >( tuple ), indexes( ) );
}

template< typename Fn, typename InnerFn, typename... Args >
typename std::enable_if<
	Q_RESULT_OF_AS_ARGUMENT( InnerFn )::size::value == 0 &&
	Q_ARITY_OF( Fn ) == 0 &&
	Q_ARGUMENTS_ARE_CONVERTIBLE_FROM( InnerFn, Args... )::value,
	Q_RESULT_OF( Fn )
>::type
call_with_args_by_fun( Fn&& fn, InnerFn&& inner_fn, Args&&... args )
{
	inner_fn( std::forward< Args >( args )... );
	return fn( );
}

template< typename Fn, typename InnerFn, typename... Args >
typename std::enable_if<
	( Q_RESULT_OF_AS_ARGUMENT( InnerFn )::size::value > 0 ) &&
	::q::is_argument_same_or_convertible<
		Q_RESULT_OF_AS_ARGUMENT_TYPE( InnerFn ),
		Q_ARGUMENTS_OF( Fn )
	>::value &&
	Q_ARGUMENTS_ARE_CONVERTIBLE_FROM( InnerFn, Args... )::value,
	Q_RESULT_OF( Fn )
>::type
call_with_args_by_fun( Fn&& fn, InnerFn&& inner_fn, Args&&... args )
{
	return fn( inner_fn( std::forward< Args >( args )... ) );
}

template< typename Class, typename Fn, typename... Args >
typename std::enable_if<
	Q_RESULT_OF_AS_ARGUMENT( Fn )::size::value == 0 &&
	std::is_default_constructible< Class >::value,
	Class
>::type
construct_with_function_call( Fn&& fn, Args&&... args )
{
	fn( std::forward< Args >( args )... );
	return Class( );
}

template< typename Class, typename Fn, typename... Args >
typename std::enable_if<
	( Q_RESULT_OF_AS_ARGUMENT( Fn )::size::value > 0 ) &&
	merge<
		std::is_constructible,
		q::arguments< Class >,
		Q_RESULT_OF_AS_ARGUMENT_TYPE( Fn )
	>::type::value,
	Class
>::type
construct_with_function_call( Fn&& fn, Args&&... args )
{
	return Class( fn( std::forward< Args >( args )... ) );
}

/**
 * Automatically deduces the types of the @a args and tries to call @a fn with
 * these arguments, or if @a args is a tuple or @c q::arguments instance,
 * tries to unpack it so that it matches the arguments to @a fn.
 *
 * In other words, a function void( int, double ) can be invoked with @a args
 * being either ( int, double ) or an instance of std::tuple< int, double > or
 * q::arguments< int, double >.
 */
template< typename Fn, typename... Args >
Q_RESULT_OF( Fn )
call( Fn&& fn, Args&&... args )
{
	// TODO: Implement
}

} // namespace q

#endif // LIBQ_FUNCTIONAL_HPP
