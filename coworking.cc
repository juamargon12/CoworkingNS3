/*
 * MODO A (Validación): ./ns3 run "coworking --nDesign=5 --wanBW=150Mbps"
 * MODO B (Gráficas):   ./ns3 run "coworking --generatePlots=true"
 * MODO C (Trazas .pcap): ./ns3 run "coworking --useQoS=true --tracing=true"
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/bridge-helper.h"

// Archivos locales
#include "punto.h" 
#include "vigilaTrading.h"
#include "vigilaDiseno.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("coworking");

// Estructura para devolver resultados de la simulación
struct MisMetricas {
    double latenciaMs;
    double jitterMs;
    double throughputMbps;
    uint32_t paquetesRx;
    uint64_t bytesRx;
};


// FUNCIÓN DE LA SIMULACIÓN
MisMetricas EjecutarSimulacion (
    uint32_t nTrading, uint32_t nDesign, 
    bool useQoS, 
    DataRate wanBW, Time wanDelay, 
    DataRate lanAccessBW, Time lanAccessDelay,
    DataRate trunkBW, Time trunkDelay,
    uint32_t rngRun, bool enableTracing)
{
    // Limpieza y Semilla
    Simulator::Destroy ();
    RngSeedManager::SetSeed (112233);
    RngSeedManager::SetRun (rngRun);

    // Tiempos
    Time simTime = Seconds(30.0);
    Time appStartTime = Seconds(2.0);
    Time appStopTime = Seconds(29.0);

    // --- 1. CREACIÓN DE NODOS ---
    NodeContainer tradingNodes; tradingNodes.Create (nTrading);
    NodeContainer designNodes;  designNodes.Create (nDesign);
    NodeContainer switches;     switches.Create (2);
    NodeContainer routerNode;   routerNode.Create (1);
    NodeContainer internetNode; internetNode.Create (1);

    InternetStackHelper stack;
    stack.InstallAll ();

    // --- 2. TOPOLOGÍA DE RED ---

    // A) Acceso (PCs -> Switches)
    CsmaHelper csmaAccess;
    csmaAccess.SetChannelAttribute ("DataRate", DataRateValue (lanAccessBW));
    csmaAccess.SetChannelAttribute ("Delay", TimeValue (lanAccessDelay));

    NetDeviceContainer tradingDevices, designDevices;
    NetDeviceContainer switchPortsTrading, switchPortsDesign;

    for (uint32_t i = 0; i < nTrading; ++i) {
        NodeContainer linkNodes; linkNodes.Add(tradingNodes.Get(i)); linkNodes.Add(switches.Get(0));
        NetDeviceContainer d = csmaAccess.Install(linkNodes);
        tradingDevices.Add(d.Get(0)); switchPortsTrading.Add(d.Get(1));
    }
    for (uint32_t i = 0; i < nDesign; ++i) {
        NodeContainer linkNodes; linkNodes.Add(designNodes.Get(i)); linkNodes.Add(switches.Get(1));
        NetDeviceContainer d = csmaAccess.Install(linkNodes);
        designDevices.Add(d.Get(0)); switchPortsDesign.Add(d.Get(1));
    }

    // B) Troncal (Switches -> Router)
    CsmaHelper csmaTrunk;
    csmaTrunk.SetChannelAttribute ("DataRate", DataRateValue (trunkBW));
    csmaTrunk.SetChannelAttribute ("Delay", TimeValue (trunkDelay));

    NodeContainer lTT; lTT.Add(switches.Get(0)); lTT.Add(routerNode.Get(0));
    NetDeviceContainer trunkT = csmaTrunk.Install(lTT);
    switchPortsTrading.Add(trunkT.Get(0));

    NodeContainer lTD; lTD.Add(switches.Get(1)); lTD.Add(routerNode.Get(0));
    NetDeviceContainer trunkD = csmaTrunk.Install(lTD);
    switchPortsDesign.Add(trunkD.Get(0));

    // Bridges
    BridgeHelper bridge;
    bridge.Install(switches.Get(0), switchPortsTrading);
    bridge.Install(switches.Get(1), switchPortsDesign);

    // C) WAN (Router -> Internet)
    PointToPointHelper p2pWAN;
    p2pWAN.SetDeviceAttribute ("DataRate", DataRateValue (wanBW));
    p2pWAN.SetChannelAttribute ("Delay", TimeValue (wanDelay));
    
    // Cola física mínima (2 paquetes) para ceder el control al QoS
    p2pWAN.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("2p"));
    
    NetDeviceContainer wanDevices = p2pWAN.Install (routerNode.Get(0), internetNode.Get(0));

    // --- 3. QoS (TRAFFIC CONTROL) ---
    TrafficControlHelper tch;
    if (useQoS) {
        tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", StringValue("1000p"));
    } else {
        tch.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue("1000p"));
    }
    tch.Install (wanDevices.Get (0));

    // --- 4. DIRECCIONAMIENTO IP ---
    Ipv4AddressHelper address;
    
    address.SetBase ("10.0.1.0", "255.255.255.0");
    NetDeviceContainer subT; subT.Add(tradingDevices); subT.Add(trunkT.Get(1));
    address.Assign (subT);

    address.SetBase ("10.0.2.0", "255.255.255.0");
    NetDeviceContainer subD; subD.Add(designDevices); subD.Add(trunkD.Get(1));
    address.Assign (subD);

    address.SetBase ("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer wanIfs = address.Assign (wanDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // --- 5. APLICACIONES Y TRÁFICO ---
    uint16_t portTrading = 50001; 
    uint16_t portDesign = 50000;
    Ipv4Address serverIp = wanIfs.GetAddress (1);

    // Sinks
    PacketSinkHelper sinkT ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), portTrading));
    sinkT.SetAttribute ("EnableSeqTsSizeHeader", BooleanValue (true));
    ApplicationContainer sT = sinkT.Install (internetNode.Get (0));
    sT.Start (Seconds (0.0)); sT.Stop (simTime);

    PacketSinkHelper sinkD ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), portDesign));
    ApplicationContainer sD = sinkD.Install (internetNode.Get (0));
    sD.Start (Seconds (0.0)); sD.Stop (simTime);

    // Jitter
    Ptr<UniformRandomVariable> startJitterVar = CreateObject<UniformRandomVariable> ();
    startJitterVar->SetAttribute ("Min", DoubleValue (0.0)); startJitterVar->SetAttribute ("Max", DoubleValue (8.0));

    // Trading (UDP)
    OnOffHelper clientTrading ("ns3::UdpSocketFactory", InetSocketAddress (serverIp, portTrading));

	clientTrading.SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=1]"));
    clientTrading.SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=1]"));
    clientTrading.SetAttribute ("DataRate", StringValue ("200kbps"));

    clientTrading.SetAttribute ("PacketSize", UintegerValue (512));
    clientTrading.SetAttribute ("EnableSeqTsSizeHeader", BooleanValue (true));
    
    if (useQoS) {
        clientTrading.SetAttribute ("Tos", UintegerValue (0x70)); 
    }

    for (uint32_t i = 0; i < nTrading; ++i) {
        ApplicationContainer app = clientTrading.Install(tradingNodes.Get(i));
        app.Start (appStartTime + Seconds(startJitterVar->GetValue()));
        app.Stop (appStopTime);
    }

    // Diseño (TCP)
    BulkSendHelper clientDesign ("ns3::TcpSocketFactory", InetSocketAddress (serverIp, portDesign));
    clientDesign.SetAttribute ("MaxBytes", UintegerValue (0)); 
    clientDesign.SetAttribute ("SendSize", UintegerValue (1460));

    for (uint32_t i = 0; i < nDesign; ++i) {
        ApplicationContainer app = clientDesign.Install(designNodes.Get(i));
        app.Start (appStartTime + Seconds(startJitterVar->GetValue()));
        app.Stop (appStopTime);
    }

	AsciiTraceHelper ascii;
    p2pWAN.EnableAsciiAll (ascii.CreateFileStream ("coworking.tr"));

    // --- 6. MONITORIZACIÓN ---
    VigilaTrading vigT (sT.Get(0)->GetObject<PacketSink> ());
    VigilaDiseno vigD (sD.Get(0)->GetObject<PacketSink> ());

    // --- 7. TRAZAS ---
    if (enableTracing) {
        // 1. Cable que une un PC de Trading con su Switch
        csmaAccess.EnablePcap ("trading-pc", tradingDevices.Get (0), true);
        
        // 2. Cable que une un PC de Diseño con su Switch
        csmaAccess.EnablePcap ("diseno-pc", designDevices.Get (0), true);

        // 3. Interfaz del Router que conecta con la red de Trading
        // trunkT: 0=Switch, 1=Router
        csmaTrunk.EnablePcap ("router-trading", trunkT.Get (1), true);

        // 4. Interfaz del Router que conecta con la red de Diseño
        // trunkD: 0=Switch, 1=Router
        csmaTrunk.EnablePcap ("router-diseno", trunkD.Get (1), true);

        // 5. Salida WAN del Router hacia Internet
        p2pWAN.EnablePcap ("router-wan", wanDevices.Get (0), true);

    }

    Simulator::Stop (simTime);
    Simulator::Run ();

    if (enableTracing) {
        std::cout << "¡Hecho! 5 archivos .pcap generados.\n" << std::endl;
    }

    // Resultados
    double dur = (appStopTime - appStartTime).GetSeconds();
    MisMetricas m;
    m.latenciaMs = vigT.RetardoMedio().GetSeconds() * 1000.0; // para pasarlo a ms
    m.jitterMs = vigT.JitterMedio().GetSeconds() * 1000.0; // para pasarlo a ms
    m.throughputMbps = vigD.CalcularThroughput(Seconds(dur));
    m.paquetesRx = vigT.TotalPaquetesRecibidos();
    m.bytesRx = vigD.TotalBytesRecibidos();

    Simulator::Destroy ();
    return m;
}

// MAIN
int main (int argc, char *argv[])
{
    // Buffer
    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1460));

    // Parámetros por defecto
    uint32_t nTrading = 50;
    uint32_t nDesign = 25; 
    bool useQoS = false;
    DataRate wanBW ("150Mbps");
    Time wanDelay ("20ms");
    DataRate lanAccessBW ("1Gbps");
    Time lanAccessDelay ("0.7ms");
    DataRate trunkBW ("10Gbps");
    Time trunkDelay ("1us");
    uint32_t rngRun = 1;
    bool enableTracing = false;
    
    bool generatePlots = false; 

    CommandLine cmd;
    cmd.AddValue ("generatePlots", "ACTIVAR MODO GRÁFICAS", generatePlots);
    cmd.AddValue ("nDesign", "Usuarios Diseño (Validación)", nDesign);
    cmd.AddValue ("useQoS", "Activar QoS (Validación)", useQoS);
    cmd.AddValue ("wanBW", "Ancho de Banda WAN (Validación)", wanBW);
    cmd.AddValue ("tracing", "Activar PCAP", enableTracing);
    cmd.AddValue ("rngRun", "Semilla", rngRun);
    cmd.AddValue ("nTrading", "Usuarios Trading", nTrading);
    cmd.AddValue ("wanDelay", "Retardo WAN", wanDelay);
    cmd.AddValue ("lanAccessBW", "Ancho de Banda Acceso", lanAccessBW);
    cmd.AddValue ("lanAccessDelay", "Retardo Acceso", lanAccessDelay);
    cmd.AddValue ("trunkBW", "Ancho de Banda Troncal", trunkBW);
    cmd.AddValue ("trunkDelay", "Retardo Troncal", trunkDelay);
    cmd.Parse (argc, argv);

    Time::SetResolution (Time::NS);

    if (generatePlots) // si generatePlots == true, entramos en el modo gráficas    
    {
        std::cout << "\n=== GENERACIÓN DE GRAFICAS ===" << std::endl;
        std::cout << "Puntos: 70, 90, 110, 130, 150 Mbps ; Intervalo Conf. 90% ; Reps 10" << std::endl;

        Grafica gLat ("grafica1_latencia.plt", "Latencia Trading vs Capacidad", "Capacidad WAN (Mbps)", "Latencia (ms)");
        Grafica gJit ("grafica2_jitter.plt", "Jitter Trading vs Capacidad", "Capacidad WAN (Mbps)", "Jitter (ms)");
        Grafica gThr ("grafica3_throughput.plt", "Throughput Diseno vs Capacidad", "Capacidad WAN (Mbps)", "Throughput (Mbps)");

        Curva cLatF ("FIFO", 0.1); Curva cLatS ("Prioridad Estricta", 0.1);
        Curva cJitF ("FIFO", 0.1); Curva cJitS ("Prioridad Estricta", 0.1);
        Curva cThrF ("FIFO", 0.1); Curva cThrS ("Prioridad Estricta", 0.1);

        std::vector<uint32_t> capacidades = {70, 90, 110, 130, 150};
        int REPETICIONES = 10; 

        for (uint32_t cap : capacidades)
        {
            std::cout << " Procesando " << cap << " Mbps... " << std::flush; //para que por pantalla salga por qué punto vamos
            DataRate bw = DataRate(std::to_string(cap) + "Mbps");

            Punto pLatF, pJitF, pThrF;
            Punto pLatS, pJitS, pThrS;

            for (int r = 1; r <= REPETICIONES; ++r) {
                // FIFO
                MisMetricas mF = EjecutarSimulacion(nTrading, nDesign, false, bw, wanDelay, lanAccessBW, lanAccessDelay, trunkBW, trunkDelay, r, false);
                pLatF.Update(mF.latenciaMs); pJitF.Update(mF.jitterMs); pThrF.Update(mF.throughputMbps);

                // QoS
                MisMetricas mS = EjecutarSimulacion(nTrading, nDesign, true, bw, wanDelay, lanAccessBW, lanAccessDelay, trunkBW, trunkDelay, r, false);
                pLatS.Update(mS.latenciaMs); pJitS.Update(mS.jitterMs); pThrS.Update(mS.throughputMbps);
            }

            cLatF.Add(cap, pLatF); cLatS.Add(cap, pLatS);
            cJitF.Add(cap, pJitF); cJitS.Add(cap, pJitS);
            cThrF.Add(cap, pThrF); cThrS.Add(cap, pThrS);

            std::cout << "OK." << std::endl;
        }

        gLat.Add(cLatF); gLat.Add(cLatS);
        gJit.Add(cJitF); gJit.Add(cJitS);
        gThr.Add(cThrF); gThr.Add(cThrS);

        std::cout << "¡Hecho! Gráficas generadas.\n" << std::endl;
    }

    else // si generatePlots == false, entramos en el modo validación
    {
        NS_LOG_INFO ("--- INICIANDO SIMULACIÓN INNOVA HUB ---");
        NS_LOG_INFO ("QoS: " << (useQoS ? "ON (Prioridad)" : "OFF (FIFO)"));
        NS_LOG_INFO ("WAN BW: " << wanBW);
        NS_LOG_INFO ("Troncal Interno: " << trunkBW);
        NS_LOG_INFO ("PARÁMETROS:");
        NS_LOG_INFO ("\tnTrading:\t\t" << nTrading);
        NS_LOG_INFO ("\tnDesign:\t\t" << nDesign);
        NS_LOG_INFO ("\tuseQoS:\t\t\t" << (useQoS ? "true" : "false"));
        NS_LOG_INFO ("\twanBW:\t\t\t" << wanBW);
        NS_LOG_INFO ("\twanDelay:\t\t" << wanDelay);
        NS_LOG_INFO ("\tlanBW:\t\t\t" << lanAccessBW);
        NS_LOG_INFO ("\tlanDelay:\t\t" << lanAccessDelay);
        NS_LOG_INFO ("\tRngRun:\t\t\t" << rngRun);

        MisMetricas m = EjecutarSimulacion(nTrading, nDesign, useQoS, wanBW, wanDelay, lanAccessBW, lanAccessDelay, trunkBW, trunkDelay, rngRun, enableTracing);
        
        NS_LOG_INFO ("--- RESULTADOS FINALES ---");
        NS_LOG_INFO ("TRADING (UDP):");
        NS_LOG_INFO ("\tPaquetes Recibidos: " << m.paquetesRx);
        NS_LOG_INFO ("\tLatencia Media:     " << m.latenciaMs << " ms");
        NS_LOG_INFO ("\tJitter Medio:       " << m.jitterMs << " ms");
        NS_LOG_INFO ("DISEÑO (TCP):");
        NS_LOG_INFO ("\tTotal Bytes:        " << m.bytesRx);
        NS_LOG_INFO ("\tThroughput:         " << m.throughputMbps << " Mbps");
        NS_LOG_INFO ("------------------------------------------------");
    }

    return 0;
}
