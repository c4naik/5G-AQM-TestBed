#include "ns3/stats-module.h"
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
#include <ns3/three-gpp-ftp-m1-helper.h>
#include <ns3/three-gpp-http-client.h>
#include <ns3/three-gpp-http-helper.h>
#include <ns3/three-gpp-http-server.h>
#include <ns3/three-gpp-http-variables.h>
#include <ns3/traffic-generator-ngmn-ftp-multi.h>
#include <ns3/traffic-generator-ngmn-gaming.h>
#include <ns3/traffic-generator-ngmn-video.h>
#include <ns3/traffic-generator-ngmn-voip.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "ns3/dash-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/traffic-control-module.h"
//#include "eval-ts.h"
#include <utility> 
#include "ns3/delay-jitter-estimation.h"
using namespace ns3;

std::string aqm ;

class EvalTimestampTag : public Tag
{
public:
  /**
   * \brief Constructor
   */
  EvalTimestampTag ();

  /**
   * \brief Get the type ID
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);


  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

  /**
   * \brief Gets the Tag creation time
   * \return the time object stored in the tag
   */
  Time GetTxTime (void) const;

private:
  uint64_t m_creationTime;      //!< Tag creation time
};

EvalTimestampTag::EvalTimestampTag ()
  : m_creationTime (Simulator::Now ().GetTimeStep ())
{
}

TypeId
EvalTimestampTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EvalTimestampTag")
    .SetParent<Tag> ()
    .AddConstructor<EvalTimestampTag> ()
    .AddAttribute ("CreationTime",
                   "The time at which the timestamp was created",
                   StringValue ("0.0s"),
                   MakeTimeAccessor (&EvalTimestampTag::GetTxTime),
                   MakeTimeChecker ())
  ;
  return tid;
}

TypeId
EvalTimestampTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
EvalTimestampTag::GetSerializedSize (void) const
{
  return 8;
}

void
EvalTimestampTag::Serialize (TagBuffer i) const
{
  i.WriteU64 (m_creationTime);
}

void
EvalTimestampTag::Deserialize (TagBuffer i)
{
  m_creationTime = i.ReadU64 ();
}

void
EvalTimestampTag::Print (std::ostream &os) const
{
  os << "CreationTime=" << m_creationTime;
}

Time
EvalTimestampTag::GetTxTime (void) const
{
  return TimeStep (m_creationTime);
}


int bytecounter;
uint32_t period = 1;
int checkTimes=0;
int32_t  avgQueueDiscSize=0;

AsciiTraceHelper asciiTraceHelper1;
Ptr<OutputStreamWrapper> th; 
Ptr<OutputStreamWrapper> gp ;

void CheckQueueDiscSize(Ptr<QueueDisc> queue)
{
    int32_t  qSize = queue->GetCurrentSize().GetValue();
    avgQueueDiscSize = avgQueueDiscSize + qSize;
    checkTimes = checkTimes + 1;
    //check queue disc size every 1/100 of a second
    ns3::Simulator::Schedule(ns3::Seconds(0.01),CheckQueueDiscSize, queue);
    std::cout << Simulator::Now ().GetSeconds () << "\t" << qSize << "\t" << std::endl;
    std::cout << Simulator::Now ().GetSeconds () << "\t" << avgQueueDiscSize / checkTimes << std::endl;
    return;
}
void VoipTx(Ptr<OutputStreamWrapper> stream ,std::string context, ns3::Ptr<ns3::Packet const>packet)
{
    // DelayJitterEstimation Dj;
    DelayJitterEstimation::PrepareTx(packet);
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t"<< packet->GetSize() <<" " << packet->GetUid() << std::endl; 
    return;
}
void VoipRx(Ptr<OutputStreamWrapper> stream ,std::string context, ns3::Ptr<ns3::Packet const>packet, const Address &address)
{
    static DelayJitterEstimation Dj;
    Dj.RecordRx(packet);
    //int delay = Dj.GetLastDelay().GetSeconds();
    float jitter=Dj.GetLastJitter()/(float)1000000;
    *stream->GetStream()<<Simulator::Now().GetSeconds()<< "\t" <<jitter  <<std::endl;
    return;
}



void UdpTxTrace(Ptr<OutputStreamWrapper> stream,Ptr<const Packet> packet)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << packet->GetSize ()  << std::endl;
    return;
}
void
NotifyPacketTx (Ptr<OutputStreamWrapper> stream ,std::string context, ns3::Ptr<ns3::Packet const>packet)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t"<< packet->GetSize() <<" " << std::endl; 
    return;
}
void
NotifyPacketRx (Ptr<OutputStreamWrapper> stream,std::string context, ns3::Ptr<ns3::Packet const>packet, const Address &address)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t"  << packet->GetSize() << "\t" << packet->GetUid() << std::endl; 
    return;
}


void
NotifyHttpTx(Ptr<OutputStreamWrapper> stream,std::string context,Ptr< Packet const> packet)
{
    TimestampTag timestamp;
    timestamp.SetTimestamp (Simulator::Now ());
    packet->AddByteTag (timestamp);
    *stream->GetStream () << Simulator::Now().GetSeconds() << "\t" << packet->GetUid()<< std::endl;
    return;
}
void
NotifyHttpRx(Ptr<OutputStreamWrapper> stream,std::string context,Ptr< Packet const> packet,const Address &from)
{   

    TimestampTag timestamp;
    Time transmit_time;
    if (packet->FindFirstMatchingByteTag (timestamp))           
    {
        transmit_time = timestamp.GetTimestamp ();
    }
    
    *stream->GetStream () << Simulator::Now().GetSeconds() << "\t" << packet->GetUid()<< "\t Transmit time->"<< transmit_time.GetSeconds()<<std::endl;
    return;
}

void
NotifyBulkTx(Ptr<OutputStreamWrapper> stream,std::string context,Ptr< const Packet > packet)
{
    *stream->GetStream () << Simulator::Now().GetSeconds() << "\t" << packet->GetUid()<<std::endl;
    return;
}

//  void
//  PacketEnqueue (Ptr<OutputStreamWrapper> stream,std::string context,Ptr<const Packet> packet)
//  {
//     *stream->GetStream () << Simulator::Now ().GetSeconds () <<"Packet enqueued " << packet->GetUid()<< std::endl;
//  }

//  void
//  PacketDrop (Ptr<OutputStreamWrapper> stream,ns3::Ptr<ns3::QueueDiscItem const>q)
//  {
//     Ptr<const Ipv4QueueDiscItem> iqdi = Ptr<const Ipv4QueueDiscItem> (dynamic_cast<const Ipv4QueueDiscItem *> (PeekPointer (q)));
//    *stream->GetStream () << (iqdi->GetHeader ()).GetDestination ()
//                              << " "
//                              << Simulator::Now ().GetSeconds ()
//                              << "\n";

//     *stream->GetStream () << Simulator::Now ().GetSeconds () <<" Packet Dropped " << std::endl;
//  }

 void 
 Queuesize(Ptr<OutputStreamWrapper> stream,std::string context,uint32_t oldValue, uint32_t newValue)
 {
     *stream->GetStream () << "old size " << oldValue << " new size "<< newValue << std::endl;
 }

void
 IpRxTrace (std::string context, Ptr< const Packet > packet, Ptr< Ipv4 > ipv4, uint32_t interface)
{   
    bytecounter =+ packet->GetSize(); // the size in bytes
    std::cout << "IpRxTrace (bytes/sec) = " << bytecounter;
}

void TcpPacketReceivedCallback (Ptr<OutputStreamWrapper> stream,Ptr<const Packet> packet, const Address& from, const Address& to)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t"<< packet->GetSize() <<"\t" << packet->GetUid() << std::endl; 
}
void Throughput()
{
//  int packetSize=512;
  double  throughput = bytecounter/ period;
  std::cout << " ----> Throughput (bytes/sec) = " << throughput << std::endl;
  Simulator::Schedule(Seconds(period), &Throughput);
}
void OnDataReceivedCallback(Ptr<OutputStreamWrapper> stream,std::string context,uint32_t dataSize)
{
    ns3::Time currentTime = ns3::Simulator::Now(); // Get the current simulation time
    *stream->GetStream () <<  "At " << currentTime.GetSeconds() << "s, received data chunk of size: " << dataSize << " bytes" << std::endl;
}
void NotifypacketDrop(Ptr<OutputStreamWrapper> stream,std::string context,Ptr<const Packet> packet)
{
    *stream->GetStream () << Simulator::Now().GetSeconds() << " Packet Dropped " << packet->GetUid() << "\t" << packet->GetSize() << std::endl;
}
void GetSojourntime( Ptr<OutputStreamWrapper> stream,std::string context,Time value)
{
    *stream->GetStream () << Simulator::Now().GetSeconds() << "\t" << value << std::endl;
}
void
NotifyPacketRxDash (Ptr<OutputStreamWrapper> stream,std::string context, ns3::Ptr<ns3::Packet const>packet, ns3::Ptr<ns3::Ipv4> ptr,unsigned int a)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t"  << packet->GetSize() << "\t" << packet->GetUid() << std::endl; 
    return;
}
void NotifyBulkTxDash(Ptr<OutputStreamWrapper> stream,std::string context,Ptr< const Packet > packet,ns3::Ptr<ns3::Ipv4> ptr,unsigned int a)
{
    *stream->GetStream () << Simulator::Now().GetSeconds() << "\t" << packet->GetUid()<<std::endl;
    return;
}

// QUEUEING DELAY TRACECALLBACKS 

uint64_t m_QDrecord1=0;                            
uint64_t m_numQDrecord1=0;                         
Time m_lastQDrecord1= Time::Min ();
void
PacketEnqueue (Ptr<const QueueDiscItem> item)
{
  Ptr<Packet> p = item->GetPacket ();
  EvalTimestampTag tag;
  p->AddPacketTag (tag);
  Ptr<const Ipv4QueueDiscItem> iqdi = Ptr<const Ipv4QueueDiscItem> (dynamic_cast<const Ipv4QueueDiscItem *> (PeekPointer (item)));
  std::cout << (iqdi->GetHeader ()).GetDestination ()
                               << " "
                               << Simulator::Now ().GetSeconds ()
                               << "\n";
}

void
PacketDequeue (Ptr<OutputStreamWrapper> stream,Ptr<const QueueDiscItem> item)
{
  Ptr<Packet> p = item->GetPacket ();
  EvalTimestampTag tag;
  p->RemovePacketTag (tag);
  std::cout << "Tx" << tag.GetTxTime().GetSeconds() ;
  std::cout << " Rx" << Simulator::Now ().GetSeconds() << std::endl; 
  Time delta = Simulator::Now () - tag.GetTxTime ();
  if (m_lastQDrecord1 == Time::Min () || Simulator::Now () - m_lastQDrecord1 > MilliSeconds (10))
    {
      m_lastQDrecord1 = Simulator::Now ();
      if (m_numQDrecord1 > 0)
        {
           *stream->GetStream () << Simulator::Now ().GetSeconds ()
                                  << " "
                                  << ((m_QDrecord1 * 1.0) / (m_numQDrecord1 * 1.0)) - 5
                                  << "\n";
        }
      m_QDrecord1 = 0;
      m_numQDrecord1 = 0;
    }
  m_numQDrecord1++;
  m_QDrecord1 += delta.GetMilliSeconds ();
}

void PacketDrop(Ptr<OutputStreamWrapper> stream,Ptr<const QueueDiscItem> item)
{
  Ptr<const Ipv4QueueDiscItem> iqdi = Ptr<const Ipv4QueueDiscItem> (dynamic_cast<const Ipv4QueueDiscItem *> (PeekPointer (item)));
  *stream->GetStream () << (iqdi->GetHeader ()).GetDestination ()
                            << " "
                            << Simulator::Now ().GetSeconds ()
                            << "\n";
}

uint64_t m_TPrecord1 = 0, m_TPrecord2 = 0, m_TPrecord3 = 0, m_TPrecord4 = 0,
         m_TPrecord5 = 0, m_TPrecord6 = 0, m_TPrecord7 = 0, m_TPrecord8 = 0;

Time m_lastTPrecord1 = Time::Min(), m_lastTPrecord2 = Time::Min(),
     m_lastTPrecord3 = Time::Min(), m_lastTPrecord4 = Time::Min(),
     m_lastTPrecord5 = Time::Min(), m_lastTPrecord6 = Time::Min(),
     m_lastTPrecord7 = Time::Min(), m_lastTPrecord8 = Time::Min();

// Function template for tracking payload size and throughput for a specific flow
void PayloadSize(Ptr<OutputStreamWrapper> stream1, Ptr<OutputStreamWrapper> stream2,
                 Ptr<const Packet> packet, const Address &address,
                 uint64_t &m_TPrecord, Time &m_lastTPrecord) {
    *stream1->GetStream() << address
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord == Time::Min() || Simulator::Now() - m_lastTPrecord > MilliSeconds(10)) {
        if (m_TPrecord > 0) {
            *stream2->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord * 1.0) / (Simulator::Now() - m_lastTPrecord).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord = Simulator::Now();
        m_TPrecord = 0;
    }
    m_TPrecord += packet->GetSize();
}

// Individual functions for each flow, calling the template with appropriate variables
void PayloadSize1(Ptr<const Packet> packet, const Address &address) {
     *gp->GetStream() << "8"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord1 == Time::Min() || Simulator::Now() - m_lastTPrecord1 > MilliSeconds(10)) {
        if (m_TPrecord1 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord1 * 1.0) / (Simulator::Now() - m_lastTPrecord1).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord1 = Simulator::Now();
        m_TPrecord1 = 0;
    }
    m_TPrecord1 += packet->GetSize();
}

void PayloadSize2( Ptr<const Packet> packet, const Address &address) {
     *gp->GetStream() << "1"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord2 == Time::Min() || Simulator::Now() - m_lastTPrecord2 > MilliSeconds(10)) {
        if (m_TPrecord2 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord2 * 1.0) / (Simulator::Now() - m_lastTPrecord2).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord2 = Simulator::Now();
        m_TPrecord2 = 0;
    }
    m_TPrecord2 += packet->GetSize();
}

void PayloadSize3( Ptr<const Packet> packet, const Address &address) {
     *gp->GetStream() << "2"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord3 == Time::Min() || Simulator::Now() - m_lastTPrecord3 > MilliSeconds(10)) {
        if (m_TPrecord3 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord3 * 1.0) / (Simulator::Now() - m_lastTPrecord3).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord3 = Simulator::Now();
        m_TPrecord3 = 0;
    }
    m_TPrecord3 += packet->GetSize();
}

void PayloadSize4( Ptr<const Packet> packet, const Address &address) {
     *gp->GetStream() << "3"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord4 == Time::Min() || Simulator::Now() - m_lastTPrecord4 > MilliSeconds(10)) {
        if (m_TPrecord4 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord4 * 1.0) / (Simulator::Now() - m_lastTPrecord4).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord4 = Simulator::Now();
        m_TPrecord4 = 0;
    }
    m_TPrecord4 += packet->GetSize();
}

void PayloadSize5( Ptr<const Packet> packet, const Address &address) {
     *gp->GetStream() << "4"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord5 == Time::Min() || Simulator::Now() - m_lastTPrecord5 > MilliSeconds(10)) {
        if (m_TPrecord5 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord5 * 1.0) / (Simulator::Now() - m_lastTPrecord5).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord5 = Simulator::Now();
        m_TPrecord5 = 0;
    }
    m_TPrecord5 += packet->GetSize();
}
void PayloadSize6( Ptr<const Packet> packet, const Address &address) {
     *gp->GetStream() << "5"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord6 == Time::Min() || Simulator::Now() - m_lastTPrecord6 > MilliSeconds(10)) {
        if (m_TPrecord6 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord6 * 1.0) / (Simulator::Now() - m_lastTPrecord6).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord6 = Simulator::Now();
        m_TPrecord6 = 0;
    }
    m_TPrecord6 += packet->GetSize();
}

void PayloadSize7( Ptr<const Packet> packet, const Address &address) {
     *gp->GetStream() << "6"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord7 == Time::Min() || Simulator::Now() - m_lastTPrecord7 > MilliSeconds(10)) {
        if (m_TPrecord7 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord7 * 1.0) / (Simulator::Now() - m_lastTPrecord7).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord7 = Simulator::Now();
        m_TPrecord7 = 0;
    }
    m_TPrecord7 += packet->GetSize();
}

void PayloadSize8( Ptr<const Packet>packet, Ptr< Ipv4>ipv4, uint32_t interface) {
    if ( packet != nullptr && ipv4 != nullptr && interface < ipv4->GetNInterfaces()) 
    {
        //ns3::Ipv4Address ipv4Address = ipv4->GetAddress(interface, 0).GetLocal();
        //ns3::Address address = ns3::Address(ipv4Address);
     *gp->GetStream() << "7"
                          << " "
                          << Simulator::Now().GetSeconds()
                          << " "
                          << packet->GetSize()
                          << "\n";
    if (m_lastTPrecord8 == Time::Min() || Simulator::Now() - m_lastTPrecord8 > MilliSeconds(10)) {
        if (m_TPrecord8 > 0) {
            *th->GetStream() << Simulator::Now().GetSeconds()
                                  << " "
                                  << (m_TPrecord8 * 1.0) / (Simulator::Now() - m_lastTPrecord8).GetSeconds()
                                  << "\n";
        }
        m_lastTPrecord8 = Simulator::Now();
        m_TPrecord8 = 0;
    }
    m_TPrecord8 += packet->GetSize();
}}

int
main(int argc, char* argv[])
{
    aqm = argv[1];
    uint16_t gNbNum = 1;
    uint16_t ueNumPergNb = 8;    
    
    Time simTime = Seconds(4);
    Time udpAppStartTime = MilliSeconds(1000);

    
    uint16_t numerologyBwp1 = 4;
    double centralFrequencyBand1 = 28e9;
    double bandwidthBand1 = 50e6;
    
    double totalTxPower = 35;

    
    std::string simTag = "default";
    std::string outputDir = "./";
        

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(2000000));

    
    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(gNbNum);
    // All units below are in meters
    gridScenario.SetHorizontalBsDistance(10.0);
    gridScenario.SetVerticalBsDistance(10.0);
    gridScenario.SetBsHeight(10);
    gridScenario.SetUtHeight(1.5);
    // must be set before BS number
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(gNbNum);
    gridScenario.SetUtNumber(ueNumPergNb * gNbNum);
    gridScenario.SetScenarioHeight(3); // Create a 3x3 scenario where the UE will
    gridScenario.SetScenarioLength(3); // be distributed.
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();


    NodeContainer ueVoiceContainer;
    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(j);
        
        {
            ueVoiceContainer.Add(ue);
        }
    }
    
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Put the pointers inside nrHelper
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1; // in this example, both bands have a single CC

    
    CcBwpCreator::SimpleOperationBandConf bandConf1(centralFrequencyBand1,
                                                    bandwidthBand1,
                                                    numCcPerBand,
                                                    BandwidthPartInfo::UMi_StreetCanyon);
    

    // By using the configuration created, it is time to make the operation bands
    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);
    
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));    
    nrHelper->InitializeOperationBand(&band1);

    double x = pow(10, totalTxPower / 10);
    double totalBandwidth = bandwidthBand1;

    allBwps = CcBwpCreator::GetAllBwps({band1});

    Packet::EnableChecking();
    Packet::EnablePrinting();

    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));                                         
    Time coreLatency = MilliSeconds(5);
    std::string coreBandwidth = "10Mbps";

    epcHelper->SetAttribute("S1uLinkDelay", TimeValue(coreLatency));
    epcHelper->SetAttribute("S1uLinkDataRate", DataRateValue(DataRate(coreBandwidth)));
    //epcHelper->SetAttribute("S5LinkDelay", TimeValue(coreLatency));
    // epcHelper->SetAttribute("S5LinkDataRate", DataRateValue(DataRate(coreBandwidth)));
    
    // Antennas for all the UEs
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));

    // Antennas for all the gNbsi
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    
    NetDeviceContainer enbNetDev =
        nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    NetDeviceContainer ueVoiceNetDev = nrHelper->InstallUeDevice(ueVoiceContainer, allBwps);

    randomStream += nrHelper->AssignStreams(enbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueVoiceNetDev, randomStream);
    
    nrHelper->GetGnbPhy(enbNetDev.Get(0), 0)
        ->SetAttribute("Numerology", UintegerValue(numerologyBwp1));
    nrHelper->GetGnbPhy(enbNetDev.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10 * log10((bandwidthBand1 / totalBandwidth) * x)));

    for (auto it = enbNetDev.Begin(); it != enbNetDev.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }

    for (auto it = ueVoiceNetDev.Begin(); it != ueVoiceNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }
   
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    Ptr<Node> sgw = epcHelper->GetSgwNode();
    NetDeviceContainer sgwpgwdevices = epcHelper->spdevices;
   // (sgwpgwdevices.Get(0)).SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("50p"));
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(8);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    Ptr<Node> remoteHost1 = remoteHostContainer.Get(1);
    Ptr<Node> remoteHost2 = remoteHostContainer.Get(2);
    Ptr<Node> remoteHost3 = remoteHostContainer.Get(3);
    Ptr<Node> remoteHost4 = remoteHostContainer.Get(4);
    Ptr<Node> remoteHost5 = remoteHostContainer.Get(5);
    Ptr<Node> remoteHost6 = remoteHostContainer.Get(6);
    Ptr<Node> remoteHost7 = remoteHostContainer.Get(7);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2000));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.002)));
    p2ph.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("200p"));

    NetDeviceContainer internetDevices;
    internetDevices.Add( p2ph.Install(pgw, remoteHost));
    internetDevices.Add( p2ph.Install(pgw, remoteHost1));
    internetDevices.Add( p2ph.Install(pgw, remoteHost2));
    internetDevices.Add( p2ph.Install(pgw, remoteHost3));
    internetDevices.Add( p2ph.Install(pgw, remoteHost4));
    internetDevices.Add( p2ph.Install(pgw, remoteHost5));
    internetDevices.Add( p2ph.Install(pgw, remoteHost6));
    internetDevices.Add( p2ph.Install(pgw, remoteHost7));    

    Ptr<NetDeviceQueueInterface> ndqi = CreateObject<NetDeviceQueueInterface> ();
    enbNetDev.Get(0)->AggregateObject (ndqi);
    NetDeviceContainer internalNet;
    internalNet.Add(enbNetDev);
    internalNet.Add(internetDevices);
    
    TrafficControlHelper tch;
    if(aqm == "ns3::FqPieQueueDisc"|| aqm == "ns3::FqCoDelQueueDisc")
    { 
    tch.SetRootQueueDisc(aqm,"DropBatchSize", UintegerValue(6400000)
                                             ,"Perturbation", UintegerValue(1));
    tch.SetQueueLimits("ns3::DynamicQueueLimits", "HoldTime", StringValue("1ms")); 
    }
    else
    {
    	tch.SetRootQueueDisc(aqm);
    }
    QueueDiscContainer queueDiscs1;
    QueueDiscContainer queueDiscs2;
    QueueDiscContainer queueDiscs3;
    QueueDiscContainer queueDiscs4;
    
    queueDiscs1 = tch.Install(internetDevices);
    queueDiscs2 = tch.Install(enbNetDev);
    tch.Uninstall(sgwpgwdevices);
    queueDiscs3 = tch.Install(sgwpgwdevices);
    //(epcHeler->EnbSgwDevices).SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("50p"));
    tch.Uninstall(epcHelper->EnbSgwDevices);
    queueDiscs4 =tch.Install(epcHelper->EnbSgwDevices); 	
    
    // PGW NODES FOR ALL 8 FLOWS 
    Ptr<QueueDisc> q1 = queueDiscs1.Get(1);
    Ptr<QueueDisc> q2 = queueDiscs1.Get(3);
    Ptr<QueueDisc> q3 = queueDiscs1.Get(5);
    Ptr<QueueDisc> q4 = queueDiscs1.Get(7);
    Ptr<QueueDisc> q5 = queueDiscs1.Get(9);
    Ptr<QueueDisc> q6 = queueDiscs1.Get(11);
    Ptr<QueueDisc> q7 = queueDiscs1.Get(13);
    Ptr<QueueDisc> q8 = queueDiscs1.Get(15);

    //PGW SGW NODES FOR PGW-SGW LINK
    Ptr<QueueDisc> p1 = queueDiscs3.Get(0);


    Ipv4AddressHelper ipv4h;
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting1 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost1->GetObject<Ipv4>());
    remoteHostStaticRouting1->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting2 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost2->GetObject<Ipv4>());
    remoteHostStaticRouting2->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting3 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost3->GetObject<Ipv4>());
    remoteHostStaticRouting3->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting4 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost4->GetObject<Ipv4>());
    remoteHostStaticRouting4->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting5 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost5->GetObject<Ipv4>());
    remoteHostStaticRouting5->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting6 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost6->GetObject<Ipv4>());
    remoteHostStaticRouting6->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting7 =
        ipv4RoutingHelper.GetStaticRouting(remoteHost7->GetObject<Ipv4>());
    remoteHostStaticRouting7->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    
    
    internet.Install(gridScenario.GetUserTerminals());

Ptr<Ipv4StaticRouting> pgwRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.2"), 3);
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.4"), 4);
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.6"), 5);
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.8"), 6);
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.10"), 7);
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.12"), 8);
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.14"), 9);
    pgwRouting->AddHostRouteTo(Ipv4Address("1.0.0.16"), 10);
    
    Ipv4InterfaceContainer ueVoiceIpIface =
        epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueVoiceNetDev));

    // Set the default gateway for the UEs
    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(
            gridScenario.GetUserTerminals().Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    
    nrHelper->AttachToClosestEnb(ueVoiceNetDev, enbNetDev);
    

    // --------------------------------------- NETWORK APPLICATION TRAFFIC ------------------------------------------

    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (242992));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (242992));

    uint16_t dlPortVoice = 1235;
    
    ApplicationContainer serverApps;
    ApplicationContainer clientApps1,clientApps2,clientApps3,clientApps4,clientApps5,clientApps6,clientApps7,clientApps8;

        serverApps.Start(udpAppStartTime);
        clientApps1.Start(udpAppStartTime);			//UDP
        clientApps2.Start(udpAppStartTime + Seconds(0.9));	//TCP
        clientApps3.Start(udpAppStartTime + Seconds(0.5));	//FTP
        clientApps4.Start(udpAppStartTime + Seconds(0.6));	//HTTP
        clientApps5.Start(udpAppStartTime + Seconds(0.4));	//VoIP
        clientApps6.Start(udpAppStartTime + Seconds(0.3));	//Gaming
        clientApps7.Start(udpAppStartTime + Seconds(0.7));	//Video
        clientApps8.Start(udpAppStartTime + Seconds(0.1));	//Dash
        serverApps.Stop(simTime);
        clientApps1.Stop(simTime);
        clientApps2.Stop(simTime);
        clientApps3.Stop(simTime);
        clientApps4.Stop(simTime);
        clientApps5.Stop(simTime);
        clientApps6.Stop(simTime);
        clientApps7.Stop(simTime);
        clientApps8.Stop(simTime);

    // 1.UDP ONOFFHELPER  

        //client
        PacketSinkHelper dlPacketSinkVoiceof ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPortVoice));
        clientApps1.Add(dlPacketSinkVoiceof.Install(ueVoiceContainer.Get(0)));
        
        //server
        OnOffHelper onOffHelper1("ns3::UdpSocketFactory", InetSocketAddress (ueVoiceIpIface.GetAddress(0),dlPortVoice));
        onOffHelper1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=20]"));
        onOffHelper1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=20]"));
        onOffHelper1.SetAttribute ("DataRate",StringValue ("10Mbps"));
        serverApps.Add(onOffHelper1.Install(remoteHost)); 



    // 2.TCP BULKSENDHELPER
        
        //client    
        PacketSinkHelper dlPacketSinkVoice ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPortVoice));
        clientApps2.Add(dlPacketSinkVoice.Install(ueVoiceContainer.Get(1)));
        
	
        //server
        BulkSendHelper source3 ("ns3::TcpSocketFactory", InetSocketAddress (ueVoiceIpIface.GetAddress(1),dlPortVoice));
        source3.SetAttribute ("MaxBytes", UintegerValue (0));
       //OnOffHelper onOffHelper2("ns3::TcpSocketFactory", InetSocketAddress (ueVoiceIpIface.GetAddress(1),dlPortVoice));
       //onOffHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=50]"));
       // onOffHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        //onOffHelper2.SetAttribute ("DataRate",StringValue ("10Mbps"));

        serverApps.Add(source3.Install(remoteHost1));
   
    // 3.TCP FTPHELPER
        
        //client
        PacketSinkHelper dlPacketSinkVoiceftp ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 20));
        clientApps3.Add(dlPacketSinkVoiceftp.Install(ueVoiceContainer.Get(2)));

        //server
        TrafficGeneratorHelper ftpHelper("ns3::TcpSocketFactory",
                                                 Address(),
                                                 TrafficGeneratorNgmnFtpMulti::GetTypeId());
        ftpHelper.SetAttribute("MaxFileSize", UintegerValue(5e6));//5e6
        ftpHelper.SetAttribute("Remote",AddressValue(InetSocketAddress (ueVoiceIpIface.GetAddress(2), 20)));
        serverApps.Add(ftpHelper.Install(remoteHost2));

    // 4.HTTP TRAFFIC
                
        //client    
        ThreeGppHttpClientHelper dlPacketSinkVoicehttp((Ipv4Address("1.0.0.8")));
        clientApps4.Add(dlPacketSinkVoicehttp.Install(ueVoiceContainer.Get(3)));

        //server
        ThreeGppHttpServerHelper httpServerHelper((Ipv4Address("1.0.0.8")));
        serverApps.Add(httpServerHelper.Install(remoteHost3));
        Ptr<ThreeGppHttpServer> httpServer = serverApps.Get(3)->GetObject<ThreeGppHttpServer>();
        PointerValue varPtr;
        httpServer->GetAttribute("Variables", varPtr);
        Ptr<ThreeGppHttpVariables> httpVariables = varPtr.Get<ThreeGppHttpVariables>();
        httpVariables->SetMainObjectSizeMean(256000);  
        //httpVariables->SetMainObjectSizeMax(3560000); // 250kB
        httpVariables->SetMainObjectSizeStdDev(4); // 40kB
	httpVariables->SetEmbeddedObjectSizeMean(250000);
	//httpVariables->SetEmbeddedObjectSizeMax(3000000);
	httpVariables->SetNumOfEmbeddedObjectsMax(200);
	httpVariables->SetNumOfEmbeddedObjectsScale(8);
	httpVariables->SetReadingTimeMean(Seconds(30));


    
    // 5.VOIP TRAFFIC
        
        //client
        PacketSinkHelper dlPacketSinkVoiceVoIP ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPortVoice));
        clientApps5.Add(dlPacketSinkVoiceVoIP.Install(ueVoiceContainer.Get(4)));
    
        //server
        TrafficGeneratorHelper trafficGeneratorHelperVoIP("ns3::UdpSocketFactory",
                                                              Address(),
                                                              TrafficGeneratorNgmnVoip::GetTypeId());
        trafficGeneratorHelperVoIP.SetAttribute("Remote",AddressValue(InetSocketAddress(ueVoiceIpIface.GetAddress(4),dlPortVoice)));
        serverApps.Add(trafficGeneratorHelperVoIP.Install(remoteHost4));
    
    
    // 6.GAMING TRAFFIC
        
        //client
        PacketSinkHelper dlPacketSinkVoiceGaming("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPortVoice));
        clientApps6.Add(dlPacketSinkVoiceGaming.Install(ueVoiceContainer.Get(5)));

        //server
        TrafficGeneratorHelper trafficGeneratorHelper1("ns3::TcpSocketFactory",
                                                              Address(),
                                                              TrafficGeneratorNgmnGaming::GetTypeId());
        trafficGeneratorHelper1.SetAttribute("IsDownlink", BooleanValue(true));
        //trafficGeneratorHelper1.SetAttribute("bParamPacketSizeDl", DoubleValue(36));
        //trafficGeneratorHelper1.SetAttribute("aParamPacketArrivalDl", DoubleValue(11));
        //trafficGeneratorHelper1.SetAttribute("bParamPacketArrivalDl", DoubleValue(3.7));
        //trafficGeneratorHelper1.SetAttribute("InitialPacketArrivalMin", UintegerValue(0));
        //trafficGeneratorHelper1.SetAttribute("InitialPacketArrivalMax", UintegerValue(40));
        trafficGeneratorHelper1.SetAttribute("Remote",AddressValue(InetSocketAddress(ueVoiceIpIface.GetAddress(5),dlPortVoice)));
        serverApps.Add(trafficGeneratorHelper1.Install(remoteHost5));
        
    
    // 7.NGMN VIDEO

        //client
        PacketSinkHelper dlPacketSinkVoiceVideo ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPortVoice));
        clientApps7.Add(dlPacketSinkVoiceVideo.Install(ueVoiceContainer.Get(6)));  
        
        //server
        TrafficGeneratorHelper trafficGeneratorHelper2("ns3::UdpSocketFactory",
                                                              Address(),
                                                              TrafficGeneratorNgmnVideo::GetTypeId());
        trafficGeneratorHelper2.SetAttribute("Remote",AddressValue(InetSocketAddress (ueVoiceIpIface.GetAddress(6),dlPortVoice)));
        serverApps.Add(trafficGeneratorHelper2.Install(remoteHost6));
  
    
    // 8.DASH TRAFFIC

        uint32_t users = 1;
        double target_dt = 35.0;
        std::string algorithm = "ns3::FdashClient";
        uint32_t bufferSpace = 400000000;
        std::string window = "10s";
        uint32_t protoNum = 0;
        std::vector<std::string> algorithms;
        std::stringstream ss (algorithm);
        uint32_t user=0;
        std::string proto;
        while(std::getline(ss,proto,',') && protoNum++ < users)
        {
        	algorithms.push_back(proto);
        }

        //client
        DashClientHelper client ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address("1.0.0.16"), dlPortVoice),algorithms[user % protoNum]);
        client.SetAttribute ("VideoId", UintegerValue (user+1)); // VideoId should be positive
        client.SetAttribute ("TargetDt", TimeValue (Seconds (target_dt)));
        client.SetAttribute ("window", TimeValue (Time (window)));
        client.SetAttribute ("bufferSpace", UintegerValue (bufferSpace));
        clientApps8.Add(client.Install (ueVoiceContainer.Get(7)));

        //server
        DashServerHelper server ("ns3::TcpSocketFactory",InetSocketAddress (Ipv4Address::GetAny(),dlPortVoice));
        serverApps.Add(server.Install (remoteHost7));

    // THROUGHPUT TRACING 

	th= asciiTraceHelper1.CreateFileStream("Output/"+ aqm + "/Throughput-stream.tr");	
    	gp=asciiTraceHelper1.CreateFileStream("Output/"+ aqm + "/Goodput-stream.tr");
        
    AsciiTraceHelper asciiTraceHelper;

    Ptr<Application> appsink1 = clientApps1.Get(0);
    Ptr<PacketSink> psink1    =  Ptr<PacketSink> (dynamic_cast<PacketSink *> (PeekPointer(appsink1)));
    psink1->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize1));

    Ptr<Application> appsink2 = clientApps2.Get(0);
    Ptr<PacketSink> psink2    =  Ptr<PacketSink> (dynamic_cast<PacketSink *> (PeekPointer(appsink2)));
    psink2->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize2));

    Ptr<Application> appsink3 = clientApps3.Get(0);
    Ptr<PacketSink> psink3    =  Ptr<PacketSink> (dynamic_cast<PacketSink *> (PeekPointer(appsink3)));
    psink3->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize3));

    Ptr<Application> appsink4  = clientApps4.Get(0);
    Ptr<ns3::ThreeGppHttpClient> httpclient =  Ptr<ThreeGppHttpClient> (dynamic_cast<ThreeGppHttpClient*> (PeekPointer(appsink4)));
    httpclient->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize4));

    Ptr<Application> appsink5 = clientApps5.Get(0);
    Ptr<PacketSink> psink5    =  Ptr<PacketSink> (dynamic_cast<PacketSink *> (PeekPointer(appsink5)));
    psink5->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize5));

    Ptr<Application> appsink6 = clientApps6.Get(0);
    Ptr<PacketSink> psink6    =  Ptr<PacketSink> (dynamic_cast<PacketSink *> (PeekPointer(appsink6)));
    psink6->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize6));

    Ptr<Application> appsink7 = clientApps7.Get(0);
    Ptr<PacketSink> psink7    =  Ptr<PacketSink> (dynamic_cast<PacketSink *> (PeekPointer(appsink7)));
    psink7->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize7));
    
    Ptr<Application> app1 = clientApps8.Get(0);
    Ptr<Node> node = app1->GetNode();
    Ptr<Ipv4L3Protocol> dashapp = node->GetObject<Ipv4L3Protocol>();
    if(dashapp == nullptr)
    {
        std ::cout << "null ptr" << std::endl;
        return 0;
    }
    dashapp->TraceConnectWithoutContext("Rx",MakeBoundCallback(&PayloadSize8));

    // NETWORK NODES ID INFORMATION     

    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        Ptr<Node> ue = gridScenario.GetUserTerminals().Get(j);
        std::cout << "UE ID is :" << ue->GetId() << std::endl;
    }

    Ptr<Node>gnbptr = gridScenario.GetBaseStations().Get(0);
    std::cout << "GNB ID is " << gnbptr->GetId() << std::endl;
    std::cout << "PGW ID is " << pgw->GetId() << std::endl;
    std::cout << "SGW ID is " << sgw->GetId() << std::endl;
    Ptr<Node>remoteHnode =remoteHost;
    std::cout <<"Remote Node 0 ID is "<< remoteHost->GetId() << std::endl;   

   // ----------------------------------------------TRACING -------------------------

       

    // NOTE  
    // TRACE1 IS SERVER 
    // TRACE2 IS CLIENT

    // UDP ON OFF APPLICATION
    Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream("Output/" + aqm + "/Udponoff1.tr");
    Config::Connect("/NodeList/12/ApplicationList/*/$ns3::OnOffApplication/Tx",MakeBoundCallback(&NotifyPacketTx,stream1));
    
    Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Udponoff2.tr");    
    Config::Connect("/NodeList/1/ApplicationList/*/$ns3::PacketSink/Rx",MakeBoundCallback(&NotifyPacketRx,stream2));

    // HTTP TRACE
    Ptr<OutputStreamWrapper> stream3 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Http1.tr");
    Config::Connect("/NodeList/15/ApplicationList/*/$ns3::ThreeGppHttpServer/Tx",MakeBoundCallback(&NotifyHttpTx,stream3));

    Ptr<OutputStreamWrapper> stream4 = asciiTraceHelper.CreateFileStream("Output/"+ aqm +"/Http2.tr");
    Config::Connect("/NodeList/4/ApplicationList/*/$ns3::ThreeGppHttpClient/Rx",MakeBoundCallback(&NotifyHttpRx,stream4));

    // TCP BULKSENDHELPER
    Ptr<OutputStreamWrapper> stream5 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Tcpbulk1.tr");
// Config::Connect("/NodeList/13/ApplicationList/*/$ns3::BulkSendApplication/Tx",MakeBoundCallback(&NotifyBulkTx,stream5));

    Ptr<OutputStreamWrapper> stream6 = asciiTraceHelper.CreateFileStream("Output/" +aqm +"/Tcpbulk2.tr");
    Config::Connect("/NodeList/2/ApplicationList/*/$ns3::PacketSink/Rx",MakeBoundCallback(&NotifyPacketRx,stream6));

    // FTP
    Ptr<OutputStreamWrapper> stream7 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Ftp1.tr");
    Config::Connect("/NodeList/3/ApplicationList/*/$ns3::PacketSink/Rx",MakeBoundCallback(&NotifyPacketRx,stream7));
    
    Ptr<OutputStreamWrapper> stream8 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Ftp2.tr");
    Config::Connect("/NodeList/14/ApplicationList/*/$ns3::TrafficGeneratorNgmnFtpMulti/Tx",MakeBoundCallback(&NotifyPacketTx,stream8));
    
    // VOIP
    Ptr<OutputStreamWrapper> stream9 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Voip1.tr");
    Config::Connect("/NodeList/16/ApplicationList/*/$ns3::TrafficGeneratorNgmnVoip/Tx",MakeBoundCallback(&VoipTx,stream9));
    
    Ptr<OutputStreamWrapper> stream10 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Voip2.tr");
    Config::Connect("/NodeList/5/ApplicationList/*/$ns3::PacketSink/Rx",MakeBoundCallback(&NotifyPacketRx,stream10));
    
    Ptr<OutputStreamWrapper> jitterstr = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Jitter.tr");
    Config::Connect("/NodeList/5/ApplicationList/*/$ns3::PacketSink/Rx",MakeBoundCallback(&VoipRx,jitterstr));

    // GAMING
    Ptr<OutputStreamWrapper> stream11 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Gaming1.tr");
    Config::Connect("/NodeList/17/ApplicationList/*/$ns3::TrafficGeneratorNgmnGaming/Tx",MakeBoundCallback(&NotifyPacketTx,stream11));
    
    Ptr<OutputStreamWrapper> stream12 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Gaming2.tr");
    Config::Connect("/NodeList/6/ApplicationList/*/$ns3::PacketSink/Rx",MakeBoundCallback(&NotifyPacketRx,stream12));

    // NGMN VIDEO
    Ptr<OutputStreamWrapper> stream13 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Video1.tr");
    Config::Connect("/NodeList/18/ApplicationList/*/$ns3::TrafficGeneratorNgmnVideo/Tx",MakeBoundCallback(&NotifyPacketTx,stream13));
    
    Ptr<OutputStreamWrapper> stream14 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Video2.tr");
    Config::Connect("/NodeList/7/ApplicationList/*/$ns3::PacketSink/Rx",MakeBoundCallback(&NotifyPacketRx,stream14));

    // DASH TRAFFIC 
    Ptr<OutputStreamWrapper> stream15 = asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Dash1.tr");
    Config::Connect("/NodeList/19/$ns3::Ipv4L3Protocol/Rx",MakeBoundCallback(&NotifyBulkTxDash ,stream15));
    
    Ptr<OutputStreamWrapper> stream16= asciiTraceHelper.CreateFileStream("Output/" + aqm +"/Dash2.tr");
    Config::Connect("/NodeList/8/$ns3::Ipv4L3Protocol/Tx",MakeBoundCallback(&NotifyPacketRxDash ,stream16));


    // QUEUEING DELAY 

    // Enqueue
    q1->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue)); 
    q2->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue)); 
    q3->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue)); 
    q4->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue)); 
    q5->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue)); 
    q6->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue)); 
    q7->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue));
    q8->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback(&PacketEnqueue));

    // DEQUEUE
    Ptr<OutputStreamWrapper> stream18 = asciiTraceHelper.CreateFileStream("Output/" + aqm + "/Queueing-Delay.tr");
    p1->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback(&PacketDequeue,stream18));
    
    // DROP 
    Ptr<OutputStreamWrapper> stream19 = asciiTraceHelper.CreateFileStream( "Output/" + aqm + "/PacketDrop.tr");
    p1->TraceConnectWithoutContext ("Drop", MakeBoundCallback(&PacketDrop,stream19));

   	    

// FLOW MONITOR
    nrHelper->EnableTraces();
    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
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
//  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();  
//    Simulator::Schedule (Seconds (5),
//  &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);
    Simulator::Stop(simTime);
    Simulator::Run();

    //nrHelper->EnableTraces();
    // ------------------------------------------------Print per-flow statistics--------------------------------------------------
    
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
    DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    monitor->SerializeToXmlFile ("results.xml" , true, true );

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;

    std::ofstream outFile;
    std::string filename = outputDir + "/" + simTag;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }

    outFile.setf(std::ios_base::fixed);

    double flowDuration = (simTime - udpAppStartTime).GetSeconds();
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


