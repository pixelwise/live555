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
// Implementation


#include "BasicUsageEnvironment.hh"
#include "HandlerSet.hh"
#include <stdio.h>
#if defined(_QNX4)
#include <sys/select.h>
#include <unix.h>
#endif

#include <algorithm>

////////// BasicTaskScheduler //////////

BasicTaskScheduler* BasicTaskScheduler::createNew(unsigned maxSchedulerGranularity) {
        return new BasicTaskScheduler(maxSchedulerGranularity);
}

BasicTaskScheduler::BasicTaskScheduler(unsigned maxSchedulerGranularity)
    : fMaxSchedulerGranularity(maxSchedulerGranularity), fMaxNumSockets(0)
#if defined(__WIN32__) || defined(_WIN32)
    , fDummySocketNum(-1)
#endif
{
    if (maxSchedulerGranularity > 0)
        schedulerTickTask(); // ensures that we handle events frequently
}

BasicTaskScheduler::~BasicTaskScheduler() {
#if defined(__WIN32__) || defined(_WIN32)
    if (fDummySocketNum >= 0) closeSocket(fDummySocketNum);
#endif
}

void BasicTaskScheduler::schedulerTickTask(void* clientData) {
    ((BasicTaskScheduler*)clientData)->schedulerTickTask();
}

void BasicTaskScheduler::schedulerTickTask() {
    scheduleDelayedTask(fMaxSchedulerGranularity, schedulerTickTask, this);
}

#ifndef MILLION
#define MILLION 1000000
#endif
void BasicTaskScheduler::SingleStep(unsigned maxDelayTimeMicros)
{
    DelayInterval maxDelayTime(maxDelayTimeMicros / MILLION, maxDelayTimeMicros % MILLION);
    auto select_result = perform_select(get_select_wait_time(maxDelayTime));
    if (auto handler = find_handler(pop_next_relevant_handler(), select_result))
        perform_handler(handler, select_result);
    // Note that we do this *after* calling a socket handler,
    // in case the triggered event handler modifies The set of readable sockets.
    perform_triggers();
    // Also handle any delayed event that may have come due.
    fDelayQueue.handleAlarm();
}



auto BasicTaskScheduler::perform_select(DelayInterval maxWaitTime) const -> socket_sets_t
{
    auto socket_sets = fSocketSets;
    auto tv_timeToDelay = maxWaitTime.value();
    int selectResult = select(
        fMaxNumSockets,
        &socket_sets.read,
        &socket_sets.write,
        &socket_sets.exceptions,
        &tv_timeToDelay
    );
    if (selectResult < 0)
    {
#if defined(__WIN32__) || defined(_WIN32)
        int err = WSAGetLastError();
        // For some unknown reason, select() in Windoze sometimes fails with WSAEINVAL if
        // it was called with no entries set in "readSet".  If this happens, ignore it:
        if (err == WSAEINVAL && readSet.fd_count == 0)
        {
            err = EINTR;
            // To stop this from happening again, create a dummy socket:
            if (fDummySocketNum >= 0)
                closeSocket(fDummySocketNum);
            fDummySocketNum = socket(AF_INET, SOCK_DGRAM, 0);
            FD_SET((unsigned)fDummySocketNum, &fReadSet);
        }
        if (err != EINTR)
#else
        if (errno != EINTR && errno != EAGAIN)
#endif
        // Unexpected error - treat this as fatal:
#if !defined(_WIN32_WCE)
        {
            perror("BasicTaskScheduler::SingleStep(): select() fails");
            // Because this failure is often "Bad file descriptor" - which is caused by an invalid socket number (i.e., a socket number
            // that had already been closed) being used in "select()" - we print out the sockets that were being used in "select()",
            // to assist in debugging:
            fprintf(stderr, "socket numbers used in the select() call:");
            for (int i = 0; i < 10000; ++i) {
                if (fSocketSets.has_socket(i))
                {
                    fprintf(stderr, " %d(", i);
                    if (fSocketSets.has_read_socket(i))
                        fprintf(stderr, "r");
                    if (fSocketSets.has_write_socket(i))
                        fprintf(stderr, "w");
                    if (fSocketSets.has_exception_socket(i))
                        fprintf(stderr, "e");
                    fprintf(stderr, ")");
                }
            }
            fprintf(stderr, "\n");
#endif
            internalError();
        }
    }
    return socket_sets;
}

DelayInterval BasicTaskScheduler::get_select_wait_time(DelayInterval maxDelayTime) const
{
    return std::min(
        fDelayQueue.timeToNextAlarm(),
        std::min(
            DelayInterval(MILLION, 0),
            maxDelayTime
        )
    );
}

void BasicTaskScheduler::perform_triggers()
{
    if (fTriggersAwaitingHandling != 0)
    {
        if (fTriggersAwaitingHandling == fLastUsedTriggerMask)
        {
            // Common-case optimization for a single event trigger:
            fTriggersAwaitingHandling &=~ fLastUsedTriggerMask;
            if (fTriggeredEventHandlers[fLastUsedTriggerNum] != NULL)
            {
                (*fTriggeredEventHandlers[fLastUsedTriggerNum])(fTriggeredEventClientDatas[fLastUsedTriggerNum]);
            }
        }
        else
        {
            // Look for an event trigger that needs handling (making sure that we make forward progress through all possible triggers):
            unsigned i = fLastUsedTriggerNum;
            EventTriggerId mask = fLastUsedTriggerMask;
            do 
            {
                i = (i+1)%MAX_NUM_EVENT_TRIGGERS;
                mask >>= 1;
                if (mask == 0)
                    mask = 0x80000000;
                if ((fTriggersAwaitingHandling&mask) != 0)
                {
                    fTriggersAwaitingHandling &=~ mask;
                    if (fTriggeredEventHandlers[i] != NULL)
                    {
                        (*fTriggeredEventHandlers[i])(fTriggeredEventClientDatas[i]);
                    }
                    fLastUsedTriggerMask = mask;
                    fLastUsedTriggerNum = i;
                    break;
                }
            }
            while (i != fLastUsedTriggerNum);
        }
    }
}

auto BasicTaskScheduler::pop_next_relevant_handler() -> HandlerIterator
{
    // To ensure forward progress through the handlers, begin past the last
    // socket number that we handled:
    HandlerIterator iter(*fHandlers);
    if (fLastHandledSocketNum >= 0)
    {
        HandlerDescriptor* handler;
        while ((handler = iter.next()) != NULL)
        {
            if (handler->socketNum == fLastHandledSocketNum)
                break;
        }
        if (handler == NULL)
        {
            fLastHandledSocketNum = -1;
            iter.reset(); // start from the beginning instead
        }
    }
    return iter;
}

HandlerDescriptor* BasicTaskScheduler::find_handler(HandlerIterator iter, socket_sets_t select_result)
{
    HandlerDescriptor* handler;
    while ((handler = iter.next()) != NULL)
    {
        int resultConditionSet = select_result.condition_set(handler->socketNum);
        if (
            (resultConditionSet & handler->conditionSet) != 0 &&
            handler->handlerProc != NULL
        )
          return handler;
    }

    if (fLastHandledSocketNum >= 0)
    {
        // We didn't call a handler, but we didn't get to check all of them,
        // so try again from the beginning:
        HandlerIterator iter(*fHandlers);
        while ((handler = iter.next()) != NULL)
        {
            int resultConditionSet = select_result.condition_set(handler->socketNum);
            if (
                (resultConditionSet & handler->conditionSet) != 0 &&
                handler->handlerProc != NULL
            )
              return handler;
        }
    }  
    fLastHandledSocketNum = -1;//because we didn't call a handler
    return NULL;
}

void BasicTaskScheduler::perform_handler(HandlerDescriptor* handler, socket_sets_t select_result)
{
    // Note: we set "fLastHandledSocketNum" before calling the handler,
    // in case the handler calls "doEventLoop()" reentrantly.
    fLastHandledSocketNum = handler->socketNum;
    (*handler->handlerProc)(handler->clientData, select_result.condition_set(handler->socketNum));    
}

void BasicTaskScheduler::setBackgroundHandling(
    int socketNum,
    int conditionSet,
    BackgroundHandlerProc* handlerProc,
    void* clientData
)
{
    if (socketNum < 0)
        return;
#if !defined(__WIN32__) && !defined(_WIN32) && defined(FD_SETSIZE)
    if (socketNum >= (int)(FD_SETSIZE))
        return;
#endif
    fSocketSets.set_socket_conditions(socketNum, conditionSet);
    if (conditionSet == 0)
    {
        fHandlers->clearHandler(socketNum);
        if (socketNum + 1 == fMaxNumSockets)
            --fMaxNumSockets;
    }
    else
    {
        fHandlers->assignHandler(socketNum, conditionSet, handlerProc, clientData);
        if (socketNum + 1 > fMaxNumSockets)
            fMaxNumSockets = socketNum + 1;
    }
}

void BasicTaskScheduler::moveSocketHandling(int oldSocketNum, int newSocketNum)
{
    if (oldSocketNum < 0 || newSocketNum < 0)
        return;
#if !defined(__WIN32__) && !defined(_WIN32) && defined(FD_SETSIZE)
    if (oldSocketNum >= (int)(FD_SETSIZE) || newSocketNum >= (int)(FD_SETSIZE))
        return;
#endif
    fSocketSets.move_socket(oldSocketNum, newSocketNum);
    fHandlers->moveHandler(oldSocketNum, newSocketNum);
    if (oldSocketNum + 1 == fMaxNumSockets)
        --fMaxNumSockets;
    if (newSocketNum + 1 > fMaxNumSockets)
        fMaxNumSockets = newSocketNum + 1;
}
