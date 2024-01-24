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
#include "ns3/nr-point-to-point-epc-helper.h"
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

NS_LOG_COMPONENT_DEFINE("sevenflows");

int
main(int argc, char* argv[])
{


/////////////////////////////////////////////////////////

uint32_t maxBytes =100000;  //tcp bulk sender
std::string dataRateRemoteHostLink = "10Gbps";
std::string coreBandwidth = "8Gb/s";   //BottleNeck S1ULink between gnb and sgw
std::string udpDataRate = "1000Kbps";
double bandwidthBand1 = 6000e6;  //6000MHz  Mbps = MHz * 8 ->  48Gbps

Time delayRemoteHostLink = MilliSeconds(10);
Time coreLatency = MilliSeconds(0.1);

Time simTime = Seconds(3.11);
Time tcpAppStartTime = Seconds(1.0);

std::string simTag = "SimResults.txt";
std::string outputDir = "./";

										// SDAP    QoS Flows
										//   |
								    		// PDCP    Radio Bearer
										//   |
										//  RLC  => RLC Channels (Buffer)   
										//   |
										//  MAC
										//   |
										//  PHY


//RLC Buffer
Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(1000000000)); //RLC Unacknowledged Mode (UM),

//AQM Algorithm
std::string aqmAlgo = "ns3::PieQueueDisc";

// CoDel
// FqCoDel
// Pie
// FqPie
// Red
// Cobalt
// FqCobalt
// Fifo
// PfifoFast
// Tbf
// Config::SetDefault("ns3::RedQueueDisc::ARED", BooleanValue(true)); , NLRED


uint16_t numerologyBwp1 = 5;
double centralFrequencyBand1 = 28e9;  //28 GHz  
double totalTxPower = 4; //dBm

std::string transportProtocol = "ns3::TcpSocketFactory";

/////////////////////////////////////////////////////////




    


  /*

	ue1                                    R1
	   \                                  /
	ue2 \                                / R2
           \ \______________________________/ /
	ue3 \________________________________/ R3    
	   \__________________________________/
	ue4____________________________________R4
	    __________________________________
	   / ________________________________ \
	ue5 / ______________________________ \ R5
	   / /                              \ \
	ue6 /                                \ R6
	   /                                  \ 
	ue7                                    R7


  */
  
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
        { //outFile<<ue<<"\n";
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
    const uint8_t numCcPerBand = 1; 

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
    epcHelper->SetAttribute("S1uLinkDataRate", DataRateValue(DataRate(coreBandwidth)));
    

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

    //uint32_t bwpId = 0;
    

    // gNb routing between Bearer and bandwidh part
    //nrHelper->SetGnbBwpManagerAlgorithmAttribute(bwpTraffic, UintegerValue(bwpId));

    //NGBR_VIDEO_TCP_DEFAULT
    // Ue routing between Bearer and bandwidth part
    //nrHelper->SetUeBwpManagerAlgorithmAttribute(bwpTraffic, UintegerValue(bwpId));

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
    Ptr<Node> sgw = epcHelper->GetSgwNode();
    Ptr<Node> gnbptr=gridScenario.GetBaseStations().Get(0);
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
    NodeContainer sgwcontainer = epcHelper->GetSgwNode();
    
    
    
    
    
    
    
    ///////////////////////////////////////////// MOBILITY POSITIONS /////////////////////////////////////////////
    MobilityHelper mobility;
    
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(remoteHostContainer);
    mobility.Install(gnb);
    //mobility.Install(ut);
    mobility.Install(pgwcontainer);
    mobility.Install(sgwcontainer);
    
    MobilityHelper mobility1;
    mobility1.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX", DoubleValue (0.0),
    "MinY", DoubleValue (20.0),
    "DeltaX", DoubleValue (2.0),
    "DeltaY", DoubleValue (10.0),
    "GridWidth", UintegerValue (1),
    "LayoutType", StringValue ("RowFirst"));

     mobility1.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
     "Bounds", RectangleValue (Rectangle (0, 20, 0, 100)));

    mobility1.Install(ut);
    
    
    //double hBS = 10;
    //double hUT = 1.5;
    //double speed = 1;             // in m/s for walking UT.
      
    
    Ptr<ConstantPositionMobilityModel> g1 = gnb.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    g1->SetPosition (Vector ( 10.0, 50.0, 0  ));
    
    Ptr<ConstantPositionMobilityModel> s1 = sgwcontainer.Get (0)->GetObject<ConstantPositionMobilityModel> ();
    s1->SetPosition (Vector ( 50.0, 50.0, 0  ));
    
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
    
    
    
    
    
    
    
    
    
    
    
    ////////////////////////////////////////// NETWORK /////////////////////////////////////////////////////////////

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

    // PointtoPoint connection pgw --->sgw
    //PointToPointHelper p2p;
    //p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(dataRatebottleneckLink)));
    //p2p.SetDeviceAttribute("Mtu", UintegerValue(2500));
    //p2p.SetChannelAttribute("Delay", TimeValue(delayRemoteHostLink));
    //NetDeviceContainer gatewayNetdevices=p2p.Install(sgw,pgw);
    //NetDeviceContainer gatewayNetdevices2=p2p.Install(sgw,gnbptr);
    
    
    //p2ph.EnablePcapAll("C");
    
    NetDeviceContainer internalNet;
    internalNet.Add(enbNetDev);
    internalNet.Add(internetDevices1);
    internalNet.Add(internetDevices2);
    internalNet.Add(internetDevices3);
    internalNet.Add(internetDevices4);
    internalNet.Add(internetDevices5);
    internalNet.Add(internetDevices6);
    internalNet.Add(internetDevices7);
    //internalNet.Add(gatewayNetdevices);
    //internalNet.Add(gatewayNetdevices2);
    QueueDiscContainer queueDiscs;
    TrafficControlHelper tch;   //AQM implementation
    tch.SetRootQueueDisc (aqmAlgo);
     queueDiscs = tch.Install(internalNet); 
    //AnimationInterface anim("working.xml");

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

    Ptr<Ipv4StaticRouting> remoteHostStaticRouting4 = ipv4RoutingHelper.GetStaticRouting(remoteHost4->GetObject<Ipv4>());
    remoteHostStaticRouting4->AddNetworkRouteTo(Ipv4Address("7.0.0.5"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting5 = ipv4RoutingHelper.GetStaticRouting(remoteHost5->GetObject<Ipv4>());
    remoteHostStaticRouting5->AddNetworkRouteTo(Ipv4Address("7.0.0.6"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting6 = ipv4RoutingHelper.GetStaticRouting(remoteHost6->GetObject<Ipv4>());
    remoteHostStaticRouting6->AddNetworkRouteTo(Ipv4Address("7.0.0.7"), Ipv4Mask("255.0.0.0"), 1);

    Ptr<Ipv4StaticRouting> remoteHostStaticRouting7 = ipv4RoutingHelper.GetStaticRouting(remoteHost7->GetObject<Ipv4>());
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
    












///////////////////////////////////////////////// TRAFFIC /////////////////////////////////////////////////////



    //Add traffic

    uint16_t port = 9;

    ApplicationContainer sourceApps;
    sourceApps.Start (tcpAppStartTime);
    sourceApps.Stop (simTime);

    ApplicationContainer sinkApps;
    sinkApps.Start (tcpAppStartTime);
    sinkApps.Stop (simTime);
    
    
    //__________________________________________ 1. FTP _________________________________________________
    
    
    TrafficGeneratorHelper ftpHelper(transportProtocol,
                                             Address(),
                                             TrafficGeneratorNgmnFtpMulti::GetTypeId());
    ftpHelper.SetAttribute("PacketSize", UintegerValue(1448));
    ftpHelper.SetAttribute("MaxFileSize", UintegerValue(5e6));//5e6
    ftpHelper.SetAttribute("Remote",AddressValue(InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(0), 20)));
    sourceApps.Add(ftpHelper.Install(remoteHost1));
    PacketSinkHelper packetSinkHelper10(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(0), 20) );
    sinkApps.Add(packetSinkHelper10.Install(ueTrafficNodeContainer.Get(0)));
    
    
    
    //_________________________________________ 2. UDP OnOff ____________________________________________
    
    OnOffHelper onOffHelper1("ns3::UdpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(1), port));
    onOffHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=10]"));
    onOffHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper1.SetAttribute ("DataRate",StringValue (udpDataRate));
    onOffHelper1.SetAttribute ("PacketSize",UintegerValue(20)); 
    sourceApps.Add(onOffHelper1.Install(remoteHost2)); 
    PacketSinkHelper sink1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    sinkApps.Add(sink1.Install (ueTrafficNodeContainer.Get(1))); 
    
    /*
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
    sinkApps.Add(sink2.Install (ueTrafficNodeContainer.Get(1)));*/


    //____________________________________ 3. TCP BULK ______________________________________________________


    BulkSendHelper source3 ("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(2), port));
  	// Set the amount of data to send in bytes.  Zero is unlimited.
    source3.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    source3.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    sourceApps.Add(source3.Install (remoteHost3));
    PacketSinkHelper sink3 ("ns3::TcpSocketFactory", InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(2), port));
    sinkApps.Add(sink3.Install (ueTrafficNodeContainer.Get(2)));
  	
  	
  	
  
   //_____________________________________ 4. HTTP _____________________________________________________________
  	
    ThreeGppHttpServerHelper httpServer4(InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(3), port));
    sourceApps.Add(httpServer4.Install(remoteHost4));
    Ptr<ThreeGppHttpServer> httpServer = sourceApps.Get(3)->GetObject<ThreeGppHttpServer>();
    PointerValue varPtr;
    httpServer->GetAttribute("Variables", varPtr);
    Ptr<ThreeGppHttpVariables> httpVariables = varPtr.Get<ThreeGppHttpVariables>();
    httpVariables->SetMainObjectSizeMean(102400);  // 100kB
    httpVariables->SetMainObjectSizeStdDev(40960); // 40kB
  	
    ThreeGppHttpClientHelper httpClient4(Ipv4Address("11.0.0.9"));
    sinkApps.Add(httpClient4.Install(ueTrafficNodeContainer.Get(3)));
    
    
    

    //______________________________________ 5. VoIP _________________________________________________________

    
    TrafficGeneratorHelper trafficGeneratorHelper("ns3::UdpSocketFactory",
                                                          Address(),
                                                          TrafficGeneratorNgmnVoip::GetTypeId());
    trafficGeneratorHelper.SetAttribute("EncoderFrameLength", UintegerValue(20));
    trafficGeneratorHelper.SetAttribute("MeanTalkSpurtDuration", UintegerValue(2000));
    trafficGeneratorHelper.SetAttribute("VoiceActivityFactor", DoubleValue(0.5));
    trafficGeneratorHelper.SetAttribute("VoicePayload", UintegerValue(40));
    trafficGeneratorHelper.SetAttribute("SIDPeriodicity", UintegerValue(160));
    trafficGeneratorHelper.SetAttribute("SIDPayload", UintegerValue(15));    trafficGeneratorHelper.SetAttribute("Remote",AddressValue(InetSocketAddress(ueTrafficIpIfaceContainer.GetAddress(4),port)));
    sourceApps.Add(trafficGeneratorHelper.Install(remoteHost5));
    PacketSinkHelper packetSinkHelper5(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(4), port) );
    sinkApps.Add(packetSinkHelper5.Install(ueTrafficNodeContainer.Get(4)));



    //______________________________________ 6. Gaming ___________________________________________________________


    TrafficGeneratorHelper trafficGeneratorHelper1(transportProtocol,
                                                          Address(),
                                                          TrafficGeneratorNgmnGaming::GetTypeId());
    trafficGeneratorHelper1.SetAttribute("IsDownlink", BooleanValue(true));
    trafficGeneratorHelper1.SetAttribute("aParamPacketSizeDl", UintegerValue(120));
    trafficGeneratorHelper1.SetAttribute("bParamPacketSizeDl", DoubleValue(36));
    trafficGeneratorHelper1.SetAttribute("aParamPacketArrivalDl", DoubleValue(45));
    trafficGeneratorHelper1.SetAttribute("bParamPacketArrivalDl", DoubleValue(5.7));
    trafficGeneratorHelper1.SetAttribute("InitialPacketArrivalMin", UintegerValue(0));
    trafficGeneratorHelper1.SetAttribute("InitialPacketArrivalMax", UintegerValue(40));
    trafficGeneratorHelper1.SetAttribute("Remote",AddressValue(InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(5), port)));
    sourceApps.Add(trafficGeneratorHelper1.Install(remoteHost6));
    PacketSinkHelper packetSinkHelper6(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(5), port) );
    sinkApps.Add(packetSinkHelper6.Install(ueTrafficNodeContainer.Get(5)));




    //____________________________________________ 7. Video ____________________________________________________
    
    TrafficGeneratorHelper trafficGeneratorHelper2(transportProtocol,
                                                          Address(),
                                                          TrafficGeneratorNgmnVideo::GetTypeId());
    trafficGeneratorHelper2.SetAttribute("NumberOfPacketsInFrame", UintegerValue(8));
    trafficGeneratorHelper2.SetAttribute("InterframeIntervalTime",
                                                TimeValue(Seconds(0.100)));
  	trafficGeneratorHelper2.SetAttribute("Remote",AddressValue(InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(6), port)));
    sourceApps.Add(trafficGeneratorHelper2.Install(remoteHost7));
    PacketSinkHelper packetSinkHelper7(transportProtocol,InetSocketAddress (ueTrafficIpIfaceContainer.GetAddress(6), port) );
    sinkApps.Add(packetSinkHelper7.Install(ueTrafficNodeContainer.Get(6)));

	
	  
	  
	  
	  
    // The filter for the traffic
    Ptr<EpcTft> filterTft = Create<EpcTft>();
    EpcTft::PacketFilter dlpf;
    //dlpfVoice.localPortStart = port;
    //dlpfVoice.localPortEnd = port;
    filterTft->Add(dlpf);
    
    EpsBearer gamingBearer(EpsBearer::GBR_GAMING);
    EpsBearer voiceBearer(EpsBearer::GBR_CONV_VOICE);
    EpsBearer videoBearer(EpsBearer::GBR_CONV_VIDEO);
    
    nrHelper->ActivateDedicatedEpsBearer(ueTrafficNetDevContainer.Get(5), gamingBearer, filterTft);
    nrHelper->ActivateDedicatedEpsBearer(ueTrafficNetDevContainer.Get(4), voiceBearer, filterTft);
    nrHelper->ActivateDedicatedEpsBearer(ueTrafficNetDevContainer.Get(6), videoBearer, filterTft);







//////////////////////////////////////////////////// TRACING FLOW //////////////////////////////////////////////


    //nrHelper->EnableTraces();

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
            // Measure the duration of the flow from receiver's perspective
            averageFlowThroughput += i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000
                    << " Mbps\n";
            std::cout << "  Mean delay:  "
                    << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
            // outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << "
            // Mbps \n";
            std::cout << "  Mean jitter:  "
                    << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
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




