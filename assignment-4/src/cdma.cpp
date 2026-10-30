#include "cdma.h"
#include <arpa/inet.h>
#include <cstring>
#include <random>

CdmaMultiplexer::CdmaMultiplexer(size_t walshDimension)
    : m_walshDimension(walshDimension) {}

void CdmaMultiplexer::reset(size_t numChips) {
    m_composite.assign(numChips, 0);
}

void CdmaMultiplexer::addSignal(const int8_t* chips, size_t count) {
    if (m_composite.size() < count) {
        m_composite.resize(count, 0);
    }
    for (size_t i = 0; i < count; ++i) {
        m_composite[i] += chips[i];
    }
}

void CdmaMultiplexer::addSignal(const std::vector<int8_t>& chips) {
    addSignal(chips.data(), chips.size());
}

void CdmaMultiplexer::addNoise(double noiseStdDev) {
    if (noiseStdDev <= 0.0) return;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, noiseStdDev);

    for (size_t i = 0; i < m_composite.size(); ++i) {
        double noisyVal = static_cast<double>(m_composite[i]) + dist(gen);
        m_composite[i] = static_cast<int>(std::round(noisyVal));
    }
}

CdmaReceiver::CdmaReceiver(const WalshCodeSet& walsh)
    : m_walsh(walsh) {}

bool CdmaReceiver::extractFrame(const std::vector<int>& composite, size_t senderId, Frame& outFrame) const {
    size_t chipsNeeded = sizeof(Frame) * 8 * m_walsh.getDimension();
    if (composite.size() < chipsNeeded) {
        return false;
    }

    std::vector<uint8_t> frameBytes = m_walsh.decodeBuffer(composite, senderId, sizeof(Frame));
    std::memcpy(&outFrame, frameBytes.data(), sizeof(Frame));
    return verifyFrameCRC(outFrame);
}

bool CdmaReceiver::verifyFrameCRC(const Frame& frame) {
    uint32_t expectedFcs = compute_crc32(reinterpret_cast<const uint8_t*>(&frame), 
                                         sizeof(Header) + PAYLOAD_SIZE);
    uint32_t actualFcs = (static_cast<uint32_t>(frame.trailer.fcs[0]) << 24) |
                         (static_cast<uint32_t>(frame.trailer.fcs[1]) << 16) |
                         (static_cast<uint32_t>(frame.trailer.fcs[2]) << 8)  |
                         (static_cast<uint32_t>(frame.trailer.fcs[3]));
    return (expectedFcs == actualFcs);
}

std::vector<uint8_t> CdmaReceiver::extractPayload(const std::vector<int>& composite, size_t senderId, size_t byteCount) const {
    return m_walsh.decodeBuffer(composite, senderId, byteCount);
}

namespace CdmaNet {

bool sendStationChips(int socketFd, uint16_t roundNum, uint8_t stationId, 
                      uint16_t numBytes, const std::vector<int8_t>& chips) {
    ChipPacketHeader hdr{};
    hdr.type = DATA_CHIP_TYPE;
    hdr.station_id = stationId;
    hdr.round_num = htons(roundNum);
    hdr.num_bytes = htons(numBytes);
    hdr.num_chips = htonl(static_cast<uint32_t>(chips.size()));

    if (send_from_buffer(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr), socketFd) < 0) {
        return false;
    }
    if (send_from_buffer(reinterpret_cast<const uint8_t*>(chips.data()), chips.size() * sizeof(int8_t), socketFd) < 0) {
        return false;
    }
    return true;
}

bool recvStationChips(int socketFd, ChipPacketHeader& header, std::vector<int8_t>& chips) {
    ChipPacketHeader wireHdr{};
    if (receive_in_buffer(reinterpret_cast<uint8_t*>(&wireHdr), sizeof(wireHdr), socketFd) < 0) {
        return false;
    }

    header.type = wireHdr.type;
    header.station_id = wireHdr.station_id;
    header.round_num = ntohs(wireHdr.round_num);
    header.num_bytes = ntohs(wireHdr.num_bytes);
    header.num_chips = ntohl(wireHdr.num_chips);

    chips.resize(header.num_chips);
    if (receive_in_buffer(reinterpret_cast<uint8_t*>(chips.data()), header.num_chips * sizeof(int8_t), socketFd) < 0) {
        return false;
    }
    return true;
}

bool sendCompositeChips(int socketFd, uint16_t roundNum, uint16_t numBytes, 
                        const std::vector<int>& compositeChips) {
    ChipPacketHeader hdr{};
    hdr.type = COMPOSITE_CHIP_TYPE;
    hdr.station_id = 0xFF; // Composite broadcast
    hdr.round_num = htons(roundNum);
    hdr.num_bytes = htons(numBytes);
    hdr.num_chips = htonl(static_cast<uint32_t>(compositeChips.size()));

    if (send_from_buffer(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr), socketFd) < 0) {
        return false;
    }

    // Convert each int to int16_t network byte order
    std::vector<int16_t> wireChips(compositeChips.size());
    for (size_t i = 0; i < compositeChips.size(); ++i) {
        wireChips[i] = htons(static_cast<int16_t>(compositeChips[i]));
    }

    if (send_from_buffer(reinterpret_cast<const uint8_t*>(wireChips.data()), 
                         wireChips.size() * sizeof(int16_t), socketFd) < 0) {
        return false;
    }
    return true;
}

bool recvCompositeChips(int socketFd, ChipPacketHeader& header, std::vector<int>& compositeChips) {
    ChipPacketHeader wireHdr{};
    if (receive_in_buffer(reinterpret_cast<uint8_t*>(&wireHdr), sizeof(wireHdr), socketFd) < 0) {
        return false;
    }

    header.type = wireHdr.type;
    header.station_id = wireHdr.station_id;
    header.round_num = ntohs(wireHdr.round_num);
    header.num_bytes = ntohs(wireHdr.num_bytes);
    header.num_chips = ntohl(wireHdr.num_chips);

    std::vector<int16_t> wireChips(header.num_chips);
    if (receive_in_buffer(reinterpret_cast<uint8_t*>(wireChips.data()), 
                          header.num_chips * sizeof(int16_t), socketFd) < 0) {
        return false;
    }

    compositeChips.resize(header.num_chips);
    for (size_t i = 0; i < header.num_chips; ++i) {
        compositeChips[i] = static_cast<int>(static_cast<int16_t>(ntohs(wireChips[i])));
    }
    return true;
}

} // namespace CdmaNet
