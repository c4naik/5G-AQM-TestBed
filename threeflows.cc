#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"
#include <iostream>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("threeflows");

int
main(int argc, char* argv[])
{


/////////////////////////////////////////////////////////

uint32_t maxBytes =0;  //tcp bulk sender

std::string dataRateRemoteHostLink = "10Gbps";
double bandwidthBand1 = 6000e6;  //6000MHz  Mbps = MHz * 8 ->  48Gbps

Time delayRemoteHostLink = MilliSeconds(1);
Time coreLatency = MilliSeconds(0.1);

Time simTime = Seconds(0.6);
Time tcpAppStartTime = Seconds(0.5);

std::string simTag = "SimResults.txt";
std::string outputDir = "./";

//RLC Buffer
Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(9999999));

//AQM Algorithm
std::string aqmAlgo = "ns3::CoDelQueueDisc";

std::string bwpTraffic = "GBR_GAMING";
EpsBearer voiceBearer(EpsBearer::GBR_GAMING);
uint16_t numerologyBwp1 = 4;
double centralFrequencyBand1 = 28e9;  //28 GHz  
double totalTxPower = 4; //dBm

/////////////////////////////////////////////////////////




    std::ofstream outFile;
    std::string filename = outputDir + "/" + simTag;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }

    outFile.setf(std::ios_base::fixed);








    uint16_t gNbNum = 1;
    uint16_t ueNumPergNb = 3;
    bool logging = false;
    

    // Where we will store the output files.
    CommandLine cmd(__FILE__);

    cmd.AddValue("gNbNum", "The number of gNbs in multiple-ue topology", gNbNum);
    cmd.AddValue("ueNumPergNb", "The number of UE per gNb in multiple-ue topology", ueNumPergNb);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.Parse(argc, argv);
    NS_ABORT_IF(centralFrequencyBand1 < 0.5e9 && centralFrequencyBand1 > 100e9);

    

    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(gNbNum);
    // All units below are in meters
    gridScenario.SetHorizontalBsDistance(5.0);
    gridScenario.SetVerticalBsDistance(5.0);
    gridScenario.SetBsHeight(1.5);
    gridScenario.SetUtHeight(1.5);
    // must be set before BS number
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(gNbNum);
    gridScenario.SetUtNumber(ueNumPergNb * gNbNum);
    gridScenario.SetScenarioHeight(3); // Create a 3x3 scenario where the UE will
    gridScenario.SetScenarioLength(3); // be distribuited.
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();
    NodeContainer gnb = gridScenario.GetBaseStations();
    NodeContainer ut = gridScenario.GetUserTerminals();

    NodeContainer ueTrafficNodeContainer;

    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(j);
        {
            ueTrafficNodeContainer.Add(ue);
        
        }
    }  

    NS_LOG_INFO("Creating " << gridScenario.GetUserTerminals().GetN() << " user terminals and "
                            << gridScenario.GetBaseStations().GetN() << " gNBs");

    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Put the pointers inside nrHelper
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1; // in this example, both bands have a single CC

    // Create the configuration for the CcBwpHelper. SimpleOperationBandConf creates
    // a single BWP per CC
    CcBwpCreator::SimpleOperationBandConf bandConf1(centralFrequencyBand1,
                                                    bandwidthBand1,
                                                    numCcPerBand,
                                                    BandwidthPartInfo::UMi_StreetCanyon);
    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    nrHelper->InitializeOperationBand(&band1);

    /*
     * Start to account for the bandwidth used by the example, as well as
     * the total power that has to be divided among the BWPs.
     */
    double x = pow(10, totalTxPower / 10);  
    double totalBandwidth = bandwidthBand1;
    allBwps = CcBwpCreator::GetAllBwps({band1});

    Packet::EnableChecking();
    Packet::EnablePrinting();

    /*
     *  Case (i): Attributes valid for all the nodes
     */
    // Beamforming method
    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // Core latency
    epcHelper->SetAttribute("S1uLinkDelay", TimeValue(coreLatency));

    // Antennas for all the UEs
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));

    // Antennas for all the gNbs
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    uint32_t bwpId = 0;
    

    // gNb routing between Bearer and bandwidh part
    nrHelper->SetGnbBwpManagerAlgorithmAttribute(bwpTraffic, UintegerValue(bwpId));

    //NGBR_VIDEO_TCP_DEFAULT
    // Ue routing between Bearer and bandwidth part
    nrHelper->SetUeBwpManagerAlgorithmAttribute(bwpTraffic, UintegerValue(bwpId));

    /*
     * We miss many other parameters. By default, not configuring them is equivalent
     * to use the default values. Please, have a look at the documentation to see
     * what are the default values for all the attributes you are not seeing here.
     */

    /*
     * Case (ii): Attributes valid for a subset of the nodes
     */

    // NOT PRESENT IN THIS SIMPLE EXAMPLE

    /*
     * We have configured the attributes we needed. Now, install and get the pointers
     * to the NetDevices, which contains all the NR stack:
     */

    NetDeviceContainer enbNetDev =
        nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    NetDeviceContainer ueTrafficNetDevContainer = nrHelper->InstallUeDevice(ueTrafficNodeContainer , allBwps);

    randomStream += nrHelper->AssignStreams(enbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueTrafficNetDevContainer, randomStream);
    /*
     * Case (iii): Go node for node and change the attributes we have to setup
     * per-node.
     */

    // Get the first netdevice (enbNetDev.Get (0)) and the first bandwidth part (0)
    // and set the attribute.
    nrHelper->GetGnbPhy(enbNetDev.Get(0), 0)
        ->SetAttribute("Numerology", UintegerValue(numerologyBwp1));
    nrHelper->GetGnbPhy(enbNetDev.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10 * log10((bandwidthBand1 / totalBandwidth) * x))); 


    // When all the configuration is done, explicitly call UpdateConfig ()

    for (auto it = enbNetDev.Begin(); it != enbNetDev.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }

    for (auto it = ueTrafficNetDevContainer.Begin(); it != ueTrafficNetDevContainer.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }

    // From here, it is standard NS3. In the future, we will create helpers
    // for this part as well.

    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(3);
    Ptr<Node> remoteHost1 = remoteHostContainer.Get(0);
    Ptr<Node> remoteHost2 = remoteHostContainer.Get(1);
    Ptr<Node> remoteHost3 = remoteHostContainer.Get(2);
    InternetStackHelper internet;       
    internet.Install(remoteHostContainer);
    
    NodeContainer pgwcontainer = epcHelper->GetPgwNode();
    
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(remoteHostContainer);
    mobility.Install(gnb);
    mobility.Install(ut);
    mobility.Install(pgwcontainer);
    
    Ptr<ConstantPositionMobilityModel> u1 = ut.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> u2 = ut.Get (1)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> u3 = ut.Get (2)->GetObject<ConstantPositionMobilityModel> ();
    u1->SetPosition (Vector ( 0.0, 40.0, 0  ));
    u2->SetPosition (Vector ( 0.0, 50.0, 0  ));
    u3->SetPosition (Vector ( 0.0, 60.0, 0  ));
    
    Ptr<ConstantPositionMobilityModel> g1 = gnb.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    g1->SetPosition (Vector ( 10.0, 50.0, 0  ));
    
    Ptr<ConstantPositionMobilityModel> p1 = pgwcontainer.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    p1->SetPosition (Vector ( 90.0, 50.0, 0  ));
    
    Ptr<ConstantPositionMobilityModel> n1 = remoteHostContainer.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n2 = remoteHostContainer.Get (1)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n3 = remoteHostContainer.Get (2)->GetObject<ConstantPositionMobilityModel> ();
    n1->SetPosition (Vector ( 100.0, 40.0, 0  ));
    n2->SetPosition (Vector ( 100.0, 50.0, 0  ));
    n3->SetPosition (Vector ( 100.0, 60.0, 0  ));

    // connect a remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph1;
    p2ph1.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph1.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph1.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices1 = p2ph1.Install(pgw, remoteHost1);
    
    PointToPointHelper p2ph2;
    p2ph2.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph2.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph2.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices2 = p2ph2.Install(pgw, remoteHost2);
    
    PointToPointHelper p2ph3;
    p2ph3.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph3.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph3.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices3 = p2ph3.Install(pgw, remoteHost3);
    
    p2ph1.EnablePcapAll("R1");
    //p2ph2.EnablePcapAll("R2");
    //p2ph3.EnablePcapAll("R3");
   // epcHelper.EnablePcapAll("epc");
   // gridScenario.EnablePcapAll("grid");
    
    NetDeviceContainer internalNet;
    internalNet.Add(enbNetDev);
    internalNet.Add(internetDevices1);
    internalNet.Add(internetDevices2);
    internalNet.Add(internetDevices3);

    QueueDiscContainer queueDiscs;
    TrafficControlHelper tch;//AQM implementation
    tch.SetRootQueueDisc (aqmAlgo);
    queueDiscs = tch.Install(internalNet); 
    AnimationInterface anim("working.xml");

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("11.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internalNet);
    
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    
    //Manually Configure the routing table of Packet Gateway
    Ptr<Ipv4StaticRouting> pgwRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
    pgwRouting->AddHostRouteTo(Ipv4Address("11.0.0.3"), 3);
    pgwRouting->AddHostRouteTo(Ipv4Address("11.0.0.5"), 4);
    pgwRouting->AddHostRouteTo(Ipv4Address("11.0.0.7"), 5);



    Ptr<Ipv4StaticRouting> remoteHostStaticRouting1 = ipv4RoutingHelper.GetStaticRouting(remoteHost1->GetObject<Ipv4>());
    remoteHostStaticRouting1->AddNetworkRouteTo(Ipv4Address("7.0.0.2"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting2 = ipv4RoutingHelper.GetStaticRouting(remoteHost2->GetObject<Ipv4>());
    remoteHostStaticRouting2->AddNetworkRouteTo(Ipv4Address("7.0.0.3"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting3 = ipv4RoutingHelper.GetStaticRouting(remoteHost3->GetObject<Ipv4>());
    remoteHostStaticRouting3->AddNetworkRouteTo(Ipv4Address("7.0.0.4"), Ipv4Mask("255.0.0.0"), 1);
    
    internet.Install(gridScenario.GetUserTerminals());


    Ipv4InterfaceContainer ueTrafficIpIfaceContainer =
        epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueTrafficNetDevContainer));
    
    // Set the default gateway for the UEs
    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(gridScenario.GetUserTerminals().Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
        //outFile<<epcHelper->GetUeDefaultGatewayAddress()<<"\n"; this is 7.0.0.1
    }

    // attach UEs to the closest eNB
    nrHelper->AttachToClosestEnb(ueTrafficNetDevContainer, enbNetDev);
    




    //Add traffic

    uint16_t port = 9;
    //uint16_t port2 = 5001;
    //uint16_t port3 = 5002;
    
    
    
      	//BulkSendHelper source1 ("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(0), port));
      	BulkSendHelper source2 ("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(1), port));
      	BulkSendHelper source3 ("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(2), port));
                  
                  
                  
        OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(0), port));

    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute ("DataRate",StringValue ("100Mbps"));
    onOffHelper.SetAttribute ("PacketSize",UintegerValue(20));       
        
  	// Set the amount of data to send in bytes.  Zero is unlimited.
  	//source1.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  	source2.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  	source3.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  	
  	ApplicationContainer sourceApps;
  	sourceApps.Add(onOffHelper.Install(remoteHost1));
  	//sourceApps.Add(source1.Install (remoteHost3));
  	//sourceApps.Add(source1.Install (remoteHost2));
  	
  	//sourceApps.Add(source2.Install (remoteHost1));
  	//sourceApps.Add(source2.Install (remoteHost3));
  	sourceApps.Add(source2.Install (remoteHost2));
  	
  	//sourceApps.Add(source3.Install (remoteHost1));
  	sourceApps.Add(source3.Install (remoteHost3));
  	//sourceApps.Add(source3.Install (remoteHost2));
  	
  	sourceApps.Start (tcpAppStartTime);
  	sourceApps.Stop (simTime);


	PacketSinkHelper sink1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
	PacketSinkHelper sink2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
	PacketSinkHelper sink3 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
	
	ApplicationContainer sinkApps;
	sinkApps.Add(sink1.Install (ueTrafficNodeContainer.Get(0)));
	sinkApps.Add(sink2.Install (ueTrafficNodeContainer.Get(1)));
	sinkApps.Add(sink3.Install (ueTrafficNodeContainer.Get(2)));
	sinkApps.Start (tcpAppStartTime);
	sinkApps.Stop (simTime);
	  
	  
	  
	  
    // The filter for the voice tcp traffic
    Ptr<EpcTft> voiceTft = Create<EpcTft>();
    EpcTft::PacketFilter dlpfVoice;
    dlpfVoice.localPortStart = port;
    dlpfVoice.localPortEnd = port;
    voiceTft->Add(dlpfVoice);
    
    
    nrHelper->ActivateDedicatedEpsBearer(ueTrafficNetDevContainer, voiceBearer, voiceTft);


    /*Address ueSinkLocalAddress(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(0), porttcp));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", ueSinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(ueTrafficNodeContainer.Get(0));
    sinkApp.Start(tcpAppStartTime);
    sinkApp.Stop(simTime);

    Address ueSinkLocalAddress2(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(1), porttcp2));
    PacketSinkHelper packetSinkHelper2("ns3::TcpSocketFactory", ueSinkLocalAddress2);
    ApplicationContainer sinkApp2 = packetSinkHelper2.Install(ueTrafficNodeContainer.Get(1));
    sinkApp2.Start(tcpAppStartTime);
    sinkApp2.Stop(simTime);

    Address ueSinkLocalAddress3(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(2), porttcp3));
    PacketSinkHelper packetSinkHelper3("ns3::TcpSocketFactory", ueSinkLocalAddress3);
    ApplicationContainer sinkApp3 = packetSinkHelper3.Install(ueTrafficNodeContainer.Get(2));
    sinkApp3.Start(tcpAppStartTime);
    sinkApp3.Stop(simTime);



    

    ApplicationContainer senderApps;
    EpsBearer voiceBearer(EpsBearer::NGBR_VIDEO_TCP_DEFAULT);
    // The filter for the voice tcp traffic
    Ptr<EpcTft> voiceTft = Create<EpcTft>();
    EpcTft::PacketFilter dlpfVoice;
    dlpfVoice.localPortStart = porttcp;
    dlpfVoice.localPortEnd = porttcp3;
    voiceTft->Add(dlpfVoice);




    Ptr<NetDevice> ueDevice = ueTrafficNetDevContainer.Get(0);
    Ptr<NetDevice> ueDevice2 = ueTrafficNetDevContainer.Get(1);
    Ptr<NetDevice> ueDevice3 = ueTrafficNetDevContainer.Get(2);
   
    nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, voiceTft);
    nrHelper->ActivateDedicatedEpsBearer(ueDevice2, voiceBearer, voiceTft);
    nrHelper->ActivateDedicatedEpsBearer(ueDevice3, voiceBearer, voiceTft);


    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    OnOffHelper onOffHelper2("ns3::TcpSocketFactory", Address());
    OnOffHelper onOffHelper3("ns3::TcpSocketFactory", Address());

    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute ("DataRate",StringValue ("200Gbps"));
    onOffHelper.SetAttribute ("PacketSize",UintegerValue(20));

    onOffHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    onOffHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper2.SetAttribute ("DataRate",StringValue ("200Gbps"));
    onOffHelper2.SetAttribute ("PacketSize",UintegerValue(20));

    onOffHelper3.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    onOffHelper3.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper3.SetAttribute ("DataRate",StringValue ("200Gbps"));
    onOffHelper3.SetAttribute ("PacketSize",UintegerValue(20));

    AddressValue remoteAddress(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(0), porttcp));
    AddressValue remoteAddress2(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(1), porttcp2));
    AddressValue remoteAddress3(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(2), porttcp3));

    onOffHelper.SetAttribute("Remote", remoteAddress);
    onOffHelper2.SetAttribute("Remote", remoteAddress2);
    onOffHelper3.SetAttribute("Remote", remoteAddress3);

    senderApps.Add(onOffHelper.Install(remoteHostContainer.Get(0)));
    senderApps.Add(onOffHelper2.Install(remoteHostContainer.Get(1)));
    senderApps.Add(onOffHelper3.Install(remoteHostContainer.Get(2)));

    senderApps.Start(tcpAppStartTime);
    senderApps.Stop(simTime);
*/

    nrHelper->EnableTraces();

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost1);
    endpointNodes.Add(remoteHost2);
    endpointNodes.Add(remoteHost3);
    endpointNodes.Add(gridScenario.GetUserTerminals());

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    Simulator::Stop(simTime);
    Simulator::Run();



    // Print per-flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;



    double flowDuration = (simTime - tcpAppStartTime).GetSeconds();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::stringstream protoStream;
        protoStream << (uint16_t)t.protocol;
        if (t.protocol == 6)
        {
            protoStream.str("TCP");
        }
        if (t.protocol == 17)
        {
            protoStream.str("UDP");
        }
        outFile << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                << t.destinationAddress << ":" << t.destinationPort << ") proto "
                << protoStream.str() << "\n";
        outFile << "  Tx Packets: " << i->second.txPackets << "\n";
        outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
        outFile << "  TxOffered:  " << i->second.txBytes * 8.0 / flowDuration / 1000.0 / 1000.0
                << " Mbps\n";
        outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        if (i->second.rxPackets > 0)
        {
            // Measure the duration of the flow from receiver's perspective
            averageFlowThroughput += i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            outFile << "  Throughput: " << i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000
                    << " Mbps\n";
            outFile << "  Mean delay:  "
                    << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
            // outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << "
            // Mbps \n";
            outFile << "  Mean jitter:  "
                    << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
        }
        else
        {
            outFile << "  Throughput:  0 Mbps\n";
            outFile << "  Mean delay:  0 ms\n";
            outFile << "  Mean jitter: 0 ms\n";
        }
        outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
    }

    double meanFlowThroughput = averageFlowThroughput / stats.size();
    double meanFlowDelay = averageFlowDelay / stats.size();

    outFile << "\n\n  Mean flow throughput: " << meanFlowThroughput << "\n";
    outFile << "  Mean flow delay: " << meanFlowDelay << "\n";

    outFile.close();

    std::ifstream f(filename.c_str());

    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    Simulator::Destroy();

    if (argc == 0)
    {
        double toleranceMeanFlowThroughput = 0.0001 * 56.258560;
        double toleranceMeanFlowDelay = 0.0001 * 0.553292;

        if (meanFlowThroughput >= 56.258560 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 56.258560 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 0.553292 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 0.553292 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else if (argc == 1 and ueNumPergNb == 9) // called from examples-to-run.py with these parameters
    {
        double toleranceMeanFlowThroughput = 0.0001 * 47.858536;
        double toleranceMeanFlowDelay = 0.0001 * 10.504189;

        if (meanFlowThroughput >= 47.858536 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 47.858536 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 10.504189 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 10.504189 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        return EXIT_SUCCESS; // we dont check other parameters configurations at the moment
    }
}




// ... (existing includes and code)

// Remove file-related operations
// std::ofstream outFile;
// std::string filename = outputDir + "/" + simTag;
// outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
// if (!outFile.is_open())
// {
//     std::cerr << "Can't open file " << filename << std::endl;
//     return 1;
// }

// outFile.setf(std::ios_base::fixed);

// Print per-flow statistics
//monitor->CheckForLostPackets();
//Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
//FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

/*double averageFlowThroughput = 0.0;
double averageFlowDelay = 0.0;

double flowDuration = (simTime - tcpAppStartTime).GetSeconds();
for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
{
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::stringstream protoStream;
    protoStream << (uint16_t)t.protocol;
    if (t.protocol == 6)
    {
        protoStream.str("TCP");
    }
    if (t.protocol == 17)
    {
        protoStream.str("UDP");
    }

    // Change from outFile to std::cout
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
              << t.destinationAddress << ":" << t.destinationPort << ") proto "
              << protoStream.str() << "\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / flowDuration / 1000.0 / 1000.0
              << " Mbps\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";

    if (i->second.rxPackets > 0)
    {
        averageFlowThroughput += i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000;
        averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000
                  << " Mbps\n";
        std::cout << "  Mean delay:  " << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
        std::cout << "  Mean jitter:  " << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
    }
    else
    {
        std::cout << "  Throughput:  0 Mbps\n";
        std::cout << "  Mean delay:  0 ms\n";
        std::cout << "  Mean jitter: 0 ms\n";
    }

    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
}

double meanFlowThroughput = averageFlowThroughput / stats.size();
double meanFlowDelay = averageFlowDelay / stats.size();

std::cout << "\n\n  Mean flow throughput: " << meanFlowThroughput << "\n";
std::cout << "  Mean flow delay: " << meanFlowDelay << "\n";

// Remove file closing
// outFile.close();

Simulator::Destroy();

if (argc == 0)
{
    double toleranceMeanFlowThroughput = 0.0001 * 56.258560;
    double toleranceMeanFlowDelay = 0.0001 * 0.553292;

    if (meanFlowThroughput >= 56.258560 - toleranceMeanFlowThroughput &&
        meanFlowThroughput <= 56.258560 + toleranceMeanFlowThroughput &&
        meanFlowDelay >= 0.553292 - toleranceMeanFlowDelay &&
        meanFlowDelay <= 0.553292 + toleranceMeanFlowDelay)
    {
        return EXIT_SUCCESS;
    }
    else
    {
        return EXIT_FAILURE;
    }
}
}*/
