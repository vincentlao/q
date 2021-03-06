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

#include <q/queue.hpp>
#include <q/mutex.hpp>
#include <q/memory.hpp>
#include <q/exception.hpp>

#include <queue>
#include <atomic>

// TODO: REMOVE
#include <iostream>

namespace q {

mutex queue_mutex_;
queue_ptr main_queue_;
queue_ptr background_queue_;
queue_ptr default_queue_;
/*
std::atomic< queue_ptr > main_queue_;
std::atomic< queue_ptr > background_queue_;
std::atomic< queue_ptr > default_queue_;
*/

queue_ptr main_queue( )
{
	Q_AUTO_UNIQUE_LOCK( queue_mutex_, Q_HERE );
	return main_queue_;
}
queue_ptr background_queue( )
{
	Q_AUTO_UNIQUE_LOCK( queue_mutex_, Q_HERE );
	return background_queue_;
}
queue_ptr default_queue( )
{
	Q_AUTO_UNIQUE_LOCK( queue_mutex_, Q_HERE );
	return default_queue_;
}

queue_ptr set_main_queue( queue_ptr queue )
{
	queue_ptr old;
	{
		Q_AUTO_UNIQUE_LOCK( queue_mutex_, Q_HERE );
		old = main_queue_;
		main_queue_ = queue;
	}
	return old;

	/* TODO: why doesn't this work?
	return std::atomic_exchange(
		&main_queue_,
		queue );
	*/
}
queue_ptr set_background_queue( queue_ptr queue )
{
	queue_ptr old;
	{
		Q_AUTO_UNIQUE_LOCK( queue_mutex_, Q_HERE );
		old = background_queue_;
		background_queue_ = queue;
	}
	return old;
}
queue_ptr set_default_queue( queue_ptr queue )
{
	queue_ptr old;
	{
		Q_AUTO_UNIQUE_LOCK( queue_mutex_, Q_HERE );
		old = default_queue_;
		default_queue_ = queue;
	}
	return old;
}


// TODO: Consider using a bemaphore instead, and then preferably a non-locking
// queue altogether. The only thing necessary is that two push-calls from the
// same thread must follow order.
struct queue::pimpl
{
	pimpl( priority_t priority )
	: priority_( priority )
	, mutex_( Q_HERE, "queue mutex" )
	{ }

	const priority_t priority_;
	mutex mutex_;
	queue::notify_type notify_;
	std::queue< task > queue_;
};

queue_ptr queue::make( priority_t priority )
{
	return make_shared< queue >( priority );
}

queue::queue( priority_t priority )
: pimpl_( new pimpl( priority ) )
{
}

queue::~queue( )
{
}

void queue::push( task&& task )
{
	notify_type notifyer;
	std::size_t size;

	{
		Q_AUTO_UNIQUE_LOCK( pimpl_->mutex_, Q_HERE, "queue::push" );

		pimpl_->queue_.push( std::move( task ) );

		notifyer = pimpl_->notify_;
		size = pimpl_->queue_.size( );
	}

	if ( notifyer )
		notifyer( size );
}

priority_t queue::priority( ) const
{
	return pimpl_->priority_;
}

std::size_t queue::set_consumer( queue::notify_type fn )
{
	Q_AUTO_UNIQUE_LOCK( pimpl_->mutex_, Q_HERE, "queue::set_consumer" );

	std::size_t backlog = pimpl_->queue_.size( );
	pimpl_->notify_ = fn;

	return backlog;
}

bool queue::empty( )
{
	return pimpl_->queue_.empty( );
}

task queue::pop( )
{
	Q_AUTO_UNIQUE_LOCK( pimpl_->mutex_, Q_HERE, "queue::pop" );

	if ( pimpl_->queue_.empty( ) )
	{
		/* TODO: , "queue::pop: Queue empty" */
		Q_THROW( queue_exception( ) );
	}

	task task = std::move( pimpl_->queue_.front( ) );

	pimpl_->queue_.pop( );

	return std::move( task );
}

} // namespace q
