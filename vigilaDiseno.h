#ifndef VIGILA_DISENO_H
#define VIGILA_DISENO_H

#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/packet-sink.h"
#include "ns3/nstime.h" 

using namespace ns3;

class VigilaDiseno
{
public:
  VigilaDiseno(Ptr<PacketSink> sink);
  uint64_t TotalBytesRecibidos();
  double CalcularThroughput(Time duration);

private:
  void PaqueteRecibido (Ptr<const Packet> p, const Address &addr);
  uint64_t m_totalBytes;
};

#endif