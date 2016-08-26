/* $Id: RTIDDSImpl.cxx,v 1.3.2.1 2014/04/01 11:56:52 juanjo Exp $

 (c) 2005-2012  Copyright, Real-Time Innovations, Inc.  All rights reserved.    	
 Permission to modify and use for internal purposes granted.   	
 This software is provided "as is", without warranty, express or implied.

 Modification History
 --------------------
 5.1.0.9,27mar14,jmc PERFTEST-27 Fixing resource limits when using
                     Turbo Mode
 5.1.0,19dec13,jmc PERFTEST-3 Added autothrottle and turbomode
 5.1.0,19dec13,jmc PERFTEST-2 window size in batching path and
                   domain id now is 1
 1.1b,29aug13,jmc CORE-5854 multicast disabled by default
 1.1b,29aug13,jmc CORE-5919 Moved hardcoded QoS to XML file when
                  possible
 1.1b,29aug13,jmc CORE-5867 transport builtin mask to only shmem
 1.0b,13jul10,jsr Added waitForPingResponse with timeout
 1.0b,07jul10,eys Cleanup perftest parameters
 1.0b,07jul10,jsr Fixed keepDurationUsec help
 1.0b,29jun10,jsr Fix heartbeat and fastheartbeat for windows
 1.0b,19apr10,acr Added latencyTest
 1.0a,11mar10,jsr Fixed tcp feature
 1.0a,10mar10,gn  Introduced tcp feature
 1.0a,26may09,fcs Fixed test finalization for keyed topics
 1.0a,14may09,fcs Added instances to INI
 1.0a,14may09,fcs Fixed command-line arguments processing
 1.0a,08may09,jsr Fixed default profile names
 1.0a,30apr09,jsr Added fflush to ensure the correct output
 1.0a,29apr09,jsr Added detection of wrong command line parameter
 1.0a,23apr09,jsr Changed to stderr the error and status messages
 1.0a,21apr09,jsr Reformat the help menu
 1.0a,17apr09,jsr Fixed #12322, added -waitsetDelayUsec and -waitsetEventCount 
                  command line option
 1.0a,03dec08,jsr Added -HeartbeatPeriod and -FastHeartbeatPeriod
 1.0a,14aug08,ch  optimized changing the key value before write
 1.0a,09aug08,ch  changed default instance_hash_buckets to number
                  of instances
 1.0a,02aug08,eys Added instanceHashBuckets parameter
 1.0a,10jun08,fcs Fixed latency channel durability
 1.0a,13may08,hhw Now using topic presentation scope.
 1.0a,08may08,ch  Support for multiple instances and durability
 1.0a,01may08,hhw Removed singleCore option.
                  Increased shared memory buffer for shm tests
                  KEEP_ALL is used for both reliable/unreliable.
 1.0a,24apr08,fcs Added -dataMulticastAddress
 1.0a,22apr08,fcs Fixed batching/best_effort scenario
 1.0a,21apr08,fcs Support for multiple publishers
 1.0a,19mar08,hhw Created.

===================================================================== */

#include "ndds/ndds_cpp.h"
#include "test.h"
#include "testPlugin.h"
#include "testSupport.h"
#include "MessagingIF.h"

#include "perftest_cpp.h"

#include "RTIDDSImpl.h"

#include "Property.h"


#ifdef RTI_WIN32
#pragma warning(push)
#pragma warning(disable : 4996)
#define STRNCASECMP     _strnicmp
#else
#define STRNCASECMP     strncasecmp
#endif
#define IS_OPTION(str, option) (STRNCASECMP(str, option, strlen(str)) == 0)

int          RTIDDSImpl::_WaitsetEventCount;
unsigned int RTIDDSImpl::_WaitsetDelayUsec;


/*********************************************************
 * Shutdown
 */
void RTIDDSImpl::Shutdown()
{
    if (_participant != NULL) {
        perftest_cpp::MilliSleep(2000);

        if (_reader != NULL) {
            _subscriber->delete_datareader(_reader);
        }

        perftest_cpp::MilliSleep(4000);

        _participant->delete_contained_entities();
        DDSTheParticipantFactory->delete_participant(_participant);
    }


    if(_pongSemaphore != NULL) {
		RTIOsapiSemaphore_delete(_pongSemaphore);
		_pongSemaphore = NULL;
    }

    DDSDomainParticipantFactory::finalize_instance();
}


/*********************************************************
 * PrintCmdLineHelp
 */
void RTIDDSImpl::PrintCmdLineHelp()
{
   const char *usage_string =
        /**************************************************************************/
		"\t-sendQueueSize <number> - Sets number of samples (or batches) in send\n"
        "\t                          queue, default 50\n"
        "\t-domain <ID>            - RTI DDS Domain, default 1\n"
        "\t-qosprofile <filename>  - Name of XML file for DDS Qos profiles, \n"
        "\t                          default perftest.xml\n"
        "\t-nic <ipaddr>           - Use only the nic specified by <ipaddr>.\n"
        "\t                          If unspecificed, use all available interfaces\n"
        "\t-multicast              - Use multicast to send data, default not to\n"
        "\t                          use multicast\n"
		"\t-nomulticast            - Do not use multicast to send data (default)\n"
        "\t-multicastAddress <ipaddr>   - Multicast address to use for receiving \n"
        "\t                          latency/announcement (pub) or \n"
        "\t                          throughtput(sub) data.\n"
        "\t                          If unspecified: latency 239.255.1.2,\n"
        "\t                                          announcement 239.255.1.100,\n"
	    "\t                                          throughput 239.255.1.1\n"
        "\t-bestEffort             - Run test in best effort mode, default reliable\n"
        "\t-batchSize <bytes>      - Size in bytes of batched message, default 0\n"
        "\t                          (no batching)\n"
        "\t-noPositiveAcks         - Disable use of positive acks in reliable \n"
        "\t                          protocol, default use positive acks\n"
        "\t-keepDurationUsec <usec> - Minimum time (us) to keep samples when\n"
        "\t                          positive acks are disabled, default 1000 us\n"
        "\t-enableSharedMemory     - Enable use of shared memory transport and \n"
		"\t                          disable all the other transports, default\n"
        "\t                          shared memory not enabled\n"
        "\t-enableTcpOnly          - Enable use of TCP transport and disable all\n"
        "\t                          the other transports, default do not use\n"
        "\t                          tcp transport\n"
        "\t-heartbeatPeriod <sec>:<nanosec>     - Sets the regular heartbeat period\n"
        "\t                          for throughput DataWriter, default 0:0\n"
        "\t                          (use XML QoS Profile value)\n"
        "\t-fastHeartbeatPeriod <sec>:<nanosec> - Sets the fast heartbeat period\n"
        "\t                          for the throughput DataWriter, default 0:0\n"
        "\t                          (use XML QoS Profile value)\n"
        "\t-durability <0|1|2|3>   - Set durability QOS, 0 - volatile,\n"
        "\t                          1 - transient local, 2 - transient, \n"
        "\t                          3 - persistent, default 0\n"
        "\t-noDirectCommunication  - Use brokered mode for persistent durability\n"
        "\t-instanceHashBuckets <#count> - Number of hash buckets for instances.\n"
        "\t                          If unspecified, same as number of\n"
        "\t                          instances.\n"
        "\t-waitsetDelayUsec <usec>  - UseReadThread related. Allows you to\n"
        "\t                          process incoming data in groups, based on the\n"
        "\t                          time rather than individually. It can be used\n"
        "\t                          combined with -waitsetEventCount,\n"
        "\t                          default 100 usec\n"
        "\t-waitsetEventCount <count> - UseReadThread related. Allows you to\n"
        "\t                          process incoming data in groups, based on the\n"
        "\t                          number of samples rather than individually. It\n"
        "\t                          can be used combined with -waitsetDelayUsec,\n"
        "\t                          default 5\n"
		"\t-enableAutoThrottle     - Enables the AutoThrottling feature in the\n"
		"\t                          throughput DataWriter (pub)\n"
	    "\t-enableTurboMode        - Enables the TurboMode feature in the throughput\n"
		"\t                          DataWriter (pub)\n"
        ;

    fprintf(stderr, usage_string);
}


/*********************************************************
 * ParseConfig
 */
bool RTIDDSImpl::ParseConfig(int argc, char *argv[])
{
    int i;
    int sec = 0;
    unsigned int nanosec = 0;
    // first scan for configFile

    for (i = 1; i < argc; ++i) {
        if (IS_OPTION(argv[i], "-configFile")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <fileName> after -configFile\n");
                return false;
            }
            _ConfigFile = argv[i];
        }
    }

    // now load configuration values from config file
    if (_ConfigFile != NULL) {
        QosDictionary *configSource;

        try {
            configSource = new QosDictionary(_ConfigFile);
        } catch (std::logic_error e) {
            fprintf(stderr, "Problem loading configuration file.\n");
            fprintf(stderr, "%s\n", e.what());
            return false;
        }

        QosProfile* config = configSource->get("perftest");
        
        if (config == NULL) {
            fprintf(stderr, "Could not find section [perftest] in file %s\n", _ConfigFile);
            return false;
        }

        _DataLen        = config->get_int("data length", _DataLen);
        _InstanceCount  = config->get_int("instances", _InstanceCount);
	    _LatencyTest = config->get_bool("run latency test", _LatencyTest);
        _IsDebug = config->get_bool("is debug", _IsDebug);

        config = configSource->get("RTIImpl");
        
        if (config == NULL) {
            fprintf(stderr, "Could not find section [RTIImpl] in file %s\n", _ConfigFile);
            return false;
        }

        _SendQueueSize = config->get_int("send queue size", _SendQueueSize);
        _DomainID = config->get_int("domain", _DomainID);
        _ProfileFile = config->get_string("qos profile file", _ProfileFile);
        _Nic = config->get_string("interface", _Nic);
        _IsMulticast = config->get_bool("is multicast", _IsMulticast);
        _IsReliable = config->get_bool("is reliable", _IsReliable);
        _BatchSize = config->get_int("batch size", _BatchSize);
        _KeepDurationUsec = (unsigned int)config->get_int("keep duration usec", (int)_KeepDurationUsec);
        _UsePositiveAcks = config->get_bool("use positive acks", _UsePositiveAcks);
        _UseSharedMemory = config->get_bool("enable shared memory", _UseSharedMemory);
        _UseTcpOnly = config->get_bool("enable tcp only", _UseTcpOnly);
        _WaitsetEventCount = config->get_int("waitset event count", _WaitsetEventCount);
        _WaitsetDelayUsec = (unsigned int)config->get_int("waitset delay usec", (int)_WaitsetDelayUsec);
        _Durability = config->get_int("durability", _Durability);
        _DirectCommunication = config->get_bool("direct communication", _DirectCommunication);
        _HeartbeatPeriod.sec = config->get_int(
            "heartbeat period sec", _HeartbeatPeriod.sec);
        _HeartbeatPeriod.nanosec = config->get_int(
            "heartbeat period nanosec", _HeartbeatPeriod.nanosec);
        _FastHeartbeatPeriod.sec = config->get_int(
            "fast heartbeat period sec", _FastHeartbeatPeriod.sec);
        _FastHeartbeatPeriod.nanosec = config->get_int(
            "fast heartbeat period nanosec", _FastHeartbeatPeriod.nanosec);
        _InstanceHashBuckets = config->get_int(
            "instance hash buckets", _InstanceHashBuckets);
    }

    // now load everything else, command line params override config file
    for (i = 0; i < argc; ++i) {
        if (IS_OPTION(argv[i], "-dataLen")) {

            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <length> after -dataLen\n");
                return false;
            }

            _DataLen = strtol(argv[i], NULL, 10);

            if (_DataLen < perftest_cpp::OVERHEAD_BYTES) {
                fprintf(stderr, "-dataLen must be >= %d\n", perftest_cpp::OVERHEAD_BYTES);
                return false;
            }

            if (_DataLen > TestMessage::MAX_DATA_SIZE) {
                fprintf(stderr, "-dataLen must be <= %d\n", TestMessage::MAX_DATA_SIZE);
                return false;
            }

            if (_DataLen > MAX_BINDATA_SIZE) {
                fprintf(stderr, "-dataLen must be <= %d\n", MAX_BINDATA_SIZE);
                return false;
            }
        } else if (IS_OPTION(argv[i], "-sendQueueSize")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <count> after -sendQueueSize\n");
                return false;
            }
            _SendQueueSize = strtol(argv[i], NULL, 10);
        } else if (IS_OPTION(argv[i], "-heartbeatPeriod")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <period> after -heartbeatPeriod\n");
                return false;
            }

            sec = 0;
            nanosec = 0;

            if (sscanf(argv[i],"%d:%d",&sec,&nanosec) != 2) {
                fprintf(stderr, "-heartbeatPeriod value must have the format <sec>:<nanosec>\n");
                return false;
            }

            if (sec > 0 || nanosec > 0) {
                _HeartbeatPeriod.sec = sec;
                _HeartbeatPeriod.nanosec = nanosec;
            }
        } else if (IS_OPTION(argv[i], "-fastHeartbeatPeriod")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <period> after -fastHeartbeatPeriod\n");
                return false;
            }

            sec = 0;
            nanosec = 0;

            if (sscanf(argv[i],"%d:%d",&sec,&nanosec) != 2) {
                fprintf(stderr, "-fastHeartbeatPeriod value must have the format <sec>:<nanosec>\n");
                return false;
            }

            if (sec > 0 || nanosec > 0) {
                _FastHeartbeatPeriod.sec = sec;
                _FastHeartbeatPeriod.nanosec = nanosec;
            }
        } else if (IS_OPTION(argv[i], "-domain")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <id> after -domain\n");
                return false;
            }
            _DomainID = strtol(argv[i], NULL, 10);
        } else if (IS_OPTION(argv[i], "-qosprofile")) {
            if ((i == (argc-1)) || *argv[++i] == '-') 
            {
                fprintf(stderr, "Missing <filename> after -qosprofile\n");
                return false;
            }
            _ProfileFile = argv[i];
        } else if (IS_OPTION(argv[i], "-multicast")) {
            _IsMulticast = true;
        } else if (IS_OPTION(argv[i], "-nomulticast")) {
            _IsMulticast = false;
        } else if (IS_OPTION(argv[i], "-multicastAddress")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <multicast address> after -multicastAddress\n");
                return false;
            }
            THROUGHPUT_MULTICAST_ADDR = argv[i];
            LATENCY_MULTICAST_ADDR = argv[i];
            ANNOUNCEMENT_MULTICAST_ADDR = argv[i];
        } else if (IS_OPTION(argv[i], "-nic")) {
            if ((i == (argc-1)) || *argv[++i] == '-') 
            {
                fprintf(stderr, "Missing <address> after -nic\n");
                return false;
            }
            _Nic = argv[i];
        } else if (IS_OPTION(argv[i], "-bestEffort")) {
            _IsReliable = false;
        } else if (IS_OPTION(argv[i], "-durability")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <kind> after -durability\n");
                return false;
            }       
            _Durability = strtol(argv[i], NULL, 10);

            if ((_Durability < 0) || (_Durability > 3)) {
                fprintf(stderr, "durability kind must be 0(volatile), 1(transient local), 2(transient), or 3(persistent) \n");
                return false;
            }
        } else if (IS_OPTION(argv[i], "-noDirectCommunication")) {
            _DirectCommunication = false;
        } else if (IS_OPTION(argv[i], "-instances")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <count> after -instances\n");
                return false;
            }
            _InstanceCount = strtol(argv[i], NULL, 10);

            if (_InstanceCount <= 0) {
                fprintf(stderr, "instance count cannot be negative or zero\n");
                return false;
            }
        } else if (IS_OPTION(argv[i], "-instanceHashBuckets")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <count> after -instanceHashBuckets\n");
                return false;
            }
            _InstanceHashBuckets = strtol(argv[i], NULL, 10);

            if (_InstanceHashBuckets <= 0 && _InstanceHashBuckets != -1) {
                fprintf(stderr, "instance hash buckets cannot be negative or zero\n");
                return false;
            }
        } else if (IS_OPTION(argv[i], "-batchSize")) {
            if ((i == (argc-1)) || *argv[++i] == '-') 
            {
                fprintf(stderr, "Missing <#bytes> after -batchSize\n");
                return false;
            }
            _BatchSize = strtol(argv[i], NULL, 10);

            if (_BatchSize < 0) 
            {
                fprintf(stderr, "batch size cannot be negative\n");
                return false;
            }
        } else if (IS_OPTION(argv[i], "-keepDurationUsec")) {
            if ((i == (argc-1)) || *argv[++i] == '-') {
                fprintf(stderr, "Missing <usec> after -keepDurationUsec\n");
                return false;
            }
            _KeepDurationUsec = strtol(argv[i], NULL, 10);
            if (_KeepDurationUsec < 0) {
                fprintf(stderr, "keep duration usec cannot be negative\n");
                return false;
            }
        } else if (IS_OPTION(argv[i], "-noPositiveAcks")) {
            _UsePositiveAcks = false;
        } 
        else if (IS_OPTION(argv[i], "-enableSharedMemory"))
        {
            _UseSharedMemory = true;
        } 
        else if (IS_OPTION(argv[i], "-enableTcpOnly") )
        {
            _UseTcpOnly = true;
        } 
        
        else if (IS_OPTION(argv[i], "-debug")) 
        {
            NDDSConfigLogger::get_instance()->
                set_verbosity(NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);
        }
        else if (IS_OPTION(argv[i], "-waitsetDelayUsec")) {
            if ((i == (argc-1)) || *argv[++i] == '-') 
            {
                fprintf(stderr, "Missing <usec> after -waitsetDelayUsec\n");
                return false;
            }
            _WaitsetDelayUsec = (unsigned int) strtol (argv[i], NULL, 10);
            if (_WaitsetDelayUsec < 0) 
            {
                fprintf(stderr, "waitset delay usec cannot be negative\n");
                return false;
            }
        }
        else if (IS_OPTION(argv[i], "-waitsetEventCount")) {
            if ((i == (argc-1)) || *argv[++i] == '-') 
            {
                fprintf(stderr, "Missing <count> after -waitsetEventCount\n");
                return false;
            }
            _WaitsetEventCount = strtol (argv[i], NULL, 10);
            if (_WaitsetEventCount < 0) 
            {
                fprintf(stderr, "waitset event count cannot be negative\n");
                return false;
            }
        } 
	    else if (IS_OPTION(argv[i], "-latencyTest")) 
        {
            _LatencyTest = true;
        }
	else if (IS_OPTION(argv[i], "-enableAutoThrottle"))
	{
	    fprintf(stderr, "Auto Throttling enabled. Automatically adjusting the DataWriter\'s writing rate\n");
            _AutoThrottle = true;
	}
	else if (IS_OPTION(argv[i], "-enableTurboMode") )
	{
	    _TurboMode = true;
	}
        else if (IS_OPTION(argv[i], "-configFile")) {
            /* Ignore config file */
            ++i;
        } 
		else {
            if (i > 0) {
                fprintf(stderr, "%s: not recognized\n", argv[i]);
                return false;
            }
        }
    }

    return true;
}


/*********************************************************
 * DomainListener
 */
class DomainListener : public DDSDomainParticipantListener
{
    virtual void on_inconsistent_topic(
        DDSTopic *topic,
        const DDS_InconsistentTopicStatus& /*status*/)
    {
        fprintf(stderr,"Found inconsistent topic %s of type %s.\n",
               topic->get_name(), topic->get_type_name());
        fflush(stderr);
    }

    virtual void on_offered_incompatible_qos(
        DDSDataWriter *writer,
        const DDS_OfferedIncompatibleQosStatus &status)
    {
        fprintf(stderr,"Found incompatible reader for writer %s QoS is %d.\n",
               writer->get_topic()->get_name(), status.last_policy_id);
        fflush(stderr);
    }

    virtual void on_requested_incompatible_qos(
        DDSDataReader *reader,
        const DDS_RequestedIncompatibleQosStatus &status)
    {
        fprintf(stderr,"Found incompatible writer for reader %s QoS is %d.\n",
               reader->get_topicdescription()->get_name(), status.last_policy_id);
        fflush(stderr);
    }
};

/*********************************************************
 * RTIPublisher
 */
class RTIPublisher : public IMessagingWriter
{
  private:
    TestData_tDataWriter *_writer;
    TestData_t data;
    int _num_instances;
    unsigned long _instance_counter;
    DDS_InstanceHandle_t *_instance_handles;
    RTIOsapiSemaphore *_pongSemaphore;

 public:
    RTIPublisher(DDSDataWriter *writer, int num_instances, RTIOsapiSemaphore * pongSemaphore)
    {
        _writer = TestData_tDataWriter::narrow(writer);
        data.bin_data.maximum(0);
        _num_instances = num_instances;
        _instance_counter = 0;
        _instance_handles = 
            (DDS_InstanceHandle_t *) malloc(sizeof(DDS_InstanceHandle_t)*num_instances);
	    _pongSemaphore = pongSemaphore;

        for (int i=0; i<_num_instances; ++i)
        {
            data.key[0] = (char) (i);
            data.key[1] = (char) (i >> 8);
            data.key[2] = (char) (i >> 16);
            data.key[3] = (char) (i >> 24);

            _instance_handles[i] = _writer->register_instance(data);
        }
    }

    void Flush()
    {
        _writer->flush();
    }

    bool Send(TestMessage &message)
    {
        DDS_ReturnCode_t retcode;
        int key = 0;

        data.entity_id = message.entity_id;
        data.seq_num = message.seq_num;
        data.timestamp_sec = message.timestamp_sec;
        data.timestamp_usec = message.timestamp_usec;
        data.latency_ping = message.latency_ping;
        data.bin_data.loan_contiguous((DDS_Octet*)message.data, message.size, message.size);

        if (_num_instances > 1) {
            key = _instance_counter++ % _num_instances;
            data.key[0] = (char) (key);
            data.key[1] = (char) (key >> 8);
            data.key[2] = (char) (key >> 16);
            data.key[3] = (char) (key >> 24);
        }

        retcode = _writer->write(data, _instance_handles[key]);

        data.bin_data.unloan();

        if (retcode != DDS_RETCODE_OK)
        {
            fprintf(stderr,"Write error %d.\n", retcode);
            return false;
        }

        return true;
    }

    void WaitForReaders(int numSubscribers)
    {
        DDS_PublicationMatchedStatus status;

        while (true)
        {
            _writer->get_publication_matched_status(status);
            if (status.current_count >= numSubscribers)
            {
                break;
            }
            perftest_cpp::MilliSleep(1000);
        }
    }

    bool waitForPingResponse() 
    {
	if(_pongSemaphore != NULL) 
	{
	    if(!RTIOsapiSemaphore_take(_pongSemaphore, NULL)) 
	    {
		fprintf(stderr,"Unexpected error taking semaphore\n");
		return false;
	    }
	}
	return true;
    }

    /* time out in milliseconds */
    bool waitForPingResponse(int timeout) 
    {
        struct RTINtpTime blockDurationIn;
        RTINtpTime_packFromMillisec(blockDurationIn, 0, timeout);

	if(_pongSemaphore != NULL) 
	{
	    if(!RTIOsapiSemaphore_take(_pongSemaphore, &blockDurationIn)) 
	    {
		fprintf(stderr,"Unexpected error taking semaphore\n");
		return false;
	    }
	}
	return true;
    }    

    bool notifyPingResponse() 
    {
	if(_pongSemaphore != NULL) 
	{
	    if(!RTIOsapiSemaphore_give(_pongSemaphore)) 
	    {
		fprintf(stderr,"Unexpected error giving semaphore\n");
		return false;
	    }
	}
	return true;
    }
};

/*********************************************************
 * ReceiverListener
 */
class ReceiverListener : public DDSDataReaderListener
{
  private:

    TestData_tSeq     _data_seq;
    DDS_SampleInfoSeq _info_seq;
    TestMessage       _message;
    IMessagingCB     *_callback;

  public:

    ReceiverListener(IMessagingCB *callback)
    {
        _callback = callback;
    }

    void on_data_available(DDSDataReader *reader)
    {
        TestData_tDataReader *datareader;

        DDS_ReturnCode_t retcode;
        int i;
        int seq_length;

        datareader = TestData_tDataReader::narrow(reader);
        if (datareader == NULL)
        {
            fprintf(stderr,"DataReader narrow error.\n");
            return;
        }

        
        retcode = datareader->take(
            _data_seq, _info_seq,
            DDS_LENGTH_UNLIMITED,
            DDS_ANY_SAMPLE_STATE,
            DDS_ANY_VIEW_STATE,
            DDS_ANY_INSTANCE_STATE);

        if (retcode == DDS_RETCODE_NO_DATA)
        {
            fprintf(stderr,"called back no data\n");
            return;
        }
        else if (retcode != DDS_RETCODE_OK)
        {
            fprintf(stderr,"Error during taking data %d\n", retcode);
            return;
        }

        seq_length = _data_seq.length();
        for (i = 0; i < seq_length; ++i)
        {
            if (_info_seq[i].valid_data)
            {
                _message.entity_id = _data_seq[i].entity_id;
                _message.seq_num = _data_seq[i].seq_num;
                _message.timestamp_sec = _data_seq[i].timestamp_sec;
                _message.timestamp_usec = _data_seq[i].timestamp_usec;
                _message.latency_ping = _data_seq[i].latency_ping;
                _message.size = _data_seq[i].bin_data.length();
                _message.data = (char *)_data_seq[i].bin_data.get_contiguous_bufferI();

                _callback->ProcessMessage(_message);

            }
        }

        retcode = datareader->return_loan(_data_seq, _info_seq);
        if (retcode != DDS_RETCODE_OK)
        {
            fprintf(stderr,"Error during return loan %d.\n", retcode);
            fflush(stderr);
        }
	
    }

};

/*********************************************************
 * RTISubscriber
 */
class RTISubscriber : public IMessagingReader
{
  private:
    TestData_tDataReader *_reader;
    TestData_tSeq         _data_seq;
    DDS_SampleInfoSeq     _info_seq;
    TestMessage           _message;
    DDSWaitSet           *_waitset;
    DDSConditionSeq       _active_conditions;

    int      _data_idx;
    bool      _no_data;

  public:

    RTISubscriber(DDSDataReader *reader)
    {
        _reader = TestData_tDataReader::narrow(reader);

        // null listener means using receive thread
        if (_reader->get_listener() == NULL) {
            DDS_WaitSetProperty_t property;
            property.max_event_count         = RTIDDSImpl::_WaitsetEventCount;
            property.max_event_delay.sec     = (int)RTIDDSImpl::_WaitsetDelayUsec / 1000000;
            property.max_event_delay.nanosec = (RTIDDSImpl::_WaitsetDelayUsec % 1000000) * 1000;

            _waitset = new DDSWaitSet(property);
           
            DDSStatusCondition *reader_status;
            reader_status = reader->get_statuscondition();
            reader_status->set_enabled_statuses(DDS_DATA_AVAILABLE_STATUS);
            _waitset->attach_condition(reader_status);
        }
    }

    virtual void Shutdown()
    {
        // loan may be outstanding during shutdown
        _reader->return_loan(_data_seq, _info_seq);
    }

    TestMessage *ReceiveMessage()
    {
        DDS_ReturnCode_t retcode;
        int seq_length;

        while (true) {
            // no outstanding reads
            if (_no_data)
            {
                _waitset->wait(_active_conditions, DDS_DURATION_INFINITE);

                if (_active_conditions.length() == 0)
                {
                    //printf("Read thread woke up but no data\n.");
                    //return NULL;
                    continue;
                }   

                retcode = _reader->take(
                    _data_seq, _info_seq,
                    DDS_LENGTH_UNLIMITED,
                    DDS_ANY_SAMPLE_STATE,
                    DDS_ANY_VIEW_STATE,
                    DDS_ANY_INSTANCE_STATE);
                if (retcode == DDS_RETCODE_NO_DATA)
                {
                    //printf("Called back no data.\n");
                    //return NULL;
                    continue;
                }
                else if (retcode != DDS_RETCODE_OK)
                {
                    fprintf(stderr,"Error during taking data %d.\n", retcode);
                    return NULL;
                }

                _data_idx = 0;
                _no_data = false;
            }

            seq_length = _data_seq.length();
            // check to see if hit end condition
            if (_data_idx == seq_length)
            {
                _reader->return_loan(_data_seq, _info_seq);
                _no_data = true;
                continue;
            }

            // skip non-valid data
            while ( (_info_seq[_data_idx].valid_data == false) && 
                    (++_data_idx < seq_length));

             // may have hit end condition
             if (_data_idx == seq_length) { continue; }

            _message.entity_id = _data_seq[_data_idx].entity_id;
            _message.seq_num = _data_seq[_data_idx].seq_num;
            _message.timestamp_sec = _data_seq[_data_idx].timestamp_sec;
            _message.timestamp_usec = _data_seq[_data_idx].timestamp_usec;
            _message.latency_ping = _data_seq[_data_idx].latency_ping;
            _message.size = _data_seq[_data_idx].bin_data.length();
            _message.data = (char *)_data_seq[_data_idx].bin_data.get_contiguous_bufferI();

            ++_data_idx;

            return &_message;
        }
    }

    void WaitForWriters(int numPublishers)
    {
        DDS_SubscriptionMatchedStatus status;

        while (true)
        {
            _reader->get_subscription_matched_status(status);

            if (status.current_count >= numPublishers)
            {
                break;
            }
            perftest_cpp::MilliSleep(1000);
        }
    }
};


/*********************************************************
 * Initialize
 */
bool RTIDDSImpl::Initialize(int argc, char *argv[])
{
    DDS_DomainParticipantQos qos; 
    DDS_DomainParticipantFactoryQos factory_qos;
    DomainListener *listener = new DomainListener();

    _factory = DDSDomainParticipantFactory::get_instance();

    if (!ParseConfig(argc, argv))
    {
        return false;
    }


    // only if we run the latency test we need to wait 
    // for pongs after sending pings
    _pongSemaphore = _LatencyTest ?
	RTIOsapiSemaphore_new(RTI_OSAPI_SEMAPHORE_KIND_BINARY, NULL) :
	NULL;
    
    

    // setup the QOS profile file to be loaded
    _factory->get_qos(factory_qos);
    factory_qos.profile.url_profile.ensure_length(1, 1);
    factory_qos.profile.url_profile[0] = DDS_String_dup(_ProfileFile);
    _factory->set_qos(factory_qos);

    if (_factory->reload_profiles() != DDS_RETCODE_OK) 
    {
        fprintf(stderr,"Problem opening QOS profiles file %s.\n", _ProfileFile);
        return false;
    }


    if (_factory->set_default_library(_ProfileLibraryName) != DDS_RETCODE_OK) 
    {
        fprintf(stderr,"No QOS Library named \"%s\" found in %s.\n",
               _ProfileLibraryName, _ProfileFile);
        return false;
    }

    // Configure DDSDomainParticipant QOS
    _factory->get_participant_qos_from_profile(qos, "PerftestQosLibrary", "BaseProfileQos");

    

    // set transports to use
    qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_UDPv4;
    if (_UseTcpOnly) {
        qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_MASK_NONE;
		DDSPropertyQosPolicyHelper::add_property(qos.property,
                                                     "dds.transport.load_plugins", "dds.transport.TCPv4.tcp1", false);
    } else {
        if (_UseSharedMemory)
        {
            qos.transport_builtin.mask = DDS_TRANSPORTBUILTIN_SHMEM;
        }
    }
    
    if (_AutoThrottle) {
		DDSPropertyQosPolicyHelper::add_property(qos.property,
			 "dds.domain_participant.auto_throttle.enable", "true", false);
	}

    if (!_UseTcpOnly) {
    
        char buf[64];

        if ((_Nic != NULL) && (strlen(_Nic) >= 0)) {
            DDSPropertyQosPolicyHelper::add_property(qos.property,
                                                     "dds.transport.UDPv4.builtin.parent.allow_interfaces", _Nic, false);
        }

        // Shem transport properties
        int received_message_count_max = 1024 * 1024 * 2 / _DataLen;

        sprintf(buf,"%d", received_message_count_max);
        DDSPropertyQosPolicyHelper::add_property(qos.property,
                                                 "dds.transport.shmem.builtin.received_message_count_max", buf, false);
    }

    // Creates the participant
    _participant = _factory->create_participant(
        _DomainID, qos, listener,
        DDS_INCONSISTENT_TOPIC_STATUS |
        DDS_OFFERED_INCOMPATIBLE_QOS_STATUS |
        DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);

    if (_participant == NULL)
    {
        fprintf(stderr,"Problem creating participant.\n");
        return false;
    }

    // Register the types and create the topics
    TestData_tTypeSupport::register_type(_participant, _typename);

    // Create the DDSPublisher and DDSSubscriber
    {       
        _publisher = _participant->create_publisher_with_profile(
            "PerftestQosLibrary", "BaseProfileQos", NULL, DDS_STATUS_MASK_NONE);
        if (_publisher == NULL)
        {
            fprintf(stderr,"Problem creating publisher.\n");
            return false;
        }

        _subscriber = _participant->create_subscriber_with_profile(
            "PerftestQosLibrary", "BaseProfileQos", NULL, DDS_STATUS_MASK_NONE);
        if (_subscriber == NULL)
        {
            fprintf(stderr,"Problem creating subscriber.\n");
            return false;
        }
    }

    return true;
}

/*********************************************************
 * CreateWriter
 */
IMessagingWriter *RTIDDSImpl::CreateWriter(const char *topic_name)
{
    DDSDataWriter *writer = NULL;
    DDS_DataWriterQos dw_qos;
    char *qos_profile = NULL;

    DDSTopic *topic = _participant->create_topic(
                       topic_name, _typename,
                       DDS_TOPIC_QOS_DEFAULT, NULL,
                       DDS_STATUS_MASK_NONE);

    if (topic == NULL)
    {
        fprintf(stderr,"Problem creating topic %s.\n", topic_name);
        return NULL;
    }

    if (strcmp(topic_name, perftest_cpp::_ThroughputTopicName) == 0)
    {
        if (_UsePositiveAcks)
        {
            qos_profile = "ThroughputQos";
        }
        else
        {
            qos_profile = "NoAckThroughputQos";
        }
    } else if (strcmp(topic_name, perftest_cpp::_LatencyTopicName) == 0) {
        if (_UsePositiveAcks)
        {
            qos_profile = "LatencyQos";
        }
        else
        {
            qos_profile = "NoAckLatencyQos";
        }
    } else if (strcmp(topic_name, perftest_cpp::_AnnouncementTopicName) == 0)
    {
        qos_profile = "AnnouncementQos";
    } else {
        fprintf(stderr,"topic name must either be %s or %s or %s.\n",
               perftest_cpp::_ThroughputTopicName, perftest_cpp::_LatencyTopicName,
               perftest_cpp::_AnnouncementTopicName);
        return NULL;
    }

    if (_factory->get_datawriter_qos_from_profile(dw_qos, _ProfileLibraryName, qos_profile)
        != DDS_RETCODE_OK) {
        fprintf(stderr,"No QOS Profile named \"%s\" found in QOS Library \"%s\" in file %s.\n",
                qos_profile, _ProfileLibraryName, _ProfileFile);
        return NULL;
    }
    
    if (_UsePositiveAcks) {
        dw_qos.protocol.rtps_reliable_writer.disable_positive_acks_min_sample_keep_duration.sec = (int)_KeepDurationUsec/1000000;
        dw_qos.protocol.rtps_reliable_writer.disable_positive_acks_min_sample_keep_duration.nanosec = _KeepDurationUsec%1000000;
    }

    // only force reliability on throughput/latency topics
    if (strcmp(topic_name, perftest_cpp::_AnnouncementTopicName) != 0) {
        if (_IsReliable) {
            dw_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        }
        else {
            dw_qos.reliability.kind = DDS_BEST_EFFORT_RELIABILITY_QOS;
        }
    }

    // These QOS's are only set for the Throughput datawriter
    if ((strcmp(qos_profile,"ThroughputQos") == 0) ||
        (strcmp(qos_profile,"NoAckThroughputQos") == 0))
    {
        if (_BatchSize > 0) {
            dw_qos.batch.enable = true;
            dw_qos.batch.max_data_bytes = _BatchSize;
            dw_qos.resource_limits.max_samples = DDS_LENGTH_UNLIMITED;
            dw_qos.writer_resource_limits.max_batches = _SendQueueSize;
        } else {
            dw_qos.resource_limits.max_samples = _SendQueueSize;
        }

        if (_HeartbeatPeriod.sec > 0 || _HeartbeatPeriod.nanosec > 0) {
            // set the heartbeat_period
            dw_qos.protocol.rtps_reliable_writer.heartbeat_period =
                _HeartbeatPeriod;
            // make the late joiner heartbeat compatible
            dw_qos.protocol.rtps_reliable_writer.late_joiner_heartbeat_period =
                _HeartbeatPeriod;
        }

        if (_FastHeartbeatPeriod.sec > 0 || _FastHeartbeatPeriod.nanosec > 0) {
            // set the fast_heartbeat_period
            dw_qos.protocol.rtps_reliable_writer.fast_heartbeat_period =
                _FastHeartbeatPeriod;
        }
        
        if (_AutoThrottle) {
			DDSPropertyQosPolicyHelper::add_property(dw_qos.property,
			"dds.data_writer.auto_throttle.enable", "true", false);
		}

		if (_TurboMode) {
			DDSPropertyQosPolicyHelper::add_property(dw_qos.property,
				"dds.data_writer.enable_turbo_mode", "true", false);
			dw_qos.batch.enable = false;
            dw_qos.resource_limits.max_samples = DDS_LENGTH_UNLIMITED;
            dw_qos.writer_resource_limits.max_batches = _SendQueueSize;
		}

        dw_qos.resource_limits.initial_samples = _SendQueueSize;
        dw_qos.resource_limits.max_samples_per_instance
            = dw_qos.resource_limits.max_samples;

        dw_qos.durability.kind = (DDS_DurabilityQosPolicyKind)_Durability;
        dw_qos.durability.direct_communication = _DirectCommunication;

        dw_qos.protocol.rtps_reliable_writer.heartbeats_per_max_samples = _SendQueueSize / 10;

        dw_qos.protocol.rtps_reliable_writer.low_watermark = _SendQueueSize * 1 / 10;
        dw_qos.protocol.rtps_reliable_writer.high_watermark = _SendQueueSize * 9 / 10;

        dw_qos.protocol.rtps_reliable_writer.max_send_window_size = _SendQueueSize;
        dw_qos.protocol.rtps_reliable_writer.min_send_window_size = _SendQueueSize;
    }

    if (((strcmp(qos_profile,"LatencyQos") == 0) || 
        (strcmp(qos_profile,"NoAckLatencyQos") == 0)) &&
        !_DirectCommunication && 
        (_Durability == DDS_TRANSIENT_DURABILITY_QOS ||
         _Durability == DDS_PERSISTENT_DURABILITY_QOS)) {
        dw_qos.durability.kind = (DDS_DurabilityQosPolicyKind)_Durability;
        dw_qos.durability.direct_communication = _DirectCommunication;
    }

    dw_qos.resource_limits.max_instances = _InstanceCount;
    dw_qos.resource_limits.initial_instances = _InstanceCount;

    if (_InstanceCount > 1) {
        if (_InstanceHashBuckets > 0) {
            dw_qos.resource_limits.instance_hash_buckets =
                _InstanceHashBuckets;
        } else {
            dw_qos.resource_limits.instance_hash_buckets = _InstanceCount;
        }
    }

    writer = _publisher->create_datawriter(
        topic, dw_qos, NULL,
        DDS_STATUS_MASK_NONE);
    
    if (writer == NULL)
    {
        fprintf(stderr,"Problem creating writer.\n");
        return NULL;
    }

    RTIPublisher *pub = new RTIPublisher(writer, _InstanceCount, _pongSemaphore);

    return pub;
}
    
/*********************************************************
 * CreateReader
 */
IMessagingReader *RTIDDSImpl::CreateReader(const char *topic_name, 
                                           IMessagingCB *callback)
{
    ReceiverListener *reader_listener = NULL;
    DDSDataReader *reader = NULL;
    DDS_DataReaderQos dr_qos;
    const char *qos_profile = NULL;

    DDSTopic *topic = _participant->create_topic(
                       topic_name, _typename,
                       DDS_TOPIC_QOS_DEFAULT, NULL,
                       DDS_STATUS_MASK_NONE);

    if (topic == NULL)
    {
        fprintf(stderr,"Problem creating topic %s.\n", topic_name);
        return NULL;
    }

    if (strcmp(topic_name, perftest_cpp::_ThroughputTopicName) == 0)
    {
        if (_UsePositiveAcks)
        {
            qos_profile = "ThroughputQos";
        }
        else
        {
            qos_profile = "NoAckThroughputQos";
        }
    }
    else if (strcmp(topic_name, perftest_cpp::_LatencyTopicName) == 0)
    {
        if (_UsePositiveAcks)
        {
            qos_profile = "LatencyQos";
        }
        else
        {
            qos_profile = "NoAckLatencyQos";
        }
    }
    else if (strcmp(topic_name, perftest_cpp::_AnnouncementTopicName) == 0)
    {
        qos_profile = "AnnouncementQos";
    }
    else
    {
        fprintf(stderr,"topic name must either be %s or %s or %s.\n",
               perftest_cpp::_ThroughputTopicName, perftest_cpp::_LatencyTopicName,
               perftest_cpp::_AnnouncementTopicName);
        return NULL;
    }

    if (_factory->get_datareader_qos_from_profile(dr_qos, _ProfileLibraryName, qos_profile)
        != DDS_RETCODE_OK)
    {
        fprintf(stderr,"No QOS Profile named \"%s\" found in QOS Library \"%s\" in file %s.\n",
                qos_profile, _ProfileLibraryName, _ProfileFile);
        return NULL;
    }

    // only force reliability on throughput/latency topics
    if (strcmp(topic_name, perftest_cpp::_AnnouncementTopicName) != 0)
    {
        if (_IsReliable)
        {
            dr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        }
        else
        {
            dr_qos.reliability.kind = DDS_BEST_EFFORT_RELIABILITY_QOS;
        }
    }

    // only apply durability on Throughput datareader
    if ((strcmp(qos_profile,"ThroughputQos") == 0) ||
        (strcmp(qos_profile,"NoAckThroughputQos") == 0))
    {
        dr_qos.durability.kind = (DDS_DurabilityQosPolicyKind)_Durability;
        dr_qos.durability.direct_communication = _DirectCommunication;
    }

    if (((strcmp(qos_profile,"LatencyQos") == 0) || 
        (strcmp(qos_profile,"NoAckLatencyQos") == 0)) &&
        !_DirectCommunication && 
        (_Durability == DDS_TRANSIENT_DURABILITY_QOS ||
         _Durability == DDS_PERSISTENT_DURABILITY_QOS)) {
        dr_qos.durability.kind = (DDS_DurabilityQosPolicyKind)_Durability;
        dr_qos.durability.direct_communication = _DirectCommunication;
    }

    dr_qos.resource_limits.initial_instances = _InstanceCount;
    dr_qos.resource_limits.max_instances = _InstanceCount;
    
    if (_InstanceCount > 1) {
        if (_InstanceHashBuckets > 0) {
            dr_qos.resource_limits.instance_hash_buckets =
                _InstanceHashBuckets;
        } else {
            dr_qos.resource_limits.instance_hash_buckets = _InstanceCount;
        }
    }

    if (!_UseTcpOnly && _IsMulticast) {
            const char *multicast_addr;

		if (strcmp(topic_name, perftest_cpp::_ThroughputTopicName) == 0)
		{
			multicast_addr = THROUGHPUT_MULTICAST_ADDR;
		}
		else  if (strcmp(topic_name, perftest_cpp::_LatencyTopicName) == 0)
		{
			multicast_addr = LATENCY_MULTICAST_ADDR;
		}
		else 
		{
			multicast_addr = ANNOUNCEMENT_MULTICAST_ADDR;
		}

		dr_qos.multicast.value.ensure_length(1,1);
		dr_qos.multicast.value[0].receive_address = DDS_String_dup(multicast_addr);
		dr_qos.multicast.value[0].receive_port = 0;
		dr_qos.multicast.value[0].transports.length(0);
    }


    if (callback != NULL)
    {
       /*  NDDSConfigLogger::get_instance()->
        set_verbosity_by_category(NDDS_CONFIG_LOG_CATEGORY_API, 
                                  NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);*/
        reader_listener = new ReceiverListener(callback);
        reader = _subscriber->create_datareader(
            topic, dr_qos, reader_listener, DDS_DATA_AVAILABLE_STATUS);
    }
    else
    {
        reader = _subscriber->create_datareader(
            topic, dr_qos, NULL, DDS_STATUS_MASK_NONE);
    }

    if (reader == NULL)
    {
        fprintf(stderr,"Problem creating reader.\n");
        return NULL;
    }

    if (!strcmp(topic_name, perftest_cpp::_ThroughputTopicName) ||
        !strcmp(topic_name, perftest_cpp::_LatencyTopicName)) {
        _reader = reader;
    }

    IMessagingReader *sub = new RTISubscriber(reader);
    return sub;
}

#ifdef RTI_WIN32
#pragma warning(pop)
#endif