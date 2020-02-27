
#pragma once

#include <cstdint>
#include <optional>
#include <sys/time.h>

class RTPReceptionStats
{
public:
  RTPReceptionStats(uint32_t SSRC, uint16_t initialSeqNum);
  RTPReceptionStats(uint32_t SSRC);
  uint32_t SSRC() const;
  size_t numPacketsReceivedSinceLastReset() const;
  size_t totNumPacketsReceived() const;
  double totNumKBytesReceived() const;
  size_t totNumPacketsExpected() const;
  size_t baseExtSeqNumReceived() const;
  size_t lastResetExtSeqNumReceived() const;
  size_t highestExtSeqNumReceived() const;
  double jitter() const;
  size_t lastReceivedSR_NTPmsw() const;
  size_t lastReceivedSR_NTPlsw() const;
  struct timeval const& lastReceivedSR_time() const;
  size_t minInterPacketGapUS() const;
  size_t maxInterPacketGapUS() const;
  struct timeval const& totalInterPacketGaps() const;
  virtual ~RTPReceptionStats();
  void noteIncomingPacket(
    uint16_t sequence_number,
    uint32_t rtpTimestamp,
    size_t timestampFrequency,
    bool useForJitterCalculation,
    struct timeval& resultPresentationTime,
    bool& resultHasBeenSyncedUsingRTCP,
    size_t packetSize /* payload only */
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

  void consume_packet_size(size_t sequence_number);
  void consume_sequence_number(uint16_t sequence_number);
  void consume_packet_reception_time(struct timeval timeNow);
  void update_jitter_estimate(
    uint32_t rtpTimestamp,
    size_t timestampFrequency,
    struct timeval timeNow
  );

  uint32_t fSSRC;
  size_t fNumPacketsReceivedSinceLastReset;
  size_t fTotNumPacketsReceived;
  size_t fTotBytesReceived;
  bool fHaveSeenInitialSequenceNumber;
  size_t fBaseExtSeqNumReceived;
  size_t fLastResetExtSeqNumReceived;
  size_t fHighestExtSeqNumReceived;
  std::optional<int> fLastTransit; // used in the jitter calculation
  uint32_t fPreviousPacketRTPTimestamp;
  double fJitter;
  // The following are recorded whenever we receive a RTCP SR for this SSRC:
  size_t fLastReceivedSR_NTPmsw; // NTP timestamp (from SR), most-signif
  size_t fLastReceivedSR_NTPlsw; // NTP timestamp (from SR), least-signif
  struct timeval fLastReceivedSR_time;
  struct timeval fLastPacketReceptionTime;
  size_t fMinInterPacketGapUS, fMaxInterPacketGapUS;
  struct timeval fTotalInterPacketGaps;
  // Used to convert from RTP timestamp to 'wall clock' time:
  bool fHasBeenSynchronized;
  uint32_t fSyncTimestamp;
  struct timeval fSyncTime;
};

bool seqNumLT(u_int16_t s1, u_int16_t s2);
