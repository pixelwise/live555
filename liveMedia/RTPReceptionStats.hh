
#pragma once

#include <cstdint>
#include <sys/time.h>

class RTPReceptionStats
{
public:
  RTPReceptionStats(uint32_t SSRC, uint16_t initialSeqNum);
  RTPReceptionStats(uint32_t SSRC);
  uint32_t SSRC() const { return fSSRC; }
  unsigned numPacketsReceivedSinceLastReset() const {
    return fNumPacketsReceivedSinceLastReset;
  }
  unsigned totNumPacketsReceived() const { return fTotNumPacketsReceived; }
  double totNumKBytesReceived() const;

  unsigned totNumPacketsExpected() const {
    return (fHighestExtSeqNumReceived - fBaseExtSeqNumReceived) + 1;
  }

  unsigned baseExtSeqNumReceived() const { return fBaseExtSeqNumReceived; }
  unsigned lastResetExtSeqNumReceived() const {
    return fLastResetExtSeqNumReceived;
  }
  unsigned highestExtSeqNumReceived() const {
    return fHighestExtSeqNumReceived;
  }

  unsigned jitter() const;

  unsigned lastReceivedSR_NTPmsw() const { return fLastReceivedSR_NTPmsw; }
  unsigned lastReceivedSR_NTPlsw() const { return fLastReceivedSR_NTPlsw; }
  struct timeval const& lastReceivedSR_time() const {
    return fLastReceivedSR_time;
  }
  unsigned minInterPacketGapUS() const { return fMinInterPacketGapUS; }
  unsigned maxInterPacketGapUS() const { return fMaxInterPacketGapUS; }
  struct timeval const& totalInterPacketGaps() const {
    return fTotalInterPacketGaps;
  }

  virtual ~RTPReceptionStats();
  void noteIncomingPacket(
    uint16_t seqNum, uint32_t rtpTimestamp,
    unsigned timestampFrequency,
    bool useForJitterCalculation,
    struct timeval& resultPresentationTime,
    bool& resultHasBeenSyncedUsingRTCP,
    unsigned packetSize /* payload only */
  );
  void noteIncomingSR(
    uint32_t ntpTimestampMSW,
    uint32_t ntpTimestampLSW,
    uint32_t rtpTimestamp
  );
  void init(uint32_t SSRC);
  void initSeqNum(u_int16_t initialSeqNum);
  void reset();

private:
  uint32_t fSSRC;
  unsigned fNumPacketsReceivedSinceLastReset;
  unsigned fTotNumPacketsReceived;
  uint32_t fTotBytesReceived_hi, fTotBytesReceived_lo;
  bool fHaveSeenInitialSequenceNumber;
  unsigned fBaseExtSeqNumReceived;
  unsigned fLastResetExtSeqNumReceived;
  unsigned fHighestExtSeqNumReceived;
  int fLastTransit; // used in the jitter calculation
  uint32_t fPreviousPacketRTPTimestamp;
  double fJitter;
  // The following are recorded whenever we receive a RTCP SR for this SSRC:
  unsigned fLastReceivedSR_NTPmsw; // NTP timestamp (from SR), most-signif
  unsigned fLastReceivedSR_NTPlsw; // NTP timestamp (from SR), least-signif
  struct timeval fLastReceivedSR_time;
  struct timeval fLastPacketReceptionTime;
  unsigned fMinInterPacketGapUS, fMaxInterPacketGapUS;
  struct timeval fTotalInterPacketGaps;
  // Used to convert from RTP timestamp to 'wall clock' time:
  bool fHasBeenSynchronized;
  uint32_t fSyncTimestamp;
  struct timeval fSyncTime;
};

bool seqNumLT(u_int16_t s1, u_int16_t s2);
