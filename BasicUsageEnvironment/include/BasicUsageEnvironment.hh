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
  // To implement background operations:
  int fMaxNumSockets;
  fd_set fReadSet;
  fd_set fWriteSet;
  fd_set fExceptionSet;

private:
  struct select_result_t
  {
    fd_set read;
    fd_set write;
    fd_set exceptions;
    int condition_set(int sock) const
    {
      int resultConditionSet = 0;
      if (FD_ISSET(sock, &read))
        resultConditionSet |= SOCKET_READABLE;
      if (FD_ISSET(sock, &write))
        resultConditionSet |= SOCKET_WRITABLE;
      if (FD_ISSET(sock, &exceptions))
        resultConditionSet |= SOCKET_EXCEPTION;
      return resultConditionSet;      
    }
  };
  select_result_t perform_select(struct timeval) const;
  struct timeval get_select_wait_time(unsigned maxDelayTime);
  HandlerIterator pop_next_relevant_handler();
  void perform_handler(HandlerIterator iter, select_result_t select_result);
  void handle_newly_triggered_events();

#if defined(__WIN32__) || defined(_WIN32)
  // Hack to work around a bug in Windows' "select()" implementation:
  mutable int fDummySocketNum;
#endif
};

#endif
