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
// "liveMedia"
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// RTP Sources
// C++ header

#ifndef _RTP_SOURCE_HH
#define _RTP_SOURCE_HH

#include "RTPReceptionStats.hh"
#include "FramedSource.hh"
#include "RTPInterface.hh"

class RTPReceptionStatsDB; // forward

class RTPSource: public FramedSource {
public:
  static Boolean lookupByName(UsageEnvironment& env, char const* sourceName,
            RTPSource*& resultSource);

  Boolean curPacketMarkerBit() const { return fCurPacketMarkerBit; }

  unsigned char rtpPayloadFormat() const { return fRTPPayloadFormat; }

  virtual Boolean hasBeenSynchronizedUsingRTCP();

  Groupsock* RTPgs() const { return fRTPInterface.gs(); }

  virtual void setPacketReorderingThresholdTime(unsigned uSeconds) = 0;

  // used by RTCP:
  u_int32_t SSRC() const { return fSSRC; }
      // Note: This is *our* SSRC, not the SSRC in incoming RTP packets.
     // later need a means of changing the SSRC if there's a collision #####
  void registerForMultiplexedRTCPPackets(class RTCPInstance* rtcpInstance) {
    fRTCPInstanceForMultiplexedRTCPPackets = rtcpInstance;
  }
  void deregisterForMultiplexedRTCPPackets() { registerForMultiplexedRTCPPackets(NULL); }

  unsigned timestampFrequency() const {return fTimestampFrequency;}

  RTPReceptionStatsDB& receptionStatsDB() const {
    return *fReceptionStatsDB;
  }

  u_int32_t lastReceivedSSRC() const { return fLastReceivedSSRC; }
  // Note: This is the SSRC in the most recently received RTP packet; not *our* SSRC

  Boolean& enableRTCPReports() { return fEnableRTCPReports; }
  Boolean const& enableRTCPReports() const { return fEnableRTCPReports; }

  void setStreamSocket(int sockNum, unsigned char streamChannelId) {
    // hack to allow sending RTP over TCP (RFC 2236, section 10.12)
    fRTPInterface.setStreamSocket(sockNum, streamChannelId);
  }

  void setAuxilliaryReadHandler(AuxHandlerFunc* handlerFunc,
                                void* handlerClientData) {
    fRTPInterface.setAuxilliaryReadHandler(handlerFunc,
             handlerClientData);
  }

  // Note that RTP receivers will usually not need to call either of the following two functions, because
  // RTP sequence numbers and timestamps are usually not useful to receivers.
  // (Our implementation of RTP reception already does all needed handling of RTP sequence numbers and timestamps.)
  u_int16_t curPacketRTPSeqNum() const { return fCurPacketRTPSeqNum; }
  u_int32_t curPacketRTPTimestamp() const { return fCurPacketRTPTimestamp; }

protected:
  RTPSource(UsageEnvironment& env, Groupsock* RTPgs,
      unsigned char rtpPayloadFormat, u_int32_t rtpTimestampFrequency);
      // abstract base class
  virtual ~RTPSource();

protected:
  RTPInterface fRTPInterface;
  u_int16_t fCurPacketRTPSeqNum;
  u_int32_t fCurPacketRTPTimestamp;
  Boolean fCurPacketMarkerBit;
  Boolean fCurPacketHasBeenSynchronizedUsingRTCP;
  u_int32_t fLastReceivedSSRC;
  class RTCPInstance* fRTCPInstanceForMultiplexedRTCPPackets;

private:
  // redefined virtual functions:
  virtual Boolean isRTPSource() const;
  virtual void getAttributes() const;

private:
  unsigned char fRTPPayloadFormat;
  unsigned fTimestampFrequency;
  u_int32_t fSSRC;
  Boolean fEnableRTCPReports; // whether RTCP "RR" reports should be sent for this source (default: True)

  RTPReceptionStatsDB* fReceptionStatsDB;
};


class RTPReceptionStats; // forward

class RTPReceptionStatsDB {
public:
  unsigned totNumPacketsReceived() const { return fTotNumPacketsReceived; }
  unsigned numActiveSourcesSinceLastReset() const {
    return fNumActiveSourcesSinceLastReset;
 }

  void reset();
      // resets periodic stats (called each time they're used to
      // generate a reception report)

  class Iterator {
  public:
    Iterator(RTPReceptionStatsDB& receptionStatsDB);
    virtual ~Iterator();

    RTPReceptionStats* next(Boolean includeInactiveSources = False);
        // NULL if none

  private:
    HashTable::Iterator* fIter;
  };

  // The following is called whenever a RTP packet is received:
  void noteIncomingPacket(u_int32_t SSRC, u_int16_t seqNum,
        u_int32_t rtpTimestamp,
        unsigned timestampFrequency,
        Boolean useForJitterCalculation,
        struct timeval& resultPresentationTime,
        Boolean& resultHasBeenSyncedUsingRTCP,
        unsigned packetSize /* payload only */);

  // The following is called whenever a RTCP SR packet is received:
  void noteIncomingSR(u_int32_t SSRC,
          u_int32_t ntpTimestampMSW, u_int32_t ntpTimestampLSW,
          u_int32_t rtpTimestamp);

  // The following is called when a RTCP BYE packet is received:
  void removeRecord(u_int32_t SSRC);

  RTPReceptionStats* lookup(u_int32_t SSRC) const;

protected: // constructor and destructor, called only by RTPSource:
  friend class RTPSource;
  RTPReceptionStatsDB();
  virtual ~RTPReceptionStatsDB();

protected:
  void add(u_int32_t SSRC, RTPReceptionStats* stats);

protected:
  friend class Iterator;
  unsigned fNumActiveSourcesSinceLastReset;

private:
  HashTable* fTable;
  unsigned fTotNumPacketsReceived; // for all SSRCs
};


Boolean seqNumLT(u_int16_t s1, u_int16_t s2);
  // a 'less-than' on 16-bit sequence numbers

#endif
