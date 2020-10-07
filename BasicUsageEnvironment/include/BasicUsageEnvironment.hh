/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// Basic Usage Environment: for a simple, non-scripted, console application
// C++ header

#ifndef _BASIC_USAGE_ENVIRONMENT_HH
#define _BASIC_USAGE_ENVIRONMENT_HH

#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
#include "BasicUsageEnvironment0.hh"
#endif

#include "HandlerSet.hh"

class BasicUsageEnvironment: public BasicUsageEnvironment0 {
public:
  static BasicUsageEnvironment* createNew(TaskScheduler& taskScheduler);
  virtual ~BasicUsageEnvironment();

  // redefined virtual functions:
  virtual int getErrno() const;

  virtual UsageEnvironment& operator<<(char const* str);
  virtual UsageEnvironment& operator<<(int i);
  virtual UsageEnvironment& operator<<(unsigned u);
  virtual UsageEnvironment& operator<<(double d);
  virtual UsageEnvironment& operator<<(void* p);

protected:
  BasicUsageEnvironment(TaskScheduler& taskScheduler);
      // called only by "createNew()" (or subclass constructors)
};


class BasicTaskScheduler: public BasicTaskScheduler0 {
public:
  static BasicTaskScheduler* createNew(unsigned maxSchedulerGranularity = 10000/*microseconds*/);
    // "maxSchedulerGranularity" (default value: 10 ms) specifies the maximum time that we wait (in "select()") before
    // returning to the event loop to handle non-socket or non-timer-based events, such as 'triggered events'.
    // You can change this is you wish (but only if you know what you're doing!), or set it to 0, to specify no such maximum time.
    // (You should set it to 0 only if you know that you will not be using 'event triggers'.)
  virtual ~BasicTaskScheduler();

protected:
  BasicTaskScheduler(unsigned maxSchedulerGranularity);
      // called only by "createNew()"

  static void schedulerTickTask(void* clientData);
  void schedulerTickTask();

protected:
  // Redefined virtual functions:
  virtual void SingleStep(unsigned maxDelayTime);

  virtual void setBackgroundHandling(int socketNum, int conditionSet, BackgroundHandlerProc* handlerProc, void* clientData);
  virtual void moveSocketHandling(int oldSocketNum, int newSocketNum);

protected:
  unsigned fMaxSchedulerGranularity;

private:

  struct socket_sets_t
  {
    socket_sets_t()
    {
      FD_ZERO(&read);
      FD_ZERO(&write);
      FD_ZERO(&exceptions);
    }
    fd_set read;
    fd_set write;
    fd_set exceptions;
    bool has_read_socket(int sock) const
    {
      return FD_ISSET(sock, &read);
    }
    bool has_write_socket(int sock) const
    {
      return FD_ISSET(sock, &write);
    }
    bool has_exception_socket(int sock) const
    {
      return FD_ISSET(sock, &exceptions);
    }
    bool has_socket(int sock) const
    {
      return has_read_socket(sock) || has_write_socket(sock) || has_exception_socket(sock);
    }
    int condition_set(int sock) const
    {
      int resultConditionSet = 0;
      if (has_read_socket(sock))
        resultConditionSet |= SOCKET_READABLE;
      if (has_write_socket(sock))
        resultConditionSet |= SOCKET_WRITABLE;
      if (has_exception_socket(sock))
        resultConditionSet |= SOCKET_EXCEPTION;
      return resultConditionSet;      
    }
    void set_socket_conditions(int sock, int conditions)
    {
      read = set_socket_value(read, sock, conditions & SOCKET_READABLE);
      write = set_socket_value(write, sock, conditions & SOCKET_WRITABLE);
      exceptions = set_socket_value(exceptions, sock, conditions & SOCKET_EXCEPTION);
    }
    static fd_set set_socket_value(fd_set set, int sock, bool value)
    {
      if (value)
        FD_SET(sock, &set);
      else
        FD_CLR(sock, &set);
      return set;
    }
    static fd_set move_socket(fd_set set, int from_sock, int to_sock)
    {
      if (FD_ISSET(from_sock, &set))
      {
        set = set_socket_value(set, to_sock, true);
        set = set_socket_value(set, from_sock, false);
      }
      return set;
    }
    void move_socket(int from_sock, int to_sock)
    {
      read = move_socket(read, from_sock, to_sock);
      write = move_socket(write, from_sock, to_sock);
      exceptions = move_socket(exceptions, from_sock, to_sock);
    }
    void remove_socket(int sock)
    {
      read = set_socket_value(read, sock, false);
      write = set_socket_value(write, sock, false);
      exceptions = set_socket_value(exceptions, sock, false);
    }
  };

  // To implement background operations:
  int fMaxNumSockets;
  socket_sets_t fSocketSets;

  socket_sets_t perform_select(DelayInterval maxWaitTime) const;
  DelayInterval get_select_wait_time(DelayInterval maxDelayTime) const;
  HandlerIterator pop_next_relevant_handler();
  HandlerDescriptor* find_handler(HandlerIterator iter, socket_sets_t select_result);
  void perform_handler(HandlerDescriptor* handler, socket_sets_t select_result);
  void perform_triggers();

#if defined(__WIN32__) || defined(_WIN32)
  // Hack to work around a bug in Windows' "select()" implementation:
  mutable int fDummySocketNum;
#endif
};

#endif
