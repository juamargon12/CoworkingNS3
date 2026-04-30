#include "vigilaDiseno.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("VigilaDiseno");

VigilaDiseno::VigilaDiseno(Ptr<PacketSink> sink)
{
  m_totalBytes = 0;
  sink->TraceConnectWithoutContext("Rx", MakeCallback(&VigilaDiseno::PaqueteRecibido, this));
}

void 
VigilaDiseno::PaqueteRecibido(Ptr<const Packet> p, const Address &addr)
{
  m_totalBytes += p->GetSize();
}

uint64_t VigilaDiseno::TotalBytesRecibidos() { return m_totalBytes; }

double VigilaDiseno::CalcularThroughput(Time duration)
{
  double durationSeconds = duration.GetSeconds();
  if (durationSeconds <= 0) return 0.0;
  
  // Bytes * 8 = Bits. Bits / 1e6 = Megabits.
  // Mbps = Megabits / Segundos
  return (m_totalBytes * 8.0) / (durationSeconds * 1000000.0);
}