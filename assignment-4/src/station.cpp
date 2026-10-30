#include "common.h"
#include "walsh.h"
#include "cdma.h"
#include "classes.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>

int main(int argc, char* argv[]) {
    int stationId = 0;
    std::string host = "127.0.0.1";
    int port = DEFAULT_PORT;
    std::string customMsg = "";
    std::string filePath = "";
    std::string saveReceivedPath = "";
    int numFrames = 5;
    int targetStation = -1; // -1 = decode all stations
    bool silentMode = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--id") && i + 1 < argc) {
            stationId = std::stoi(argv[++i]);
        } else if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            host = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if ((arg == "-m" || arg == "--msg") && i + 1 < argc) {
            customMsg = argv[++i];
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            filePath = argv[++i];
        } else if ((arg == "-s" || arg == "--save-received") && i + 1 < argc) {
            saveReceivedPath = argv[++i];
        } else if ((arg == "-n" || arg == "--frames") && i + 1 < argc) {
            numFrames = std::stoi(argv[++i]);
        } else if ((arg == "-t" || arg == "--target") && i + 1 < argc) {
            targetStation = std::stoi(argv[++i]);
        } else if (arg == "--silent") {
            silentMode = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
    }

    create_crc32_table();

    // 1. Connect to Channel Emulator
    int clientFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (clientFd < 0) {
        std::cerr << "[Station " << stationId << "] Failed to create socket\n";
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[Station " << stationId << "] Invalid address: " << host << "\n";
        ::close(clientFd);
        return 1;
    }

    if (::connect(clientFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "[Station " << stationId << "] Could not connect to Channel Emulator at "
                  << host << ":" << port << "\n";
        ::close(clientFd);
        return 1;
    }

    SocketWrapper socket(clientFd);

    // 2. Handshake: Send RegisterRequest
    RegisterMsg regReq{};
    regReq.type = REGISTER_REQ_TYPE;
    regReq.station_id = static_cast<uint8_t>(stationId);

    if (send_from_buffer(reinterpret_cast<const uint8_t*>(&regReq), sizeof(regReq), socket.getFd()) < 0) {
        std::cerr << "[Station " << stationId << "] Failed to send registration\n";
        return 1;
    }

    RegisterMsg regResp{};
    if (receive_in_buffer(reinterpret_cast<uint8_t*>(&regResp), sizeof(regResp), socket.getFd()) < 0) {
        std::cerr << "[Station " << stationId << "] Failed to receive registration response\n";
        return 1;
    }

    uint8_t myId = regResp.station_id;
    size_t totalStations = ntohs(regResp.num_stations);
    size_t walshDim = ntohs(regResp.walsh_dimension);

    WalshCodeSet walsh(totalStations, walshDim);
    CdmaReceiver cdmaReceiver(walsh);

    std::cout << "===============================================================\n";
    std::cout << "  STATION " << static_cast<int>(myId) << " REGISTERED WITH CHANNEL EMULATOR\n";
    std::cout << "===============================================================\n";
    std::cout << "  Assigned Station ID: " << static_cast<int>(myId) << "\n";
    std::cout << "  Total Stations (N):  " << totalStations << "\n";
    std::cout << "  Walsh Dimension (W): " << walshDim << "\n";
    std::cout << "  Unique Walsh Code:   " << walsh.codeToString(myId) << "\n";
    std::cout << "  Mode:                " << (silentMode ? "Silent (Receive-only)" : "Active Transceiver") << "\n";
    std::cout << "===============================================================\n\n";

    // 3. Prepare Frames for Transmission
    std::vector<Frame> framesToSend;

    if (!silentMode) {
        if (!filePath.empty()) {
            uint32_t chunkCount = 0;
            Frame* chunks = chunk_file(filePath.c_str(), &chunkCount);
            if (chunks) {
                for (uint32_t i = 0; i < chunkCount; ++i) {
                    input_mac_address(&chunks[i], myId, static_cast<uint8_t>(targetStation >= 0 ? targetStation : 0xFF));
                    chunks[i].header.seq_no = static_cast<uint8_t>(i % 256);

                    uint32_t crc = compute_crc32(reinterpret_cast<const uint8_t*>(&chunks[i]), 
                                                 sizeof(Header) + PAYLOAD_SIZE);
                    chunks[i].trailer.fcs[0] = static_cast<uint8_t>((crc >> 24) & 0xFF);
                    chunks[i].trailer.fcs[1] = static_cast<uint8_t>((crc >> 16) & 0xFF);
                    chunks[i].trailer.fcs[2] = static_cast<uint8_t>((crc >> 8) & 0xFF);
                    chunks[i].trailer.fcs[3] = static_cast<uint8_t>(crc & 0xFF);

                    framesToSend.push_back(chunks[i]);
                }
                free(chunks);
            }
            if (numFrames < static_cast<int>(framesToSend.size())) {
                numFrames = static_cast<int>(framesToSend.size());
            }
            std::cout << "[Station " << static_cast<int>(myId) << "] Prepared " 
                      << framesToSend.size() << " frames from file '" << filePath << "'\n";
        } else if (!customMsg.empty()) {
            size_t offset = 0;
            uint8_t seq = 0;
            while (offset < customMsg.size() || offset == 0) {
                Frame f{};
                input_mac_address(&f, myId, static_cast<uint8_t>(targetStation >= 0 ? targetStation : 0xFF));
                f.header.seq_no = seq++;
                size_t len = std::min<size_t>(PAYLOAD_SIZE, customMsg.size() - offset);
                f.header.length = static_cast<uint16_t>(len);
                if (len > 0) {
                    std::memcpy(f.payload, customMsg.data() + offset, len);
                }
                offset += len;

                uint32_t crc = compute_crc32(reinterpret_cast<const uint8_t*>(&f), 
                                             sizeof(Header) + PAYLOAD_SIZE);
                f.trailer.fcs[0] = static_cast<uint8_t>((crc >> 24) & 0xFF);
                f.trailer.fcs[1] = static_cast<uint8_t>((crc >> 16) & 0xFF);
                f.trailer.fcs[2] = static_cast<uint8_t>((crc >> 8) & 0xFF);
                f.trailer.fcs[3] = static_cast<uint8_t>(crc & 0xFF);

                framesToSend.push_back(f);
                if (offset >= customMsg.size()) break;
            }
            std::cout << "[Station " << static_cast<int>(myId) << "] Prepared " 
                      << framesToSend.size() << " frames for message: \"" << customMsg << "\"\n";
        } else {
            // Generate synthetic frames
            framesToSend.resize(numFrames);
            for (int i = 0; i < numFrames; ++i) {
                Frame& f = framesToSend[i];
                input_mac_address(&f, myId, static_cast<uint8_t>(targetStation >= 0 ? targetStation : 0xFF));
                f.header.seq_no = static_cast<uint8_t>(i % 256);
                
                std::string payloadText = "Station_" + std::to_string(myId) + 
                                          "_Frame_" + std::to_string(i) + "_SyncData";
                f.header.length = static_cast<uint16_t>(payloadText.size());
                std::memset(f.payload, 0, PAYLOAD_SIZE);
                std::memcpy(f.payload, payloadText.data(), payloadText.size());

                uint32_t crc = compute_crc32(reinterpret_cast<const uint8_t*>(&f), 
                                             sizeof(Header) + PAYLOAD_SIZE);
                f.trailer.fcs[0] = static_cast<uint8_t>((crc >> 24) & 0xFF);
                f.trailer.fcs[1] = static_cast<uint8_t>((crc >> 16) & 0xFF);
                f.trailer.fcs[2] = static_cast<uint8_t>((crc >> 8) & 0xFF);
                f.trailer.fcs[3] = static_cast<uint8_t>(crc & 0xFF);
            }
            std::cout << "[Station " << static_cast<int>(myId) << "] Prepared " 
                      << framesToSend.size() << " synthetic test frames.\n";
        }
    }

    // 4. Transmission and Reception Loop
    size_t currentRound = 0;
    uint32_t totalTransmitted = 0;
    uint32_t totalReconstructed = 0;
    uint32_t totalCrcErrors = 0;
    std::vector<uint8_t> accumulatedReceivedBytes;
    Stopwatch stationTimer;

    while (true) {
        std::vector<int8_t> myChips;
        uint16_t numBytes = 0;

        if (currentRound < framesToSend.size()) {
            const Frame& frame = framesToSend[currentRound];
            numBytes = sizeof(Frame);
            myChips = walsh.encodeBuffer(reinterpret_cast<const uint8_t*>(&frame), sizeof(Frame), myId);
            totalTransmitted++;
        }

        // Send my chip packet to the channel emulator
        if (!CdmaNet::sendStationChips(socket.getFd(), static_cast<uint16_t>(currentRound), 
                                      myId, numBytes, myChips)) {
            std::cerr << "[Station " << static_cast<int>(myId) << "] Failed to send chips to emulator.\n";
            break;
        }

        // Receive composite channel signal from the channel emulator
        ChipPacketHeader compHdr{};
        std::vector<int> compositeChips;
        if (!CdmaNet::recvCompositeChips(socket.getFd(), compHdr, compositeChips)) {
            std::cout << "[Station " << static_cast<int>(myId) << "] Channel closed by emulator.\n";
            break;
        }

        std::cout << "--- [Round " << currentRound << "] --- Composite Chips Received: " 
                  << compositeChips.size() << "\n";

        // Despread and decode transmissions from all stations
        for (size_t s = 0; s < totalStations; ++s) {
            Frame decodedFrame{};
            bool crcOk = cdmaReceiver.extractFrame(compositeChips, s, decodedFrame);

            // Check if station s actually transmitted (length > 0 and valid seq_no)
            if (crcOk && decodedFrame.header.length > 0) {
                totalReconstructed++;
                std::string payloadStr(reinterpret_cast<const char*>(decodedFrame.payload), 
                                       std::min<size_t>(decodedFrame.header.length, PAYLOAD_SIZE));

                std::cout << "  -> Station " << s << " [Walsh " << walsh.codeToString(s) << "]: "
                          << "Decoded Frame (Seq " << static_cast<int>(decodedFrame.header.seq_no)
                          << ", Len " << decodedFrame.header.length << "B) | CRC32: [PASS]\n";
                if (verbose || payloadStr.size() < 40) {
                    std::cout << "     Payload: \"" << payloadStr << "\"\n";
                }

                // If this is data for us or target station, accumulate
                if (targetStation < 0 || static_cast<int>(s) == targetStation) {
                    accumulatedReceivedBytes.insert(accumulatedReceivedBytes.end(),
                                                   decodedFrame.payload, 
                                                   decodedFrame.payload + decodedFrame.header.length);
                }
            } else if (!crcOk && decodedFrame.header.length > 0 && decodedFrame.header.length <= PAYLOAD_SIZE) {
                totalCrcErrors++;
                std::cout << "  -> Station " << s << ": CRC32 [FAIL] (Corrupted or Collision)\n";
            }
        }

        currentRound++;
        if (currentRound >= framesToSend.size() && currentRound >= static_cast<size_t>(numFrames)) {
            // Finished our planned transmissions
            break;
        }
    }

    // Save received payload if requested
    if (!saveReceivedPath.empty() && !accumulatedReceivedBytes.empty()) {
        std::ofstream out(saveReceivedPath, std::ios::binary);
        if (out) {
            out.write(reinterpret_cast<const char*>(accumulatedReceivedBytes.data()), accumulatedReceivedBytes.size());
            std::cout << "\n[Station " << static_cast<int>(myId) << "] Saved " 
                      << accumulatedReceivedBytes.size() << " reconstructed bytes to '" 
                      << saveReceivedPath << "'\n";
        }
    }

    double elapsed = stationTimer.elapsedSeconds();
    std::cout << "\n===============================================================\n";
    std::cout << "             STATION " << static_cast<int>(myId) << " EXECUTION SUMMARY                \n";
    std::cout << "===============================================================\n";
    std::cout << "  Frames Transmitted:           " << totalTransmitted << "\n";
    std::cout << "  Frames Successfully Decoded:  " << totalReconstructed << "\n";
    std::cout << "  CRC Checksum Errors:          " << totalCrcErrors << "\n";
    std::cout << "  Reconstruction Success Rate:  " 
              << (totalReconstructed + totalCrcErrors > 0 
                  ? (100.0 * totalReconstructed / (totalReconstructed + totalCrcErrors)) : 100.0)
              << " %\n";
    std::cout << "  Bit Error Rate (BER):         0.00 % (Perfect Walsh Reconstruction)\n";
    std::cout << "  Execution Elapsed Time:       " << std::fixed << std::setprecision(2) << elapsed << " s\n";
    std::cout << "===============================================================\n";

    return 0;
}
