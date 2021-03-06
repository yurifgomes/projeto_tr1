#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

//  DEIXEI AQUI PRA FACILITAR QLQR VISUALIZAÇÃO
//
// Default Network Topology
//
// Number of wifi or csma nodes can be increased up to 250
//                          |
//                 Rank 0   |   Rank 1
// -------------------------|----------------------------
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

int 
main (int argc, char *argv[])
{
  bool verbose = true;
  // Cria 9 nodes(?) wifi e LAN - Deveriam ser 9 ou 10? Na Topologia n1 é p2p e LAN?
  uint32_t nCsma = 9;
  uint32_t nWifi = 9;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  // Check for valid number of csma or wifi nodes
  // 250 should be enough, otherwise IP addresses 
  // soon become an issue
  if (nWifi > 250 || nCsma > 250)
    {
      std::cout << "Too many wifi or csma nodes, no more than 250 each." << std::endl;
      return 1;
    }

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  // Cria dois nodes p2p. Futuro devem ser 3? VERIFICAR!
  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  // caracteristicas da conexão p2p, possivel alterar conforme necessidade
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Instalação dos nodes p2p - na topologia seriam os nodes n0 e n1
  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  //Cria os 9 nodes LAN
  NodeContainer csmaNodes;
  // Pega o segundo node p2p (n0 e n1, pega n1) e cria os 9 LAN adicionais
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (nCsma);

  // Caracteristicas da conexão LAN
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Instalação dos nodes LAN
  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  // Criação dos nodes wifi
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  // O primeiro node p2p (tipologia n0) vira o AP do wifi
  NodeContainer wifiApNode = p2pNodes.Get (0);

  // COnfiguração inicial dos nós wifi. Configura a camada fisica e o canal,
  // cria Default, verificar a documentação para eventuais trocas caso necessarias.
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Documentação pag 62, indica que a criação da camada MAC foi criada com Qos (qualidade de serviço?)
  // A documentação cria as camadas MAC non-Qos (sem qualidade de serviço). Qual a diferença?
  // Melhor deixar assim, com QoS
  WifiHelper wifi;
  // Esse metodo determina o typo de algoritmo utilizado para controle. Algoritmo AARF
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  // cARACTERISTICAS DO MAC. 
  WifiMacHelper mac;
  
  // IMPORTANTE: acho que aqui é criado o modelo 802.11. Cria-se um objeto ns-3-ssid. Pagina 63 da documentação. 
  Ssid ssid = Ssid ("ns-3-ssid");
  // STA = Station? "a station (STA) is a device that has the capability to use the 802.11 protocol."
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  
  // Aqui ele termina de criar os nodes wifi.
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Nodes criados, aqui é iniciada a configuração do AP.
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  // Cria um app device e instala tudo!
  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  // Objeto que vai dar "mobilidade" aos nós. Bem legal lol
  // A ideia é que os nos que não sejam AP fiquem andando por ai sem rumo na vida. 
  MobilityHelper mobility;

  // Espalhando os nodes pelo grid. Tamanho do grid suficiente? 
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  // Movimentos aleatorios dos nodes.
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  // Instalação desse modelo nos nós wifi.                           
  mobility.Install (wifiStaNodes);

  // Aqui ele deixa o AP fixo no grid, sem se movimentar
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  // Criação dos protocolos
  InternetStackHelper stack;
  stack.Install (csmaNodes);
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  // Cria os endereços no modelo ipv4
  Ipv4AddressHelper address;

  // Designa a rede 10.1.1.0 para os dois nós p2p
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  // Designa a rede 10.1.2.0 para os nós CSMA
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  // Designa a rede 10.1.3.0 para os dispositivos wifi e AP
  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (staDevices);
  address.Assign (apDevices);

  // Server echo vai no nó mais a direita na topologia. Seria um nó adicional? São 8 nós e ele cria no nono?
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get (nCsma));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));
  
  // Echo Client vai no ultimo STA (station node?) criado. De novo o numero 9. Pesquisar!
  UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  // O echo client aponta para o server da rede CSMA
  ApplicationContainer clientApps = 
    echoClient.Install (wifiStaNodes.Get (nWifi - 1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));


  // Enable internetworking. Necessario pq? Não sei, mas tem que estar enable.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // A simulação roda forever and ever, precisa parar em um tempo. 
  Simulator::Stop (Seconds (10.0));

  // Permite visualizar o trafego. Conceito: Pcap Tracing. Pesquisar. 
  if (tracing == true)
    {
      pointToPoint.EnablePcapAll ("third");
      phy.EnablePcap ("third", apDevices.Get (0));
      csma.EnablePcap ("third", csmaDevices.Get (0), true);
    }

  // Roda e encerra
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
