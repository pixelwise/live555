#include "RTPReceptionStats.hh"

#include <algorithm>
#include <cmath>
#include <iostream>

#ifndef MILLION
#define MILLION 1000000
#endif

RTPReceptionStats::RTPReceptionStats(
  uint32_t SSRC,
  uint16_t initialSeqNum
)
{
  _last_rtp_sequence_number = initialSeqNum;
  init(SSRC);
}

RTPReceptionStats::RTPReceptionStats(uint32_t SSRC)
{
  init(SSRC);
}

RTPReceptionStats::~RTPReceptionStats()
{
}

void RTPReceptionStats::init(uint32_t SSRC)
{
  fSSRC = SSRC;
  fTotNumPacketsReceived = 0;
  fTotBytesReceived = 0;
  fBaseExtSeqNumReceived = 0;
  fHighestExtSeqNumReceived = 0;
  fPreviousPacketRTPTimestamp = 0;
  fJitter = 0.0;
  fLastReceivedSR_NTPmsw = fLastReceivedSR_NTPlsw = 0;
  fLastReceivedSR_time.tv_sec = fLastReceivedSR_time.tv_usec = 0;
  fLastPacketReceptionTime.tv_sec = fLastPacketReceptionTime.tv_usec = 0;
  fMinInterPacketGapUS = 0x7FFFFFFF;
  fMaxInterPacketGapUS = 0;
  fTotalInterPacketGaps.tv_sec = fTotalInterPacketGaps.tv_usec = 0;
#if DISABLE_SR_SYNC
  fHasBeenSynchronized = true;
#else
  fHasBeenSynchronized = false;
#endif
  fSyncTime.tv_sec = fSyncTime.tv_usec = 0;
  reset();
}

uint32_t RTPReceptionStats::SSRC() const
{
  return fSSRC;
}

size_t RTPReceptionStats::numPacketsReceivedSinceLastReset() const
{
  return fNumPacketsReceivedSinceLastReset;
}

size_t RTPReceptionStats::totNumPacketsReceived() const
{
  return fTotNumPacketsReceived;
}

size_t RTPReceptionStats::totNumPacketsExpected() const
{
  return (fHighestExtSeqNumReceived - fBaseExtSeqNumReceived) + 1;
}

size_t RTPReceptionStats::baseExtSeqNumReceived() const
{
  return fBaseExtSeqNumReceived;
}

size_t RTPReceptionStats::lastResetExtSeqNumReceived() const
{
  return fLastResetExtSeqNumReceived;
}

size_t RTPReceptionStats::highestExtSeqNumReceived() const
{
  return fHighestExtSeqNumReceived;
}

size_t RTPReceptionStats::lastReceivedSR_NTPmsw() const
{
  return fLastReceivedSR_NTPmsw;
}

size_t RTPReceptionStats::lastReceivedSR_NTPlsw() const
{
  return fLastReceivedSR_NTPlsw;
}

struct timeval const& RTPReceptionStats::lastReceivedSR_time() const
{
  return fLastReceivedSR_time;
}

size_t RTPReceptionStats::minInterPacketGapUS() const
{
  return fMinInterPacketGapUS;
}

size_t RTPReceptionStats::maxInterPacketGapUS() const
{
  return fMaxInterPacketGapUS;
}

struct timeval const& RTPReceptionStats::totalInterPacketGaps() const
{
  return fTotalInterPacketGaps;
}

void RTPReceptionStats::noteIncomingPacket(
  uint16_t sequence_number,
  uint32_t rtpTimestamp,
  size_t timestampFrequency,
  bool useForJitterCalculation,
  struct timeval& resultPresentationTime,
  bool& resultHasBeenSyncedUsingRTCP,
  size_t packetSize
)
{
  ++fNumPacketsReceivedSinceLastReset;
  ++fTotNumPacketsReceived;
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);

  consume_packet_size(packetSize);
  consume_sequence_number(sequence_number);
  consume_packet_reception_time(timeNow);
  if (useForJitterCalculation)
    update_jitter_estimate(rtpTimestamp, timestampFrequency, timeNow);

  // Return the 'presentation time' that corresponds to "rtpTimestamp":
  if (fSyncTime.tv_sec == 0 && fSyncTime.tv_usec == 0) {
    // This is the first timestamp that we've seen, so use the current
    // 'wall clock' time as the synchronization time.  (This will be
    // corrected later when we receive RTCP SRs.)
    fSyncTimestamp = rtpTimestamp;
    fSyncTime = timeNow;
  }

  int timestampDiff = rtpTimestamp - fSyncTimestamp;
      // Note: This works even if the timestamp wraps around
      // (as long as "int" is 32 bits)

  // Divide this by the timestamp frequency to get real time:
  double timeDiff = timestampDiff/(double)timestampFrequency;

  // Add this to the 'sync time' to get our result:
  size_t const million = 1000000;
  size_t seconds, uSeconds;
  if (timeDiff >= 0.0) {
    seconds = fSyncTime.tv_sec + (size_t)(timeDiff);
    uSeconds = fSyncTime.tv_usec
      + (size_t)((timeDiff - (size_t)timeDiff)*million);
    if (uSeconds >= million) {
      uSeconds -= million;
      ++seconds;
    }
  } else {
    timeDiff = -timeDiff;
    seconds = fSyncTime.tv_sec - (size_t)(timeDiff);
    uSeconds = fSyncTime.tv_usec
      - (size_t)((timeDiff - (size_t)timeDiff)*million);
    if ((int)uSeconds < 0) {
      uSeconds += million;
      --seconds;
    }
  }
  resultPresentationTime.tv_sec = seconds;
  resultPresentationTime.tv_usec = uSeconds;
  resultHasBeenSyncedUsingRTCP = fHasBeenSynchronized;

  // Save these as the new synchronization timestamp & time:
  fSyncTimestamp = rtpTimestamp;
  fSyncTime = resultPresentationTime;

  fPreviousPacketRTPTimestamp = rtpTimestamp;
}

void RTPReceptionStats::consume_packet_size(size_t packet_size)
{
  fTotBytesReceived += packet_size;
}

void RTPReceptionStats::consume_sequence_number(uint16_t sequence_number)
{
  if (_last_rtp_sequence_number)
  {
    size_t delta =
      (sequence_number > *_last_rtp_sequence_number) ?
      size_t(sequence_number - *_last_rtp_sequence_number) :
      (size_t(sequence_number) + size_t(65536) - size_t(*_last_rtp_sequence_number));
    fHighestExtSeqNumReceived += delta;
    if (delta > 100)
    {
      std::cerr << "rtp warning: large sequence number jump: " << delta << std::endl;
    }
  }
  _last_rtp_sequence_number = sequence_number;
}

void RTPReceptionStats::consume_packet_reception_time(struct timeval timeNow)
{
  if (
    fLastPacketReceptionTime.tv_sec != 0
    || fLastPacketReceptionTime.tv_usec != 0
  )
  {
    size_t gap
      = (timeNow.tv_sec - fLastPacketReceptionTime.tv_sec)*MILLION
      + timeNow.tv_usec - fLastPacketReceptionTime.tv_usec; 
    if (gap > fMaxInterPacketGapUS) {
      fMaxInterPacketGapUS = gap;
    }
    if (gap < fMinInterPacketGapUS) {
      fMinInterPacketGapUS = gap;
    }
    fTotalInterPacketGaps.tv_usec += gap;
    if (fTotalInterPacketGaps.tv_usec >= MILLION) {
      ++fTotalInterPacketGaps.tv_sec;
      fTotalInterPacketGaps.tv_usec -= MILLION;
    }
  }
  fLastPacketReceptionTime = timeNow;  
}

void RTPReceptionStats::update_jitter_estimate(
  uint32_t rtpTimestamp,
  size_t timestampFrequency,
  struct timeval timeNow
)
{
  if (rtpTimestamp != fPreviousPacketRTPTimestamp)
  {
    size_t arrival = (
      timestampFrequency * timeNow.tv_sec
      + std::round(timestampFrequency * timeNow.tv_usec * 1e-6)
    );
    int transit = arrival - rtpTimestamp;
    double jitter = std::abs(transit - fLastTransit.value_or(transit));
    fLastTransit = transit;
    fJitter += (1.0 / 16.0) * (jitter - fJitter);
  }
}

void RTPReceptionStats::noteIncomingSR(
  uint32_t ntpTimestampMSW,
  uint32_t ntpTimestampLSW,
  uint32_t rtpTimestamp
)
{
  fLastReceivedSR_NTPmsw = ntpTimestampMSW;
  fLastReceivedSR_NTPlsw = ntpTimestampLSW;

  gettimeofday(&fLastReceivedSR_time, NULL);

  // Use this SR to update time synchronization information:
  #if !DISABLE_SR_SYNC
      fSyncTimestamp = rtpTimestamp;
      fSyncTime.tv_sec = ntpTimestampMSW - 0x83AA7E80; // 1/1/1900 -> 1/1/1970
      double microseconds = (ntpTimestampLSW*15625.0)/0x04000000; // 10^6/2^32
      fSyncTime.tv_usec = (size_t)(microseconds+0.5);
  #endif
  fHasBeenSynchronized = true;
}

double RTPReceptionStats::totNumKBytesReceived() const
{
  return fTotBytesReceived * 1e-3;
}

double RTPReceptionStats::jitter() const
{
  return fJitter;
}

void RTPReceptionStats::reset()
{
  fNumPacketsReceivedSinceLastReset = 0;
  fLastResetExtSeqNumReceived = fHighestExtSeqNumReceived;
}

bool seqNumLT(uint16_t s1, uint16_t s2)
{
  // a 'less-than' on 16-bit sequence numbers
  int diff = int(s2) - int(s1);
  if (diff > 0)
    return (diff < 0x8000);
  else if (diff < 0)
    return (diff < -0x8000);
  else
    return false;
}
