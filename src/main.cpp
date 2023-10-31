#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <LocalHost.hpp>
#include <AddressConversion.hpp>
#include <Logger.hpp>
#include <SpeedwireSocketFactory.hpp>
#include <SpeedwireHeader.hpp>
#include <SpeedwireTagHeader.hpp>
#include <SpeedwireEmeterProtocol.hpp>
#include <ObisData.hpp>
using namespace libspeedwire;

// susyids for different emeter device types
#define SUSYID_EMETER_10        (270)
#define SUSYID_EMETER_20        (349)
#define SUSYID_HOMEMANAGER_20   (372)

#define SUSYID  SUSYID_EMETER_20

// serial number of the device - choose an arbitrary number here, the combination of susyid and serialnumber must be unique in a given speedwire network
#define SERIAL_NUMBER (1901567274)

// sunny home manager in version 2.07.x.y used a different speedwire header (when unicast transmission was introduced)
#define USE_EXTENDED_EMETER_PROTOCOL (0)

// since firmware version 2.03.4.R a frequency measurement has been added to emeter packets
#define INCLUDE_FREQUENCY_MEASUREMENT (1)

#if INCLUDE_FREQUENCY_MEASUREMENT && USE_EXTENDED_EMETER_PROTOCOL
  #define UDP_PACKET_SIZE 610
  #define PROTOCOL_ID (SpeedwireData2Packet::sma_extended_emeter_protocol_id)
#elif INCLUDE_FREQUENCY_MEASUREMENT
  #define UDP_PACKET_SIZE 608
  #define PROTOCOL_ID (SpeedwireData2Packet::sma_emeter_protocol_id)
#else
  #define UDP_PACKET_SIZE 600
  #define PROTOCOL_ID (SpeedwireData2Packet::sma_emeter_protocol_id)
#endif

#if INCLUDE_FREQUENCY_MEASUREMENT
  #define FIRMWARE_VERSION    ("2.03.4.R")
#elif USE_EXTENDED_EMETER_PROTOCOL
  #define FIRMWARE_VERSION    ("2.07.4.R")
#else
  #define FIRMWARE_VERSION    ("2.0.18.R")
#endif


static void* insert(SpeedwireEmeterProtocol& emeter_packet, void* const obis, const ObisData& obis_data, const double value);
static void* insert(SpeedwireEmeterProtocol& emeter_packet, void* const obis, const ObisData& obis_data, const std::string& value);

class LogListener : public ILogListener {
public:
    virtual ~LogListener() {}

    virtual void log_msg(const std::string& msg, const LogLevel& level) {
        fprintf(stdout, "%s", msg.c_str());
    }

    virtual void log_msg_w(const std::wstring& msg, const LogLevel& level) {
        fprintf(stdout, "%ls", msg.c_str());
    }
};

static Logger logger("main");


int main(int argc, char** argv) {

    // configure logger and logging levels
    ILogListener* log_listener = new LogListener();
    LogLevel log_level = LogLevel::LOG_ERROR | LogLevel::LOG_WARNING;
    log_level = log_level | LogLevel::LOG_INFO_0;
    log_level = log_level | LogLevel::LOG_INFO_1;
    log_level = log_level | LogLevel::LOG_INFO_2;
    log_level = log_level | LogLevel::LOG_INFO_3;
    Logger::setLogListener(log_listener, log_level);

    // configure sockets; use unicast socket to avoid messing around with igmp issues
    LocalHost &localhost = LocalHost::getInstance();
    SpeedwireSocketFactory *socket_factory = SpeedwireSocketFactory::getInstance(localhost, SpeedwireSocketFactory::SocketStrategy::ONE_UNICAST_SOCKET_FOR_EACH_INTERFACE);

    // define speedwire packet
    uint8_t udp_packet[UDP_PACKET_SIZE];
    SpeedwireHeader speedwire_packet(udp_packet, sizeof(udp_packet));

    // determine the emeter payload length by subtracting the tag header overhead of the default tag header structure
    unsigned long udp_header_length = speedwire_packet.getDefaultHeaderTotalLength(1, 0, 0);
    uint16_t udp_payload_length = (uint16_t)(UDP_PACKET_SIZE - udp_header_length);

    // create a tag header structure using the correct emeter payload length
    speedwire_packet.setDefaultHeader(1, udp_payload_length, PROTOCOL_ID);
    uint8_t *end_of_emeter_payload = (uint8_t*)speedwire_packet.findTagPacket(SpeedwireTagHeader::sma_tag_endofdata);

    SpeedwireData2Packet data2_packet(speedwire_packet);
    SpeedwireEmeterProtocol emeter_packet(data2_packet);
    emeter_packet.setSusyID(SUSYID);
    emeter_packet.setSerialNumber(SERIAL_NUMBER);
    emeter_packet.setTime((uint32_t)localhost.getUnixEpochTimeInMs());

    // insert all measurements available in an sma emeter packet into udp packet payload;
    // they are inserted in the same order as they are generated by an sme emeter device;
    // the order is important, as most open source projects do not parse obis elements 
    // but rather assume information at a given byte offset inside the udp packet.
    void* obis = (void*)emeter_packet.getFirstObisElement();

    // totals
    obis = insert(emeter_packet, obis, ObisData::PositiveActivePowerTotal,     121.60);
    obis = insert(emeter_packet, obis, ObisData::PositiveActiveEnergyTotal,   1320.34);
    obis = insert(emeter_packet, obis, ObisData::NegativeActivePowerTotal,       0.00);
    obis = insert(emeter_packet, obis, ObisData::NegativeActiveEnergyTotal,    305.03);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactivePowerTotal,     0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactiveEnergyTotal,    5.90);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactivePowerTotal,   188.90);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactiveEnergyTotal,  949.68);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentPowerTotal,   224.60);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentEnergyTotal, 1757.41);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentPowerTotal,     0.00);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentEnergyTotal,  327.62);
    obis = insert(emeter_packet, obis, ObisData::PowerFactorTotal,               0.54);
#if INCLUDE_FREQUENCY_MEASUREMENT
    obis = insert(emeter_packet, obis, ObisData::Frequency,                     50.16);
#endif

    // line 1
    obis = insert(emeter_packet, obis, ObisData::PositiveActivePowerL1,          0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveActiveEnergyL1,       337.53);
    obis = insert(emeter_packet, obis, ObisData::NegativeActivePowerL1,         21.70);
    obis = insert(emeter_packet, obis, ObisData::NegativeActiveEnergyL1,       141.54);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactivePowerL1,        0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactiveEnergyL1,       2.48);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactivePowerL1,       22.30);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactiveEnergyL1,     176.48);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentPowerL1,        0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentEnergyL1,     473.68);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentPowerL1,       31.10);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentEnergyL1,     144.26);
    obis = insert(emeter_packet, obis, ObisData::CurrentL1,                      0.18);
    obis = insert(emeter_packet, obis, ObisData::VoltageL1,                    231.97);
    obis = insert(emeter_packet, obis, ObisData::PowerFactorL1,                  0.70);

    // line 2
    obis = insert(emeter_packet, obis, ObisData::PositiveActivePowerL2,        160.80);
    obis = insert(emeter_packet, obis, ObisData::PositiveActiveEnergyL2,       775.23);
    obis = insert(emeter_packet, obis, ObisData::NegativeActivePowerL2,          0.00);
    obis = insert(emeter_packet, obis, ObisData::NegativeActiveEnergyL2,        77.80);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactivePowerL2,        0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactiveEnergyL2,       7.38);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactivePowerL2,      126.00);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactiveEnergyL2,     535.19);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentPowerL2,      204.30);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentEnergyL2,     974.19);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentPowerL2,        0.00);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentEnergyL2,      89.10);
    obis = insert(emeter_packet, obis, ObisData::CurrentL2,                      1.12);
    obis = insert(emeter_packet, obis, ObisData::VoltageL2,                    230.66);
    obis = insert(emeter_packet, obis, ObisData::PowerFactorL2,                  0.79);

    // line 3
    obis = insert(emeter_packet, obis, ObisData::PositiveActivePowerL3,          0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveActiveEnergyL3,       271.21);
    obis = insert(emeter_packet, obis, ObisData::NegativeActivePowerL3,         17.60);
    obis = insert(emeter_packet, obis, ObisData::NegativeActiveEnergyL3,       149.31);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactivePowerL3,        0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveReactiveEnergyL3,       1.70);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactivePowerL3,       40.66);
    obis = insert(emeter_packet, obis, ObisData::NegativeReactiveEnergyL3,     243.67);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentPowerL3,        0.00);
    obis = insert(emeter_packet, obis, ObisData::PositiveApparentEnergyL3,     434.62);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentPowerL3,       44.30);
    obis = insert(emeter_packet, obis, ObisData::NegativeApparentEnergyL3,     156.83);
    obis = insert(emeter_packet, obis, ObisData::CurrentL3,                      0.23);
    obis = insert(emeter_packet, obis, ObisData::VoltageL3,                    230.09);
    obis = insert(emeter_packet, obis, ObisData::PowerFactorL3,                  0.40);

    // software version and end of data
    obis = insert(emeter_packet, obis, ObisData::SoftwareVersion, FIRMWARE_VERSION);
    //obis = insert(emeter_packet, obis, ObisData::EndOfData,       "");    // this is identical to the end of data tag header, i.e. the last 4 bytes of the udp packet

    //LocalHost::hexdump(udp_packet, UDP_PACKET_SIZE);

    // check if the packet is fully assembled
    if (obis != end_of_emeter_payload) {
        logger.print(LogLevel::LOG_ERROR, "invalid udp packet size %lu\n", (unsigned long)((uint8_t*)obis - udp_packet));
    }

#if 1
    // for debugging purposes
    SpeedwireHeader protocol(udp_packet, sizeof(udp_packet));
    bool valid = protocol.isValidData2Packet();
    if (valid) {

        SpeedwireData2Packet data2_packet(protocol);
        uint16_t length     = data2_packet.getTagLength();
        uint16_t protocolID = data2_packet.getProtocolID();
        int      offset     = data2_packet.getPayloadOffset();

        if (data2_packet.isEmeterProtocolID() ||
            data2_packet.isExtendedEmeterProtocolID()) {

            SpeedwireEmeterProtocol emeter(protocol);
            uint16_t susyid = emeter.getSusyID();
            uint32_t serial = emeter.getSerialNumber();
            uint32_t timer  = emeter.getTime();

            // extract obis data from the emeter packet and print each obis element
            const void* obis = emeter.getFirstObisElement();
            while (obis != NULL) {
                //emeter.printObisElement(obis, stderr);
                logger.print(LogLevel::LOG_INFO_2, "%s %s %s", SpeedwireEmeterProtocol::toHeaderString(obis).c_str(), SpeedwireEmeterProtocol::toValueString(obis, true).c_str(), SpeedwireEmeterProtocol::toValueString(obis, false).c_str());
                obis = emeter.getNextObisElement(obis);
            }
        }
    }
#endif

    //
    // main loop
    //
    while (true) {

        // update timer
        uint32_t current_time = (uint32_t)localhost.getUnixEpochTimeInMs();
        emeter_packet.setTime(current_time);

        // send speedwire emeter packet to all local interfaces
        const std::vector<std::string>& localIPs = localhost.getLocalIPv4Addresses();
        for (auto& local_ip_addr : localIPs) {
            SpeedwireSocket& socket = socket_factory->getSendSocket(SpeedwireSocketFactory::SocketType::UNICAST, local_ip_addr);
            logger.print(LogLevel::LOG_INFO_0, "broadcast sma emeter packet to %s (via interface %s)\n", AddressConversion::toString(socket.getSpeedwireMulticastIn4Address()).c_str(), socket.getLocalInterfaceAddress().c_str());
            int nbytes = socket.send(udp_packet, sizeof(udp_packet));
            if (nbytes != sizeof(udp_packet)) {
                logger.print(LogLevel::LOG_ERROR, "cannot send udp packet %d\n", nbytes);
            }
        }

        // sleep for 1000 milliseconds
        LocalHost::sleep(1000);
    }

    return 0;
}


// insert obis data into the given emeter packet
void* insert(SpeedwireEmeterProtocol& emeter_packet, void* const obis, const ObisData& obis_data, const double value) {
    // create a new obis data instance from the given obis data template instance
    ObisData temp(obis_data);
    // set its measurement value
    temp.measurementValues.addMeasurement(value, 0);
    // convert it into the obis byte representation
    std::array<uint8_t, 12> byte_array = temp.toByteArray();
    // insert it into the given emeter packet 
    return emeter_packet.setObisElement(obis, byte_array.data());
}

// insert obis data into the given emeter packet
void* insert(SpeedwireEmeterProtocol& emeter_packet, void* const obis, const ObisData& obis_data, const std::string& value) {
    // create a new obis data instance from the given obis data template instance
    ObisData temp(obis_data);
    // set its measurement value
    temp.measurementValues.value_string = value;
    // convert it into the obis byte representation
    std::array<uint8_t, 12> byte_array = temp.toByteArray();
    // insert it into the given emeter packet 
    return emeter_packet.setObisElement(obis, byte_array.data());
}