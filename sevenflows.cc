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
#include <ns3/three-gpp-ftp-m1-helper.h>
#include <ns3/three-gpp-http-client.h>
#include <ns3/three-gpp-http-helper.h>
#include <ns3/three-gpp-http-server.h>
#include <ns3/three-gpp-http-variables.h>
#include <ns3/traffic-generator-ngmn-ftp-multi.h>
#include <ns3/traffic-generator-ngmn-gaming.h>
#include <ns3/traffic-generator-ngmn-video.h>
#include <ns3/traffic-generator-ngmn-voip.h>



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

Time simTime = Seconds(3.1);
Time tcpAppStartTime = Seconds(1.0);

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
    uint16_t ueNumPergNb = 7;
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
    remoteHostContainer.Create(7);
    Ptr<Node> remoteHost1 = remoteHostContainer.Get(0);
    Ptr<Node> remoteHost2 = remoteHostContainer.Get(1);
    Ptr<Node> remoteHost3 = remoteHostContainer.Get(2);
    Ptr<Node> remoteHost4 = remoteHostContainer.Get(3);
    Ptr<Node> remoteHost5 = remoteHostContainer.Get(4);
    Ptr<Node> remoteHost6 = remoteHostContainer.Get(5);
    Ptr<Node> remoteHost7 = remoteHostContainer.Get(6);
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
    Ptr<ConstantPositionMobilityModel> u4 = ut.Get (3)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> u5 = ut.Get (4)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> u6 = ut.Get (5)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> u7 = ut.Get (6)->GetObject<ConstantPositionMobilityModel> ();

    u1->SetPosition (Vector ( 0.0, 20.0, 0  ));
    u2->SetPosition (Vector ( 0.0, 30.0, 0  ));
    u3->SetPosition (Vector ( 0.0, 40.0, 0  ));
    u4->SetPosition (Vector ( 0.0, 50.0, 0  ));
    u5->SetPosition (Vector ( 0.0, 60.0, 0  ));
    u6->SetPosition (Vector ( 0.0, 70.0, 0  ));
    u7->SetPosition (Vector ( 0.0, 80.0, 0  ));
    
    Ptr<ConstantPositionMobilityModel> g1 = gnb.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    g1->SetPosition (Vector ( 10.0, 50.0, 0  ));
    
    Ptr<ConstantPositionMobilityModel> p1 = pgwcontainer.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    p1->SetPosition (Vector ( 90.0, 50.0, 0  ));
    
    Ptr<ConstantPositionMobilityModel> n1 = remoteHostContainer.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n2 = remoteHostContainer.Get (1)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n3 = remoteHostContainer.Get (2)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n4 = remoteHostContainer.Get (3)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n5 = remoteHostContainer.Get (4)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n6 = remoteHostContainer.Get (5)->GetObject<ConstantPositionMobilityModel> ();
    Ptr<ConstantPositionMobilityModel> n7 = remoteHostContainer.Get (6)->GetObject<ConstantPositionMobilityModel> ();

    n1->SetPosition (Vector ( 100.0, 20.0, 0  ));
    n2->SetPosition (Vector ( 100.0, 30.0, 0  ));
    n3->SetPosition (Vector ( 100.0, 40.0, 0  ));
    n4->SetPosition (Vector ( 100.0, 50.0, 0  ));
    n5->SetPosition (Vector ( 100.0, 60.0, 0  ));
    n6->SetPosition (Vector ( 100.0, 70.0, 0  ));
    n7->SetPosition (Vector ( 100.0, 80.0, 0  ));

    // connect a remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices1 = p2ph.Install(pgw, remoteHost1);
    NetDeviceContainer internetDevices2 = p2ph.Install(pgw, remoteHost2);
    NetDeviceContainer internetDevices3 = p2ph.Install(pgw, remoteHost3);
    NetDeviceContainer internetDevices4 = p2ph.Install(pgw, remoteHost4);
    NetDeviceContainer internetDevices5 = p2ph.Install(pgw, remoteHost5);
    NetDeviceContainer internetDevices6 = p2ph.Install(pgw, remoteHost6);
    NetDeviceContainer internetDevices7 = p2ph.Install(pgw, remoteHost7);


   /* PointToPointHelper p2ph2;
    p2ph2.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph2.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph2.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices2 = p2ph2.Install(pgw, remoteHost2);
    
    PointToPointHelper p2ph3;
    p2ph3.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph3.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph3.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices3 = p2ph3.Install(pgw, remoteHost3);

    PointToPointHelper p2ph4;
    p2ph4.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph4.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph4.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices4 = p2ph4.Install(pgw, remoteHost4);

    PointToPointHelper p2ph5;
    p2ph5.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph5.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph5.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices5 = p2ph5.Install(pgw, remoteHost5);

    PointToPointHelper p2ph6;
    p2ph6.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph6.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph6.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices6 = p2ph6.Install(pgw, remoteHost6);

    PointToPointHelper p2ph7;
    p2ph7.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRateRemoteHostLink)));
    p2ph7.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph7.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    NetDeviceContainer internetDevices7 = p2ph7.Install(pgw, remoteHost7);
    */
    p2ph.EnablePcapAll("R");
    
    NetDeviceContainer internalNet;
    internalNet.Add(enbNetDev);
    internalNet.Add(internetDevices1);
    internalNet.Add(internetDevices2);
    internalNet.Add(internetDevices3);

    QueueDiscContainer queueDiscs;
    TrafficControlHelper tch;   //AQM implementation
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
    pgwRouting->AddHostRouteTo(Ipv4Address("11.0.0.9"), 6);
    pgwRouting->AddHostRouteTo(Ipv4Address("11.0.0.11"), 7);
    pgwRouting->AddHostRouteTo(Ipv4Address("11.0.0.13"), 8);
    pgwRouting->AddHostRouteTo(Ipv4Address("11.0.0.15"), 9);



    Ptr<Ipv4StaticRouting> remoteHostStaticRouting1 = ipv4RoutingHelper.GetStaticRouting(remoteHost1->GetObject<Ipv4>());
    remoteHostStaticRouting1->AddNetworkRouteTo(Ipv4Address("7.0.0.2"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting2 = ipv4RoutingHelper.GetStaticRouting(remoteHost2->GetObject<Ipv4>());
    remoteHostStaticRouting2->AddNetworkRouteTo(Ipv4Address("7.0.0.3"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting3 = ipv4RoutingHelper.GetStaticRouting(remoteHost3->GetObject<Ipv4>());
    remoteHostStaticRouting3->AddNetworkRouteTo(Ipv4Address("7.0.0.4"), Ipv4Mask("255.0.0.0"), 1);

    Ptr<Ipv4StaticRouting> remoteHostStaticRouting4 = ipv4RoutingHelper.GetStaticRouting(remoteHost1->GetObject<Ipv4>());
    remoteHostStaticRouting4->AddNetworkRouteTo(Ipv4Address("7.0.0.5"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting5 = ipv4RoutingHelper.GetStaticRouting(remoteHost2->GetObject<Ipv4>());
    remoteHostStaticRouting5->AddNetworkRouteTo(Ipv4Address("7.0.0.6"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting6 = ipv4RoutingHelper.GetStaticRouting(remoteHost3->GetObject<Ipv4>());
    remoteHostStaticRouting6->AddNetworkRouteTo(Ipv4Address("7.0.0.7"), Ipv4Mask("255.0.0.0"), 1);

    Ptr<Ipv4StaticRouting> remoteHostStaticRouting7 = ipv4RoutingHelper.GetStaticRouting(remoteHost3->GetObject<Ipv4>());
    remoteHostStaticRouting7->AddNetworkRouteTo(Ipv4Address("7.0.0.8"), Ipv4Mask("255.0.0.0"), 1);

    
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

    ApplicationContainer sourceApps;
    sourceApps.Start (tcpAppStartTime);
  	sourceApps.Stop (simTime);

    ApplicationContainer sinkApps;
    sinkApps.Start (tcpAppStartTime);
	sinkApps.Stop (simTime);
    
    OnOffHelper onOffHelper1("ns3::UdpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(0), port));
    onOffHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    onOffHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper1.SetAttribute ("DataRate",StringValue ("100Mbps"));
    onOffHelper1.SetAttribute ("PacketSize",UintegerValue(20)); 
    sourceApps.Add(onOffHelper1.Install(remoteHost1)); 
    PacketSinkHelper sink1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    sinkApps.Add(sink1.Install (ueTrafficNodeContainer.Get(0)));
    
    uint32_t MaxPacketSize = 1024;
    Time interPacketInterval = Seconds (0.05);
    uint32_t maxPacketCount = 320;
    UdpClientHelper udpclient2;
    udpclient2.SetAttribute("RemotePort", UintegerValue(port));
    udpclient2.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpclient2.SetAttribute("RemoteAddress", AddressValue(ueTrafficIpIfaceContainer.GetAddress(1)));
    udpclient2.SetAttribute ("Interval", TimeValue (interPacketInterval));
    udpclient2.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
    sourceApps.Add(udpclient2.Install (remoteHost2));
    UdpServerHelper sink2(port);
    sinkApps.Add(sink2.Install (ueTrafficNodeContainer.Get(1)));


    BulkSendHelper source3 ("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(2), port));
  	// Set the amount of data to send in bytes.  Zero is unlimited.
  	source3.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    sourceApps.Add(source3.Install (remoteHost3));
    PacketSinkHelper sink3 ("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(2), port));
  	sinkApps.Add(sink3.Install (ueTrafficNodeContainer.Get(2)));
  	
  	//http, voip, gaming, video, ftpmultiple, 

    uint32_t portRemoteHost = 49153;
    ThreeGppHttpServerHelper httpServer(InetSocketAddress (Ipv4Address("11.0.0.9"), portRemoteHost));
    sinkApps.Add(httpServer.Install(remoteHost4));
  	ThreeGppHttpClientHelper httpClient4(InetSocketAddress (Ipv4Address("11.0.0.9"), portRemoteHost));
    sourceApps.Add(httpClient4.Install(ueTrafficNodeContainer.Get(3)));

/*
    std::string transportProtocol = "ns3::TcpSocketFactory";
    TrafficGeneratorHelper trafficGeneratorHelper(transportProtocol,
                                                          Address(),
                                                          TrafficGeneratorNgmnVoip::GetTypeId());
    trafficGeneratorHelper.SetAttribute("EncoderFrameLength", UintegerValue(20));
    trafficGeneratorHelper.SetAttribute("MeanTalkSpurtDuration", UintegerValue(2000));
    trafficGeneratorHelper.SetAttribute("VoiceActivityFactor", DoubleValue(0.5));
    trafficGeneratorHelper.SetAttribute("VoicePayload", UintegerValue(40));
    trafficGeneratorHelper.SetAttribute("SIDPeriodicity", UintegerValue(160));
    trafficGeneratorHelper.SetAttribute("SIDPayload", UintegerValue(15));
    trafficGeneratorHelper.SetAttribute("Remote",AddressValue(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(4),port)));
    sourceApps.Add(trafficGeneratorHelper.Install(remoteHost5));
    PacketSinkHelper packetSinkHelper5(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(4), port) );
    sinkApps.Add(packetSinkHelper5.Install(ueTrafficNodeContainer.Get(4)));


    TrafficGeneratorHelper trafficGeneratorHelperg(transportProtocol,
                                                          Address(),
                                                          TrafficGeneratorNgmnGaming::GetTypeId());
    trafficGeneratorHelperg.SetAttribute("IsDownlink", BooleanValue(true));
    trafficGeneratorHelperg.SetAttribute("aParamPacketSizeDl", UintegerValue(120));
    trafficGeneratorHelperg.SetAttribute("bParamPacketSizeDl", DoubleValue(36));
    trafficGeneratorHelperg.SetAttribute("aParamPacketArrivalDl", DoubleValue(45));
    trafficGeneratorHelperg.SetAttribute("bParamPacketArrivalDl", DoubleValue(5.7));
    trafficGeneratorHelperg.SetAttribute("InitialPacketArrivalMin", UintegerValue(0));
    trafficGeneratorHelperg.SetAttribute("InitialPacketArrivalMax", UintegerValue(40));
  	trafficGeneratorHelperg.SetAttribute("Remote",AddressValue(InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(5), port)));
    sourceApps.Add(trafficGeneratorHelperg.Install(remoteHost6));
    PacketSinkHelper packetSinkHelper6(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(5), port) );
    sinkApps.Add(packetSinkHelper6.Install(ueTrafficNodeContainer.Get(5)));


    TrafficGeneratorHelper trafficGeneratorHelperg(transportProtocol,
                                                          Address(),
                                                          TrafficGeneratorNgmnVideo::GetTypeId());
    trafficGeneratorHelper.SetAttribute("NumberOfPacketsInFrame", UintegerValue(8));
    trafficGeneratorHelper.SetAttribute("InterframeIntervalTime",
                                                TimeValue(Seconds(0.100)));
  	trafficGeneratorHelperg.SetAttribute("Remote",AddressValue(InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(6), port)));
    sourceApps.Add(trafficGeneratorHelperg.Install(remoteHost7));
    PacketSinkHelper packetSinkHelper7(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(6), port) );
    sinkApps.Add(packetSinkHelper7.Install(ueTrafficNodeContainer.Get(6)));



///Put this in place of onoff
    TrafficGeneratorHelper ftpHelper(transportProtocol,
                                             Address(),
                                             TrafficGeneratorNgmnFtpMulti::GetTypeId());
    ftpHelper.SetAttribute("PacketSize", UintegerValue(1448));
    ftpHelper.SetAttribute("MaxFileSize", UintegerValue(5e6));
    trafficGeneratorHelperg.SetAttribute("Remote",AddressValue(InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(0), port)));
    sourceApps.Add(trafficGeneratorHelperg.Install(remoteHost1));
    PacketSinkHelper packetSinkHelper1(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(0), port) );
    sinkApps.Add(packetSinkHelper1.Install(ueTrafficNodeContainer.Get(0)));


  	*/
  	
  	
  	

	// The sink will always listen to the specified ports
	
	
	//PacketSinkHelper sink2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
	
	
	
	
	
	
	
	  
	  
	  
	  
    // The filter for the voice tcp traffic
    Ptr<EpcTft> voiceTft = Create<EpcTft>();
    EpcTft::PacketFilter dlpfVoice;
    dlpfVoice.localPortStart = port;
    dlpfVoice.localPortEnd = port;
    voiceTft->Add(dlpfVoice);
    
    
    nrHelper->ActivateDedicatedEpsBearer(ueTrafficNetDevContainer, voiceBearer, voiceTft);


    nrHelper->EnableTraces();

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost1);
    endpointNodes.Add(remoteHost2);
    endpointNodes.Add(remoteHost3);
    endpointNodes.Add(remoteHost4);
    endpointNodes.Add(remoteHost5);
    endpointNodes.Add(remoteHost6);
    endpointNodes.Add(remoteHost7);

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




