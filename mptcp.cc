
#include "ns3/flow-monitor-module.h"

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// CWND Tracer Function
void CwndTracer(Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue)
{
  double new_time = Simulator::Now().GetSeconds();
  *stream->GetStream() << new_time << " " << newValue << std::endl;
}

// Trace CWND of 2 flows and export to .txt file
static void TraceCwnd(std::string cwnd_tr_file_name)
{
  AsciiTraceHelper ascii;
  if (cwnd_tr_file_name.compare("") == 0)
  {
    NS_LOG_UNCOND("No trace file for cwnd provided");
    return;
  }
  else
  {
    Ptr<OutputStreamWrapper> stream1 = ascii.CreateFileStream((cwnd_tr_file_name + "1").c_str());
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeBoundCallback(&CwndTracer, stream1));
    Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream((cwnd_tr_file_name + "2").c_str());
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeBoundCallback(&CwndTracer, stream2));
  }
}

int main(int argc, char *argv[])
{
  std::string cwnd_tr_file_name = "flow";

  LogComponentEnable("MpTcpSubflow", LOG_LEVEL_ALL);
  LogComponentEnable("MpTcpSocketBase", LOG_LEVEL_DEBUG);
  LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1400));
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
  Config::SetDefault("ns3::DropTailQueue::Mode", StringValue("QUEUE_MODE_PACKETS"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(65));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(MpTcpSocketBase::GetTypeId()));
  Config::SetDefault("ns3::MpTcpSocketBase::MaxSubflows", UintegerValue(2));
  Config::SetDefault("ns3::MpTcpSocketBase::CongestionControl", StringValue("UNCOUPLED"));
  // Config::SetDefault("ns3::MpTcpSocketBase::CongestionControl", StringValue("Fully_Coupled"));    // Fully Coupled
  // Config::SetDefault("ns3::MpTcpSocketBase::CongestionControl", StringValue("Linked_Increases")); // Linked Increase
  Config::SetDefault("ns3::MpTcpSocketBase::ShortPlotting", BooleanValue(true));
  Config::SetDefault("ns3::MpTcpSocketBase::PathManagement", StringValue("NdiffPorts"));
  uint16_t port = 999;

  InternetStackHelper internet;
  Ipv4AddressHelper ipv4;

  NodeContainer router;
  router.Create(4);
  internet.Install(router);
  NodeContainer host;
  host.Create(2);
  internet.Install(host);
  NodeContainer h0r0 = NodeContainer(host.Get(0), router.Get(0));
  NodeContainer h0r1 = NodeContainer(host.Get(0), router.Get(1));

  NodeContainer r0r2 = NodeContainer(router.Get(0), router.Get(2));
  NodeContainer r1r3 = NodeContainer(router.Get(1), router.Get(3));

  NodeContainer h1r2 = NodeContainer(host.Get(1), router.Get(2));
  NodeContainer h1r3 = NodeContainer(host.Get(1), router.Get(3));

  NS_LOG_UNCOND("Create channels.");

  PointToPointHelper p2p;

  p2p.SetDeviceAttribute("DataRate", StringValue("200kbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer l0h0r0 = p2p.Install(h0r0);
  NetDeviceContainer l2r0r2 = p2p.Install(r0r2);
  NetDeviceContainer l6h1r2 = p2p.Install(h1r2);

  p2p.SetDeviceAttribute("DataRate", StringValue("200kbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer l1h0r1 = p2p.Install(h0r1);
  NetDeviceContainer l5r1r3 = p2p.Install(r1r3);

  NetDeviceContainer l7h1r3 = p2p.Install(h1r3);

  NS_LOG_UNCOND("Assign IP Addresses.");

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0h0r0 = ipv4.Assign(l0h0r0);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1h0r1 = ipv4.Assign(l1h0r1);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2r0r2 = ipv4.Assign(l2r0r2);

  ipv4.SetBase("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer i5r1r3 = ipv4.Assign(l5r1r3);

  ipv4.SetBase("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer i6h1r2 = ipv4.Assign(l6h1r2);

  ipv4.SetBase("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer i7h1r3 = ipv4.Assign(l7h1r3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(0.0001));
  // em->SetAttribute("ErrorRate", DoubleValue(0.0002)); // Linked Increase with error rate = 0.0002
  l6h1r2.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  l7h1r3.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  MpTcpPacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = sink.Install(host.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  MpTcpBulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address(i6h1r2.GetAddress(0)), port));
  source.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps = source.Install(host.Get(0));
  sourceApps.Start(Seconds(0.0));
  sourceApps.Stop(Seconds(10.0));

  Simulator::Schedule(Seconds(0.00001), &TraceCwnd, cwnd_tr_file_name);

  NS_LOG_UNCOND("Run Simulation.");
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  NS_LOG_UNCOND("Done.");
}
