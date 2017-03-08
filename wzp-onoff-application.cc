#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

#include "ns3/point-to-point-module.h"
#include "ns3/net-device.h"

#include "ns3/flow-monitor-helper.h"

#include "/home/wzp/workspace/ns-allinone-3.26/ns-3.26/scratch/configOnOff.h"
#include <vector>

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("CsmaBridgeExample");

// This is the application defined in another class, this definition will allow us to hook the congestion window.

class MyOnOffApplication : public Application 
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  MyOnOffApplication ();

  virtual ~MyOnOffApplication();

  /**
   * \brief Set the total number of bytes to send.
   *
   * Once these bytes are sent, no packet is sent again, even in on state.
   * The value zero means that there is no limit.
   *
   * \param maxBytes the total number of bytes to send
   */
  void SetMaxBytes (uint64_t maxBytes);

  /**
   * \brief Return a pointer to associated socket.
   * \return pointer to associated socket
   */
  Ptr<Socket> GetSocket (void) const;

 /**
  * \brief Assign a fixed random variable stream number to the random variables
  * used by this model.
  *
  * \param stream first stream index to use
  * \return the number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

protected:
  virtual void DoDispose (void);
private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  //helpers
  /**
   * \brief Cancel all pending events.
   */
  void CancelEvents ();

  // Event handlers
  /**
   * \brief Start an On period
   */
  void StartSending ();
  /**
   * \brief Start an Off period
   */
  void StopSending ();
  /**
   * \brief Send a packet
   */
  void SendPacket ();

  Ptr<Socket>     m_socket;       //!< Associated socket
  Address         m_peer;         //!< Peer address
  bool            m_connected;    //!< True if connected
  Ptr<RandomVariableStream>  m_onTime;       //!< rng for On Time
  Ptr<RandomVariableStream>  m_offTime;      //!< rng for Off Time
  DataRate        m_cbrRate;      //!< Rate that data is generated
  DataRate        m_cbrRateFailSafe;      //!< Rate that data is generated (check copy)
  uint32_t        m_pktSize;      //!< Size of packets
  uint32_t        m_residualBits; //!< Number of generated, but not sent, bits
  Time            m_lastStartTime; //!< Time last packet sent
  uint64_t        m_maxBytes;     //!< Limit total number of bytes sent
  uint64_t        m_totBytes;     //!< Total bytes sent so far
  EventId         m_startStopEvent;     //!< Event id for next start or stop event
  EventId         m_sendEvent;    //!< Event id of pending "send packet" event
  TypeId          m_tid;          //!< Type of the socket used

  /// Traced Callback: transmitted packets.
  TracedCallback<Ptr<const Packet> > m_txTrace;

private:
  /**
   * \brief Schedule the next packet transmission
   */
  void ScheduleNextTx ();
  /**
   * \brief Schedule the next On period start
   */
  void ScheduleStartEvent ();
  /**
   * \brief Schedule the next Off period start
   */
  void ScheduleStopEvent ();
  /**
   * \brief Handle a Connection Succeed event
   * \param socket the connected socket
   */
  void ConnectionSucceeded (Ptr<Socket> socket);
  /**
   * \brief Handle a Connection Failed event
   * \param socket the not connected socket
   */
  void ConnectionFailed (Ptr<Socket> socket);
};


TypeId
MyOnOffApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyOnOffApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MyOnOffApplication> ()
    .AddAttribute ("DataRate", "The data rate in on state.",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&MyOnOffApplication::m_cbrRate),
                   MakeDataRateChecker ())
    .AddAttribute ("PacketSize", "The size of packets sent in on state",
                   UintegerValue (512),
                   MakeUintegerAccessor (&MyOnOffApplication::m_pktSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&MyOnOffApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("OnTime", "A RandomVariableStream used to pick the duration of the 'On' state.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&MyOnOffApplication::m_onTime),
                   MakePointerChecker <RandomVariableStream>())
    .AddAttribute ("OffTime", "A RandomVariableStream used to pick the duration of the 'Off' state.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&MyOnOffApplication::m_offTime),
                   MakePointerChecker <RandomVariableStream>())
    .AddAttribute ("MaxBytes", 
                   "The total number of bytes to send. Once these bytes are sent, "
                   "no packet is sent again, even in on state. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MyOnOffApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&MyOnOffApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&MyOnOffApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

MyOnOffApplication::MyOnOffApplication ()
  : m_socket (0),
    m_connected (false),
    m_residualBits (0),
    m_lastStartTime (Seconds (0)),
    m_totBytes (0)
{
  NS_LOG_FUNCTION (this);
}

MyOnOffApplication::~MyOnOffApplication()
{
  NS_LOG_FUNCTION (this);
}

void 
MyOnOffApplication::SetMaxBytes (uint64_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
MyOnOffApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

int64_t 
MyOnOffApplication::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_onTime->SetStream (stream);
  m_offTime->SetStream (stream + 1);
  return 2;
}

void
MyOnOffApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void MyOnOffApplication::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind6 ();
        }
      else if (InetSocketAddress::IsMatchingType (m_peer) ||
               PacketSocketAddress::IsMatchingType (m_peer))
        {
          m_socket->Bind ();
        }
      m_socket->Connect (m_peer);
      m_socket->SetAllowBroadcast (true);
      m_socket->ShutdownRecv ();

      m_socket->SetConnectCallback (
        MakeCallback (&MyOnOffApplication::ConnectionSucceeded, this),
        MakeCallback (&MyOnOffApplication::ConnectionFailed, this));
    }
  m_cbrRateFailSafe = m_cbrRate;

  // Insure no pending event
  CancelEvents ();
  // If we are not yet connected, there is nothing to do here
  // The ConnectionComplete upcall will start timers at that time
  //if (!m_connected) return;
  ScheduleStartEvent ();
}

void MyOnOffApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  CancelEvents ();
  if(m_socket != 0)
    {
      m_socket->Close ();
    }
  else
    {
      NS_LOG_WARN ("MyOnOffApplication found null socket to close in StopApplication");
    }
}

void MyOnOffApplication::CancelEvents ()
{
  NS_LOG_FUNCTION (this);

  if (m_sendEvent.IsRunning () && m_cbrRateFailSafe == m_cbrRate )
    { // Cancel the pending send packet event
      // Calculate residual bits since last packet sent
      Time delta (Simulator::Now () - m_lastStartTime);
      int64x64_t bits = delta.To (Time::S) * m_cbrRate.GetBitRate ();
      m_residualBits += bits.GetHigh ();
    }
  m_cbrRateFailSafe = m_cbrRate;
  Simulator::Cancel (m_sendEvent);
  Simulator::Cancel (m_startStopEvent);
}

// Event handlers
void MyOnOffApplication::StartSending ()
{
  NS_LOG_FUNCTION (this);
  m_lastStartTime = Simulator::Now ();
  ScheduleNextTx ();  // Schedule the send packet event
  ScheduleStopEvent ();
}

void MyOnOffApplication::StopSending ()
{
  NS_LOG_FUNCTION (this);
  CancelEvents ();

  ScheduleStartEvent ();
}

// Private helpers
void MyOnOffApplication::ScheduleNextTx ()
{
  NS_LOG_FUNCTION (this);

  if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      uint32_t bits = m_pktSize * 8 - m_residualBits;
      NS_LOG_LOGIC ("bits = " << bits);
      Time nextTime (Seconds (bits /
                              static_cast<double>(m_cbrRate.GetBitRate ()))); // Time till next packet
      NS_LOG_LOGIC ("nextTime = " << nextTime);
      m_sendEvent = Simulator::Schedule (nextTime,
                                         &MyOnOffApplication::SendPacket, this);
    }
  else
    { // All done, cancel any pending events
      StopApplication ();
    }
}

void MyOnOffApplication::ScheduleStartEvent ()
{  // Schedules the event to start sending data (switch to the "On" state)
  NS_LOG_FUNCTION (this);

  Time offInterval = Seconds (m_offTime->GetValue ());
  NS_LOG_LOGIC ("start at " << offInterval);
  m_startStopEvent = Simulator::Schedule (offInterval, &MyOnOffApplication::StartSending, this);
}

void MyOnOffApplication::ScheduleStopEvent ()
{  // Schedules the event to stop sending data (switch to "Off" state)
  NS_LOG_FUNCTION (this);

  Time onInterval = Seconds (m_onTime->GetValue ());
  NS_LOG_LOGIC ("stop at " << onInterval);
  m_startStopEvent = Simulator::Schedule (onInterval, &MyOnOffApplication::StopSending, this);
}


void MyOnOffApplication::SendPacket ()
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());
  Ptr<Packet> packet = Create<Packet> (m_pktSize);
  m_txTrace (packet);
  m_socket->Send (packet);
  m_totBytes += m_pktSize;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                   << "s on-off application sent "
                   <<  packet->GetSize () << " bytes to "
                   << InetSocketAddress::ConvertFrom(m_peer).GetIpv4 ()
                   << " port " << InetSocketAddress::ConvertFrom (m_peer).GetPort ()
                   << " total Tx " << m_totBytes << " bytes");
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peer))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                   << "s on-off application sent "
                   <<  packet->GetSize () << " bytes to "
                   << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6 ()
                   << " port " << Inet6SocketAddress::ConvertFrom (m_peer).GetPort ()
                   << " total Tx " << m_totBytes << " bytes");
    }
  m_lastStartTime = Simulator::Now ();
  m_residualBits = 0;
  ScheduleNextTx ();
}


void MyOnOffApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  m_connected = true;
}

void MyOnOffApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void 
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void 
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void 
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

//static void
//CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
//{
//  NS_LOG_UNCOND ( index << Simulator::Now ().GetSeconds () << "\t" << newCwnd);
//}

static void
CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (context << "\t" << Simulator::Now ().GetSeconds () << "\t" << newCwnd);
}

//static void
//RxDrop (Ptr<const Packet> p)
//{
//  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
//}

// The following are queue related tracing functions.
// Packet drop event
static void 
AsciiDropEvent (std::string path, Ptr<const Packet> packet)
{
  NS_LOG_UNCOND ("PacketDrop:\t" << Simulator::Now ().GetSeconds () << "\t" << *packet);
//  cout << "aaa" << endl;
//  *os << "d " << Simulator::Now ().GetSeconds () << " ";
//  *os << path << " " << *packet << std::endl;
}
// Enqueue event
static void 
AsciiEnqueueEvent (std::string path, Ptr<const Packet> packet)
{
  NS_LOG_UNCOND ("PacketArrival\t" << Simulator::Now ().GetSeconds () << "\t" << *packet);
 // *os << "+ " << Simulator::Now ().GetSeconds () << " ";
 // *os << path << " " << *packet << std::endl;
}

static void 
AsciiPacketsInQueue (std::string path, uint32_t oldValue, uint32_t newValue) 
{
  NS_LOG_UNCOND ("PacketsInQueue\t" << Simulator::Now ().GetSeconds () << "\t" << newValue);
 // *os << "+ " << Simulator::Now ().GetSeconds () << " ";
 // *os << path << " " << *packet << std::endl;
}

const string ConvertToStringValue (int val, string name) {
	string tmp;
	if (name.compare (string("constant")) == 0) {
		tmp.assign("ns3::ConstantRandomVariable[Constant=");
	}
	else {
		tmp.assign("ns3::ExponentialRandomVariable[Mean=");
	}
	tmp.append(to_string(val));
	tmp.append("]");
	return tmp;
}

int 
main (int argc, char *argv[])
{
  //
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  //
#if 0 
  LogComponentEnable ("CsmaBridgeExample", LOG_LEVEL_INFO);
#endif

  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;
  cmd.Parse (argc, argv);

  //
  // Explicitly create the nodes required by the topology (shown above).
  //

  int numberOfTerminals = NUMBER_OF_TERMINALS;
  NS_LOG_INFO ("Create nodes.");
  NodeContainer terminals;
  terminals.Create (numberOfTerminals);

  // 
  NS_LOG_INFO ("Create server node.");
  NodeContainer servers;
  servers.Create (1);

  NodeContainer csmaSwitch;
  csmaSwitch.Create (1);

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;
  //csma.SetChannelAttribute ("DataRate", DataRateValue (EXPERIMENT_CONFIG_SENDER_LINK_DATA_RATE));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (EXPERIMENT_CONFIG_SENDER_LINK_DELAY)));

  CsmaHelper csmaServer;
  // UintegerValue, holds an unsigned integer type.
  csmaServer.SetQueue("ns3::DropTailQueue", "MaxBytes", UintegerValue(EXPERIMENT_CONFIG_BUFFER_SIZE_BYTES), "Mode", EnumValue (DropTailQueue::QUEUE_MODE_BYTES));
  csmaServer.SetChannelAttribute ("DataRate", DataRateValue (EXPERIMENT_CONFIG_SERVER_LINK_DATA_RATE));
  csmaServer.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (EXPERIMENT_CONFIG_SERVER_LINK_DELAY)));

  // Create the csma links, from each terminal to the switch
  NetDeviceContainer terminalDevices;
  NetDeviceContainer switchDevices;

  for (int i = 0; i < numberOfTerminals; i++)
    {
      NetDeviceContainer link = csma.Install (NodeContainer (terminals.Get (i), csmaSwitch));
      terminalDevices.Add (link.Get (0));
      switchDevices.Add (link.Get (1));
    }

  // Create point to point link, from the server to the bridge
  NetDeviceContainer serverDevices;
  NetDeviceContainer linkServer = csmaServer.Install (NodeContainer (servers.Get (0), csmaSwitch));
  serverDevices.Add (linkServer.Get (0));
  switchDevices.Add (linkServer.Get (1));
  ostringstream oss;
  oss << "/NodeList/" << csmaSwitch.Get (0) -> GetId () << "/DeviceList/" << linkServer.Get (1)->GetIfIndex() << "/$ns3::CsmaNetDevice/TxQueue/Enqueue";
  //oss << "/NodeList/" << csmaSwitch.Get (0) -> GetId () << "/DeviceList/4" << "/$ns3::CsmaNetDevice/TxQueue/Enqueue";
  //oss << "/NodeList/" << servers.Get (0)->GetId () << "/$ns3::CsmaNetDevice/TxQueue/Enqueue";
  //oss << "/NodeList/" << "5" << "/DeviceList/" << "4" << "/$ns3::CsmaNetDevice/TxQueue/Enqueue";
  //cout << oss.str() << endl;
  Config::Connect (oss.str(), MakeCallback (&AsciiEnqueueEvent));
  //servers.Get (0)->TraceConnect ("Enqueue", oss.str(), MakeCallback (&AsciiEnqueueEvent));

  oss.str("");
  oss.clear();
  //oss << "/NodeList/" << servers.Get (0)->GetId () << "/DeviceList/" << "4" << "/$ns3::CsmaNetDevice/TxQueue/Drop";
//  oss << "/NodeList/" << "5" << "/DeviceList/" << "4" << "/$ns3::CsmaNetDevice/TxQueue/Drop";
  oss << "/NodeList/" << csmaSwitch.Get (0) -> GetId () << "/DeviceList/" << linkServer.Get (1)->GetIfIndex() << "/$ns3::CsmaNetDevice/TxQueue/Drop";
  //cout << oss.str() << endl;
  Config::Connect (oss.str(), MakeCallback (&AsciiDropEvent));

  oss.str("");
  oss.clear();
  //oss << "/NodeList/" << servers.Get (0)->GetId () << "/DeviceList/" << "4" << "/$ns3::CsmaNetDevice/TxQueue/Drop";
//  oss << "/NodeList/" << "5" << "/DeviceList/" << "4" << "/$ns3::CsmaNetDevice/TxQueue/Drop";
  oss << "/NodeList/" << csmaSwitch.Get (0) -> GetId () << "/DeviceList/" << linkServer.Get (1)->GetIfIndex() << "/$ns3::CsmaNetDevice/TxQueue/PacketsInQueue";
  //cout << oss.str() << endl;
  Config::Connect (oss.str(), MakeCallback (&AsciiPacketsInQueue));

  // Create the bridge netdevice, which will do the packet switching
  Ptr<Node> switchNode = csmaSwitch.Get (0);
  BridgeHelper bridge;
  bridge.Install (switchNode, switchDevices);

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.Install (terminals);
  internet.Install (servers);
  // We've got the "hardware" in place.  Now we need to add IP addresses.
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.32.0", "255.255.224.0");
  ipv4.Assign (terminalDevices);
  ipv4.Assign (serverDevices);

  Ipv4InterfaceContainer serverIpv4; 
  serverIpv4.Add(ipv4.Assign (serverDevices));

  // Create a sink application on the server node to receive these applications. 
  // We don't need to modify this.
   uint16_t port = 50000;
   Address sinkLocalAddress (InetSocketAddress (serverIpv4.GetAddress(0), port));
   PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
   ApplicationContainer sinkApp = sinkHelper.Install (servers.Get (0));
   sinkApp.Start (Seconds (EXPERIMENT_CONFIG_START_TIME));
   sinkApp.Stop (Seconds (EXPERIMENT_CONFIG_STOP_TIME));

   //normally wouldn't need a loop here but the server IP address is different
   //on each p2p subnet

   vector<Ptr<Socket> > SocketVector(numberOfTerminals);
   for (vector<Ptr<Socket> >::iterator it = SocketVector.begin(); it < SocketVector.end(); it++) {
     int nodeIndex = it - SocketVector.begin();
     *it = Socket::CreateSocket (terminals.Get (nodeIndex), TcpSocketFactory::GetTypeId ());
     ostringstream oss;
     oss << "/NodeList/" << terminals.Get (nodeIndex)->GetId () << "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";
     //cout << oss.str() << endl;
     (*it)->TraceConnect ("CongestionWindow", oss.str(), MakeCallback (&CwndChange));
     //(*it)->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
   }

   ApplicationContainer clientApps;
   //vector<Ptr<MyOnOffApplication> > ApplicationVector(numberOfTerminals);
// Create OnOff applications to send TCP to the hub, one on each spoke node.
  // OnOffHelper onOffHelper ("ns3::TcpSocketFactory", Address ());
  // onOffHelper.SetAttribute ("OnTime", StringValue (ConvertToStringValue(EXPERIMENT_SENDER_ONTIME_CONSTANT, "constant")));
  // onOffHelper.SetAttribute ("OffTime", StringValue (ConvertToStringValue(EXPERIMENT_SENDER_ONTIME_CONSTANT, "exponential")));
  // onOffHelper.SetConstantRate (DataRate (160000));

   vector<Ptr<MyOnOffApplication> > ApplicationVector(numberOfTerminals);
   for(uint32_t i=0; i<terminals.GetN (); ++i)
   {
  //    AddressValue sinkAddress
  //       (InetSocketAddress (serverIpv4.GetAddress (0), port));
  //    onOffHelper.SetAttribute ("Remote", sinkAddress);
	ApplicationVector[i] = CreateObject<MyOnOffApplication> ();

      terminals.Get (i)->AddApplication(ApplicationVector[i]);
      // void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
   }
   clientApps.Start (Seconds (EXPERIMENT_CONFIG_START_TIME));
   clientApps.Stop (Seconds (EXPERIMENT_CONFIG_STOP_TIME));


  NS_LOG_INFO ("Configure Tracing.");

  //
  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // Trace output will be sent to the file "csma-bridge.tr"
  //
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("csma-bridge.tr"));

  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     csma-bridge-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  //
  csma.EnablePcapAll ("csma-bridge", false);

  //
  // Now, do the actual simulation.
  //
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  //flowMonitor = flowHelper.InstallAll();
  flowMonitor = flowHelper.Install(servers.Get (0));


  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();

  flowMonitor->SerializeToXmlFile("NameOfFile", false, false);
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 

