#ifndef VIGILA_TRADING_H
#define VIGILA_TRADING_H

#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/average.h"
#include "ns3/packet-sink.h"
#include "ns3/nstime.h"

using namespace ns3;

class VigilaTrading
{
public:
  VigilaTrading(Ptr<PacketSink> sink);
  uint32_t TotalPaquetesRecibidos();
  
  // Devolvemos objetos Time para precisión
  Time RetardoMedio();
  Time JitterMedio();

private:
  void PaqueteRecibido (Ptr<const Packet> paquete, const Address &desde, const Address &local, const SeqTsSizeHeader &header);

  uint32_t m_count;
  Average<double> m_delayAvg;
  Average<double> m_jitterAvg;
  Time m_lastDelay;
};

#endif