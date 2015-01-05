// <shared_mutex> -*- C++ -*-

// Copyright (C) 2013-2014 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file include/shared_mutex
 *  This is a Standard C++ Library header.
 */

#ifndef _GLIBCXX_SHARED_MUTEX
#define _GLIBCXX_SHARED_MUTEX 1

#pragma GCC system_header

#if __cplusplus <= 201103L
# include <bits/c++14_warning.h>
#else

#include <bits/c++config.h>
#include <mutex>
#include <condition_variable>
#include <bits/functexcept.h>

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  /**
   * @ingroup mutexes
   * @{
   */

#ifdef _GLIBCXX_USE_C99_STDINT_TR1
#ifdef _GLIBCXX_HAS_GTHREADS

#define __cpp_lib_shared_timed_mutex 201402

  /// shared_timed_mutex
  class shared_timed_mutex
  {
#if _GTHREAD_USE_MUTEX_TIMEDLOCK
    struct _Mutex : mutex, __timed_mutex_impl<_Mutex>
    {
      template<typename _Rep, typename _Period>
	bool
	try_lock_for(const chrono::duration<_Rep, _Period>& __rtime)
	{ return _M_try_lock_for(__rtime); }

      template<typename _Clock, typename _Duration>
	bool
	try_lock_until(const chrono::time_point<_Clock, _Duration>& __atime)
	{ return _M_try_lock_until(__atime); }
    };
#else
    typedef mutex _Mutex;
#endif

    // Based on Howard Hinnant's reference implementation from N2406

    _Mutex		_M_mut;
    condition_variable	_M_gate1;
    condition_variable	_M_gate2;
    unsigned		_M_state;

    static constexpr unsigned _S_write_entered = 1U << (sizeof(unsigned)*__CHAR_BIT__ - 1);
    static constexpr unsigned _M_n_readers = ~_S_write_entered;

  public:
    shared_timed_mutex() : _M_state(0) {}

    ~shared_timed_mutex()
    {
      _GLIBCXX_DEBUG_ASSERT( _M_state == 0 );
    }

    shared_timed_mutex(const shared_timed_mutex&) = delete;
    shared_timed_mutex& operator=(const shared_timed_mutex&) = delete;

    // Exclusive ownership

    void
    lock()
    {
      unique_lock<mutex> __lk(_M_mut);
      while (_M_state & _S_write_entered)
	_M_gate1.wait(__lk);
      _M_state |= _S_write_entered;
      while (_M_state & _M_n_readers)
	_M_gate2.wait(__lk);
    }

    bool
    try_lock()
    {
      unique_lock<mutex> __lk(_M_mut, try_to_lock);
      if (__lk.owns_lock() && _M_state == 0)
	{
	  _M_state = _S_write_entered;
	  return true;
	}
      return false;
    }

#if _GTHREAD_USE_MUTEX_TIMEDLOCK
    template<typename _Rep, typename _Period>
      bool
      try_lock_for(const chrono::duration<_Rep, _Period>& __rel_time)
      {
	unique_lock<_Mutex> __lk(_M_mut, __rel_time);
	if (__lk.owns_lock() && _M_state == 0)
	  {
	    _M_state = _S_write_entered;
	    return true;
	  }
	return false;
      }

    template<typename _Clock, typename _Duration>
      bool
      try_lock_until(const chrono::time_point<_Clock, _Duration>& __abs_time)
      {
	unique_lock<_Mutex> __lk(_M_mut, __abs_time);
	if (__lk.owns_lock() && _M_state == 0)
	  {
	    _M_state = _S_write_entered;
	    return true;
	  }
	return false;
      }
#endif

    void
    unlock()
    {
      {
	lock_guard<_Mutex> __lk(_M_mut);
	_M_state = 0;
      }
      _M_gate1.notify_all();
    }

    // Shared ownership

    void
    lock_shared()
    {
      unique_lock<mutex> __lk(_M_mut);
      while ((_M_state & _S_write_entered)
	  || (_M_state & _M_n_readers) == _M_n_readers)
	{
	  _M_gate1.wait(__lk);
	}
      unsigned __num_readers = (_M_state & _M_n_readers) + 1;
      _M_state &= ~_M_n_readers;
      _M_state |= __num_readers;
    }

    bool
    try_lock_shared()
    {
      unique_lock<_Mutex> __lk(_M_mut, try_to_lock);
      unsigned __num_readers = _M_state & _M_n_readers;
      if (__lk.owns_lock() && !(_M_state & _S_write_entered)
	  && __num_readers != _M_n_readers)
	{
	  ++__num_readers;
	  _M_state &= ~_M_n_readers;
	  _M_state |= __num_readers;
	  return true;
	}
      return false;
    }

#if _GTHREAD_USE_MUTEX_TIMEDLOCK
    template<typename _Rep, typename _Period>
      bool
      try_lock_shared_for(const chrono::duration<_Rep, _Period>& __rel_time)
      {
	unique_lock<_Mutex> __lk(_M_mut, __rel_time);
	if (__lk.owns_lock())
	  {
	    unsigned __num_readers = _M_state & _M_n_readers;
	    if (!(_M_state & _S_write_entered)
		&& __num_readers != _M_n_readers)
	      {
		++__num_readers;
		_M_state &= ~_M_n_readers;
		_M_state |= __num_readers;
		return true;
	      }
	  }
	return false;
      }

    template <typename _Clock, typename _Duration>
      bool
      try_lock_shared_until(const chrono::time_point<_Clock,
						     _Duration>& __abs_time)
      {
	unique_lock<_Mutex> __lk(_M_mut, __abs_time);
	if (__lk.owns_lock())
	  {
	    unsigned __num_readers = _M_state & _M_n_readers;
	    if (!(_M_state & _S_write_entered)
		&& __num_readers != _M_n_readers)
	      {
		++__num_readers;
		_M_state &= ~_M_n_readers;
		_M_state |= __num_readers;
		return true;
	      }
	  }
	return false;
      }
#endif

    void
    unlock_shared()
    {
      lock_guard<_Mutex> __lk(_M_mut);
      unsigned __num_readers = (_M_state & _M_n_readers) - 1;
      _M_state &= ~_M_n_readers;
      _M_state |= __num_readers;
      if (_M_state & _S_write_entered)
	{
	  if (__num_readers == 0)
	    _M_gate2.notify_one();
	}
      else
	{
	  if (__num_readers == _M_n_readers - 1)
	    _M_gate1.notify_one();
	}
    }
  };
#endif // _GLIBCXX_HAS_GTHREADS

  /// shared_lock
  template<typename _Mutex>
    class shared_lock
    {
    public:
      typedef _Mutex mutex_type;

      // Shared locking

      shared_lock() noexcept : _M_pm(nullptr), _M_owns(false) { }

      explicit
      shared_lock(mutex_type& __m) : _M_pm(&__m), _M_owns(true)
      { __m.lock_shared(); }

      shared_lock(mutex_type& __m, defer_lock_t) noexcept
      : _M_pm(&__m), _M_owns(false) { }

      shared_lock(mutex_type& __m, try_to_lock_t)
      : _M_pm(&__m), _M_owns(__m.try_lock_shared()) { }

      shared_lock(mutex_type& __m, adopt_lock_t)
      : _M_pm(&__m), _M_owns(true) { }

      template<typename _Clock, typename _Duration>
	shared_lock(mutex_type& __m,
		    const chrono::time_point<_Clock, _Duration>& __abs_time)
      : _M_pm(&__m), _M_owns(__m.try_lock_shared_until(__abs_time)) { }

      template<typename _Rep, typename _Period>
	shared_lock(mutex_type& __m,
		    const chrono::duration<_Rep, _Period>& __rel_time)
      : _M_pm(&__m), _M_owns(__m.try_lock_shared_for(__rel_time)) { }

      ~shared_lock()
      {
	if (_M_owns)
	  _M_pm->unlock_shared();
      }

      shared_lock(shared_lock const&) = delete;
      shared_lock& operator=(shared_lock const&) = delete;

      shared_lock(shared_lock&& __sl) noexcept : shared_lock()
      { swap(__sl); }

      shared_lock&
      operator=(shared_lock&& __sl) noexcept
      {
	shared_lock(std::move(__sl)).swap(*this);
	return *this;
      }

      void
      lock()
      {
	_M_lockable();
	_M_pm->lock_shared();
	_M_owns = true;
      }

      bool
      try_lock()
      {
	_M_lockable();
	return _M_owns = _M_pm->try_lock_shared();
      }

      template<typename _Rep, typename _Period>
	bool
	try_lock_for(const chrono::duration<_Rep, _Period>& __rel_time)
	{
	  _M_lockable();
	  return _M_owns = _M_pm->try_lock_shared_for(__rel_time);
	}

      template<typename _Clock, typename _Duration>
	bool
	try_lock_until(const chrono::time_point<_Clock, _Duration>& __abs_time)
	{
	  _M_lockable();
	  return _M_owns = _M_pm->try_lock_shared_until(__abs_time);
	}

      void
      unlock()
      {
	if (!_M_owns)
	  __throw_system_error(int(errc::resource_deadlock_would_occur));
	_M_pm->unlock_shared();
	_M_owns = false;
      }

      // Setters

      void
      swap(shared_lock& __u) noexcept
      {
	std::swap(_M_pm, __u._M_pm);
	std::swap(_M_owns, __u._M_owns);
      }

      mutex_type*
      release() noexcept
      {
	_M_owns = false;
	return std::exchange(_M_pm, nullptr);
      }

      // Getters

      bool owns_lock() const noexcept { return _M_owns; }

      explicit operator bool() const noexcept { return _M_owns; }

      mutex_type* mutex() const noexcept { return _M_pm; }

    private:
      void
      _M_lockable() const
      {
	if (_M_pm == nullptr)
	  __throw_system_error(int(errc::operation_not_permitted));
	if (_M_owns)
	  __throw_system_error(int(errc::resource_deadlock_would_occur));
      }

      mutex_type*	_M_pm;
      bool		_M_owns;
    };

  /// Swap specialization for shared_lock
  template<typename _Mutex>
    void
    swap(shared_lock<_Mutex>& __x, shared_lock<_Mutex>& __y) noexcept
    { __x.swap(__y); }

#endif // _GLIBCXX_USE_C99_STDINT_TR1

  // @} group mutexes
_GLIBCXX_END_NAMESPACE_VERSION
} // namespace

#endif // C++14

#endif // _GLIBCXX_SHARED_MUTEX
