#ifndef CDMA_H
#define CDMA_H

#include "common.h"
#include "walsh.h"
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>

#define MAX_CHIPS_PER_PACKET (FRAME_SIZE * 8 * 64) // Up to W=64

#pragma pack(push, 1)
struct RegisterMsg {
    uint8_t  type;              // REGISTER_REQ_TYPE or REGISTER_RESP_TYPE
    uint8_t  station_id;        // Assigned / Requested station ID
    uint16_t num_stations;      // Total expected stations N
    uint16_t walsh_dimension;   // W (spreading factor)
    uint16_t flags;             // Reserved / Flags
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ChipPacketHeader {
    uint8_t  type;              // DATA_CHIP_TYPE or COMPOSITE_CHIP_TYPE
    uint8_t  station_id;        // Transmitting station ID (or 0xFF for composite)
    uint16_t round_num;         // Slot / Round number
    uint16_t num_bytes;         // Unspread payload byte count (e.g. sizeof(Frame) = 64)
    uint32_t num_chips;         // Total chips (num_bytes * 8 * W)
};
#pragma pack(pop)

// CDMA Channel Multiplexer class (used by channel emulator and standalone simulation)
class CdmaMultiplexer {
public:
    explicit CdmaMultiplexer(size_t walshDimension);

    // Reset composite signal buffer for a new round
    void reset(size_t numChips);

    // Add a station's chip signal to the composite channel: S_channel += S_i
    void addSignal(const int8_t* chips, size_t count);
    void addSignal(const std::vector<int8_t>& chips);

    // Add AWGN or random chip noise (for noise robustness demonstration)
    void addNoise(double noiseStdDev);

    // Get the composite channel chips
    const std::vector<int>& getComposite() const noexcept { return m_composite; }

    // Summary statistics
    size_t getChipCount() const noexcept { return m_composite.size(); }

private:
    size_t m_walshDimension;
    std::vector<int> m_composite;
};

// CDMA Receiver Engine: extracts and verifies frames from composite channel signal
class CdmaReceiver {
public:
    explicit CdmaReceiver(const WalshCodeSet& walsh);

    // Extract a complete Frame transmitted by senderId from composite channel signal
    bool extractFrame(const std::vector<int>& composite, size_t senderId, Frame& outFrame) const;

    // Verify CRC-32 Frame Check Sequence
    static bool verifyFrameCRC(const Frame& frame);

    // Extract raw payload string / buffer
    std::vector<uint8_t> extractPayload(const std::vector<int>& composite, size_t senderId, size_t byteCount) const;

private:
    const WalshCodeSet& m_walsh;
};

// Socket Transmission & Reception Helpers for CDMA packets
namespace CdmaNet {
    // Send a station's chip packet over socket
    bool sendStationChips(int socketFd, uint16_t roundNum, uint8_t stationId, 
                          uint16_t numBytes, const std::vector<int8_t>& chips);

    // Receive a station's chip packet from socket
    bool recvStationChips(int socketFd, ChipPacketHeader& header, std::vector<int8_t>& chips);

    // Send composite channel signal to a station
    bool sendCompositeChips(int socketFd, uint16_t roundNum, uint16_t numBytes, 
                           const std::vector<int>& compositeChips);

    // Receive composite channel signal from emulator
    bool recvCompositeChips(int socketFd, ChipPacketHeader& header, std::vector<int>& compositeChips);
}

#endif // CDMA_H
