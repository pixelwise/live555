#include "RTPReceptionStats.hh"

#ifndef MILLION
#define MILLION 1000000
#endif

RTPReceptionStats::RTPReceptionStats(
  uint32_t SSRC,
  uint16_t initialSeqNum
)
{
  initSeqNum(initialSeqNum);
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
  fTotBytesReceived_hi = fTotBytesReceived_lo = 0;
  fBaseExtSeqNumReceived = 0;
  fHighestExtSeqNumReceived = 0;
  fHaveSeenInitialSequenceNumber = false;
  fLastTransit = ~0;
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

void RTPReceptionStats::initSeqNum(uint16_t initialSeqNum)
{
    fBaseExtSeqNumReceived = 0x10000 | initialSeqNum;
    fHighestExtSeqNumReceived = 0x10000 | initialSeqNum;
    fHaveSeenInitialSequenceNumber = true;
}

uint32_t RTPReceptionStats::SSRC() const
{
  return fSSRC;
}

unsigned RTPReceptionStats::numPacketsReceivedSinceLastReset() const
{
  return fNumPacketsReceivedSinceLastReset;
}

unsigned RTPReceptionStats::totNumPacketsReceived() const
{
  return fTotNumPacketsReceived;
}

unsigned RTPReceptionStats::totNumPacketsExpected() const
{
  return (fHighestExtSeqNumReceived - fBaseExtSeqNumReceived) + 1;
}

unsigned RTPReceptionStats::baseExtSeqNumReceived() const
{
  return fBaseExtSeqNumReceived;
}

unsigned RTPReceptionStats::lastResetExtSeqNumReceived() const
{
  return fLastResetExtSeqNumReceived;
}

unsigned RTPReceptionStats::highestExtSeqNumReceived() const
{
  return fHighestExtSeqNumReceived;
}

unsigned RTPReceptionStats::lastReceivedSR_NTPmsw() const
{
  return fLastReceivedSR_NTPmsw;
}

unsigned RTPReceptionStats::lastReceivedSR_NTPlsw() const
{
  return fLastReceivedSR_NTPlsw;
}

struct timeval const& RTPReceptionStats::lastReceivedSR_time() const
{
  return fLastReceivedSR_time;
}

unsigned RTPReceptionStats::minInterPacketGapUS() const
{
  return fMinInterPacketGapUS;
}

unsigned RTPReceptionStats::maxInterPacketGapUS() const
{
  return fMaxInterPacketGapUS;
}

struct timeval const& RTPReceptionStats::totalInterPacketGaps() const
{
  return fTotalInterPacketGaps;
}

void RTPReceptionStats::noteIncomingPacket(
  uint16_t seqNum, uint32_t rtpTimestamp,
  unsigned timestampFrequency,
  bool useForJitterCalculation,
  struct timeval& resultPresentationTime,
  bool& resultHasBeenSyncedUsingRTCP,
  unsigned packetSize
)
{
  if (!fHaveSeenInitialSequenceNumber) initSeqNum(seqNum);

  ++fNumPacketsReceivedSinceLastReset;
  ++fTotNumPacketsReceived;
  uint32_t prevTotBytesReceived_lo = fTotBytesReceived_lo;
  fTotBytesReceived_lo += packetSize;
  if (fTotBytesReceived_lo < prevTotBytesReceived_lo) { // wrap-around
    ++fTotBytesReceived_hi;
  }

  // Check whether the new sequence number is the highest yet seen:
  unsigned oldSeqNum = (fHighestExtSeqNumReceived&0xFFFF);
  unsigned seqNumCycle = (fHighestExtSeqNumReceived&0xFFFF0000);
  unsigned seqNumDifference = (unsigned)((int)seqNum-(int)oldSeqNum);
  unsigned newSeqNum = 0;
  if (seqNumLT((uint16_t)oldSeqNum, seqNum)) {
    // This packet was not an old packet received out of order, so check it:
    
    if (seqNumDifference >= 0x8000) {
      // The sequence number wrapped around, so start a new cycle:
      seqNumCycle += 0x10000;
    }
    
    newSeqNum = seqNumCycle|seqNum;
    if (newSeqNum > fHighestExtSeqNumReceived) {
      fHighestExtSeqNumReceived = newSeqNum;
    }
  } else if (fTotNumPacketsReceived > 1) {
    // This packet was an old packet received out of order
    
    if ((int)seqNumDifference >= 0x8000) {
      // The sequence number wrapped around, so switch to an old cycle:
      seqNumCycle -= 0x10000;
    }
    
    newSeqNum = seqNumCycle|seqNum;
    if (newSeqNum < fBaseExtSeqNumReceived) {
      fBaseExtSeqNumReceived = newSeqNum;
    }
  }

  // Record the inter-packet delay
  struct timeval timeNow;
  gettimeofday(&timeNow, NULL);
  if (fLastPacketReceptionTime.tv_sec != 0
      || fLastPacketReceptionTime.tv_usec != 0) {
    unsigned gap
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

  // Compute the current 'jitter' using the received packet's RTP timestamp,
  // and the RTP timestamp that would correspond to the current time.
  // (Use the code from appendix A.8 in the RTP spec.)
  // Note, however, that we don't use this packet if its timestamp is
  // the same as that of the previous packet (this indicates a multi-packet
  // fragment), or if we've been explicitly told not to use this packet.
  if (useForJitterCalculation
      && rtpTimestamp != fPreviousPacketRTPTimestamp) {
    unsigned arrival = (timestampFrequency*timeNow.tv_sec);
    arrival += (unsigned)
      ((2.0*timestampFrequency*timeNow.tv_usec + 1000000.0)/2000000);
            // note: rounding
    int transit = arrival - rtpTimestamp;
    if (fLastTransit == (~0)) fLastTransit = transit; // hack for first time
    int d = transit - fLastTransit;
    fLastTransit = transit;
    if (d < 0) d = -d;
    fJitter += (1.0/16.0) * ((double)d - fJitter);
  }

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
  unsigned const million = 1000000;
  unsigned seconds, uSeconds;
  if (timeDiff >= 0.0) {
    seconds = fSyncTime.tv_sec + (unsigned)(timeDiff);
    uSeconds = fSyncTime.tv_usec
      + (unsigned)((timeDiff - (unsigned)timeDiff)*million);
    if (uSeconds >= million) {
      uSeconds -= million;
      ++seconds;
    }
  } else {
    timeDiff = -timeDiff;
    seconds = fSyncTime.tv_sec - (unsigned)(timeDiff);
    uSeconds = fSyncTime.tv_usec
      - (unsigned)((timeDiff - (unsigned)timeDiff)*million);
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
      fSyncTime.tv_usec = (unsigned)(microseconds+0.5);
  #endif
  fHasBeenSynchronized = true;
}

double RTPReceptionStats::totNumKBytesReceived() const
{
  double const hiMultiplier = 0x20000000/125.0; // == (2^32)/(10^3)
  return fTotBytesReceived_hi*hiMultiplier + fTotBytesReceived_lo/1000.0;
}

unsigned RTPReceptionStats::jitter() const
{
  return (unsigned)fJitter;
}

void RTPReceptionStats::reset()
{
  fNumPacketsReceivedSinceLastReset = 0;
  fLastResetExtSeqNumReceived = fHighestExtSeqNumReceived;
}

bool seqNumLT(u_int16_t s1, u_int16_t s2)
{
  // a 'less-than' on 16-bit sequence numbers
  int diff = s2 - s1;
  if (diff > 0)
    return (diff < 0x8000);
  else if (diff < 0)
    return (diff < -0x8000);
  else
    return false;
}
