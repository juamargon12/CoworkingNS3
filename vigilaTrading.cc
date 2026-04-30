#include "vigilaTrading.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("VigilaTrading");

VigilaTrading::VigilaTrading(Ptr<PacketSink> sink)
{
  m_count = 0;
  m_lastDelay = Seconds(0);
  // Nos conectamos a la traza que incluye el Header con Timestamp
  sink->TraceConnectWithoutContext("RxWithSeqTsSize", MakeCallback(&VigilaTrading::PaqueteRecibido, this));
}

void 
VigilaTrading::PaqueteRecibido(Ptr<const Packet> p, const Address &from, const Address &local, const SeqTsSizeHeader &header)
{
  Time now = Simulator::Now();
  Time sentTime = header.GetTs();
  Time delay = now - sentTime; // Latencia instantánea

  m_count++;
  m_delayAvg.Update(delay.GetSeconds());

  // Cálculo de Jitter (variación del retardo)
  if (m_count > 1) {
    Time jitter = Abs(delay - m_lastDelay);
    m_jitterAvg.Update(jitter.GetSeconds());
  }
  m_lastDelay = delay;
}

uint32_t VigilaTrading::TotalPaquetesRecibidos() { return m_count; }

Time VigilaTrading::RetardoMedio() {
  if (m_count == 0) return Seconds(0);
  return Seconds(m_delayAvg.Mean());
}

Time VigilaTrading::JitterMedio() {
  if (m_count < 2) return Seconds(0);
  return Seconds(m_jitterAvg.Mean());
}