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
// Implementation

#include "RTPSource.hh"
#include "GroupsockHelper.hh"

#define DISABLE_SR_SYNC 1

////////// RTPSource //////////

Boolean RTPSource::hasBeenSynchronizedUsingRTCP() {
  return fCurPacketHasBeenSynchronizedUsingRTCP;
}

Boolean RTPSource::isRTPSource() const {
  return True;
}

RTPSource::RTPSource(UsageEnvironment& env, Groupsock* RTPgs,
		     unsigned char rtpPayloadFormat,
		     u_int32_t rtpTimestampFrequency)
  : FramedSource(env),
    fRTPInterface(this, RTPgs),
    fCurPacketHasBeenSynchronizedUsingRTCP(False), fLastReceivedSSRC(0),
    fRTCPInstanceForMultiplexedRTCPPackets(NULL),
    fRTPPayloadFormat(rtpPayloadFormat), fTimestampFrequency(rtpTimestampFrequency),
    fSSRC(our_random32()), fEnableRTCPReports(True) {
  fReceptionStatsDB = new RTPReceptionStatsDB();
}

RTPSource::~RTPSource() {
  delete fReceptionStatsDB;
}

void RTPSource::getAttributes() const {
  envir().setResultMsg(""); // Fix later to get attributes from  header #####
}


////////// RTPReceptionStatsDB //////////

RTPReceptionStatsDB::RTPReceptionStatsDB()
  : fTable(HashTable::create(ONE_WORD_HASH_KEYS)), fTotNumPacketsReceived(0) {
  reset();
}

void RTPReceptionStatsDB::reset() {
  fNumActiveSourcesSinceLastReset = 0;

  Iterator iter(*this);
  RTPReceptionStats* stats;
  while ((stats = iter.next()) != NULL) {
    stats->reset();
  }
}

RTPReceptionStatsDB::~RTPReceptionStatsDB() {
  // First, remove and delete all stats records from the table:
  RTPReceptionStats* stats;
  while ((stats = (RTPReceptionStats*)fTable->RemoveNext()) != NULL) {
    delete stats;
  }

  // Then, delete the table itself:
  delete fTable;
}

void RTPReceptionStatsDB
::noteIncomingPacket(u_int32_t SSRC, u_int16_t seqNum,
		     u_int32_t rtpTimestamp, unsigned timestampFrequency,
		     Boolean useForJitterCalculation,
		     struct timeval& resultPresentationTime,
		     Boolean& resultHasBeenSyncedUsingRTCP,
		     unsigned packetSize) {
  ++fTotNumPacketsReceived;
  RTPReceptionStats* stats = lookup(SSRC);
  if (stats == NULL) {
    // This is the first time we've heard from this SSRC.
    // Create a new record for it:
    stats = new RTPReceptionStats(SSRC, seqNum);
    if (stats == NULL) return;
    add(SSRC, stats);
  }

  if (stats->numPacketsReceivedSinceLastReset() == 0) {
    ++fNumActiveSourcesSinceLastReset;
  }

  stats->noteIncomingPacket(seqNum, rtpTimestamp, timestampFrequency,
			    useForJitterCalculation,
			    resultPresentationTime,
			    resultHasBeenSyncedUsingRTCP, packetSize);
}

void RTPReceptionStatsDB
::noteIncomingSR(u_int32_t SSRC,
		 u_int32_t ntpTimestampMSW, u_int32_t ntpTimestampLSW,
		 u_int32_t rtpTimestamp) {
  RTPReceptionStats* stats = lookup(SSRC);
  if (stats == NULL) {
    // This is the first time we've heard of this SSRC.
    // Create a new record for it:
    stats = new RTPReceptionStats(SSRC);
    if (stats == NULL) return;
    add(SSRC, stats);
  }

  stats->noteIncomingSR(ntpTimestampMSW, ntpTimestampLSW, rtpTimestamp);
}

void RTPReceptionStatsDB::removeRecord(u_int32_t SSRC) {
  RTPReceptionStats* stats = lookup(SSRC);
  if (stats != NULL) {
    long SSRC_long = (long)SSRC;
    fTable->Remove((char const*)SSRC_long);
    delete stats;
  }
}

RTPReceptionStatsDB::Iterator
::Iterator(RTPReceptionStatsDB& receptionStatsDB)
  : fIter(HashTable::Iterator::create(*(receptionStatsDB.fTable))) {
}

RTPReceptionStatsDB::Iterator::~Iterator() {
  delete fIter;
}

RTPReceptionStats*
RTPReceptionStatsDB::Iterator::next(Boolean includeInactiveSources) {
  char const* key; // dummy

  // If asked, skip over any sources that haven't been active
  // since the last reset:
  RTPReceptionStats* stats;
  do {
    stats = (RTPReceptionStats*)(fIter->next(key));
  } while (stats != NULL && !includeInactiveSources
	   && stats->numPacketsReceivedSinceLastReset() == 0);

  return stats;
}

RTPReceptionStats* RTPReceptionStatsDB::lookup(u_int32_t SSRC) const {
  long SSRC_long = (long)SSRC;
  return (RTPReceptionStats*)(fTable->Lookup((char const*)SSRC_long));
}

void RTPReceptionStatsDB::add(u_int32_t SSRC, RTPReceptionStats* stats) {
  long SSRC_long = (long)SSRC;
  fTable->Add((char const*)SSRC_long, stats);
}

