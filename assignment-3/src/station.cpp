#include "common.h"
#include "classes.h"
#include "csma.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>

using SteadyClock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

int main(int argc, char* argv[]) {
    std::string strategyStr = "csma-cd";
    int stationId = 1;
    double pVal = 0.5;
    int numFrames = 20;
    std::string filename = "";
    std::string host = "127.0.0.1";
    int port = RECEIVER_PORT;
    int slotMs = 10;

    // 1. Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-s" || arg == "--strategy") && i + 1 < argc) {
            strategyStr = argv[++i];
        } else if ((arg == "-i" || arg == "--id") && i + 1 < argc) {
            stationId = std::stoi(argv[++i]);
        } else if ((arg == "-p" || arg == "--prob") && i + 1 < argc) {
            pVal = std::stod(argv[++i]);
        } else if ((arg == "-n" || arg == "--frames") && i + 1 < argc) {
            numFrames = std::stoi(argv[++i]);
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            filename = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--slot-ms" && i + 1 < argc) {
            slotMs = std::stoi(argv[++i]);
        }
    }

    create_crc32_table();

    // 2. Instantiate Strategy using Strategy Pattern
    std::unique_ptr<MacStrategy> strategy;
    if (strategyStr == "1-persistent" || strategyStr == "1P") {
        strategy = std::make_unique<OnePersistent>();
    } else if (strategyStr == "non-persistent" || strategyStr == "NP") {
        strategy = std::make_unique<NonPersistent>();
    } else if (strategyStr == "p-persistent" || strategyStr == "PP") {
        strategy = std::make_unique<PPersistent>(16, pVal, Milliseconds(1), Milliseconds(slotMs));
    } else if (strategyStr == "csma-cd" || strategyStr == "CD") {
        strategy = std::make_unique<CsmaCd>();
    } else {
        std::cerr << "[Station " << stationId << "] Unknown strategy: " << strategyStr << "\n";
        return 1;
    }

    // 3. Connect to the Channel Emulator
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

    // Hand over ownership of descriptor to RAII SocketWrapper
    SocketWrapper socket(clientFd);
    BackoffEngine backoff{Milliseconds(slotMs)};

    // 4. Prepare Frames to Transmit
    std::vector<Frame> frames;
    if (!filename.empty()) {
        uint32_t totalChunkFrames = 0;
        Frame* rawFrames = chunk_file(filename.c_str(), &totalChunkFrames);
        if (rawFrames != nullptr) {
            for (uint32_t i = 0; i < totalChunkFrames; ++i) {
                input_mac_address(&rawFrames[i]);
                rawFrames[i].header.seq_no = static_cast<uint8_t>(i % 256);

                uint32_t crc = compute_crc32(reinterpret_cast<const uint8_t*>(&rawFrames[i]), 
                                             sizeof(Header) + PAYLOAD_SIZE);
                rawFrames[i].trailer.fcs[0] = static_cast<uint8_t>((crc >> 24) & 0xFF);
                rawFrames[i].trailer.fcs[1] = static_cast<uint8_t>((crc >> 16) & 0xFF);
                rawFrames[i].trailer.fcs[2] = static_cast<uint8_t>((crc >> 8) & 0xFF);
                rawFrames[i].trailer.fcs[3] = static_cast<uint8_t>(crc & 0xFF);

                frames.push_back(rawFrames[i]);
            }
            free(rawFrames);
        }
    } else {
        // Generate dummy synthetic frames
        frames.resize(numFrames);
        for (int i = 0; i < numFrames; ++i) {
            Frame& f = frames[i];
            input_mac_address(&f);
            f.header.length = PAYLOAD_SIZE;
            f.header.seq_no = static_cast<uint8_t>(i % 256);
            std::memset(f.payload, 'A' + (stationId % 26), PAYLOAD_SIZE);

            uint32_t crc = compute_crc32(reinterpret_cast<const uint8_t*>(&f), 
                                         sizeof(Header) + PAYLOAD_SIZE);
            f.trailer.fcs[0] = static_cast<uint8_t>((crc >> 24) & 0xFF);
            f.trailer.fcs[1] = static_cast<uint8_t>((crc >> 16) & 0xFF);
            f.trailer.fcs[2] = static_cast<uint8_t>((crc >> 8) & 0xFF);
            f.trailer.fcs[3] = static_cast<uint8_t>(crc & 0xFF);
        }
    }

    std::cout << "[Station " << stationId << "] Ready. Strategy: " << strategyStr 
              << " | Total Frames: " << frames.size() << "\n";

    // 5. Execute Transmission Loop
    int delivered = 0;
    int dropped = 0;
    int totalCollisions = 0;
    int totalAttempts = 0;
    Milliseconds totalDelay{0};

    auto runStartTime = SteadyClock::now();

    for (size_t i = 0; i < frames.size(); ++i) {
        TransmissionResult res = strategy->executeTransmission(socket, backoff, &frames[i]);
        
        totalAttempts += res.attempts;
        totalCollisions += res.collisions;
        totalDelay += res.totalDelay;

        if (res.status == TransmissionStatus::SUCCESS) {
            delivered++;
        } else if (res.status == TransmissionStatus::DROPPED_COLLISION_LIMIT) {
            dropped++;
            std::cerr << "[Station " << stationId << "] Frame " << i 
                      << " DROPPED after exceeding collision limit!\n";
        } else {
            std::cerr << "[Station " << stationId << "] Socket error during frame " << i << "!\n";
            break;
        }
    }

    auto runEndTime = SteadyClock::now();
    auto totalWallTimeMs = std::chrono::duration_cast<Milliseconds>(runEndTime - runStartTime).count();

    double avgDelayMs = (delivered > 0) ? (static_cast<double>(totalDelay.count()) / delivered) : 0.0;
    double throughput = (totalWallTimeMs > 0) 
        ? (static_cast<double>(delivered * FRAME_SIZE * 8) / (totalWallTimeMs / 1000.0)) // bits per sec
        : 0.0;
    double efficiency = (totalAttempts > 0)
        ? (static_cast<double>(delivered) / totalAttempts * 100.0) // transmission efficiency %
        : 0.0;

    // 6. Output Statistics
    std::cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    std::cout << "  STATION " << stationId << " SUMMARY REPORT (" << strategyStr << ")\n";
    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    std::cout << "  Frames Attempted:       " << frames.size() << "\n";
    std::cout << "  Frames Delivered:       " << delivered << "\n";
    std::cout << "  Frames Dropped:         " << dropped << "\n";
    std::cout << "  Total Collisions:       " << totalCollisions << "\n";
    std::cout << "  Total Tx Attempts:      " << totalAttempts << "\n";
    std::cout << "  Average Delay:          " << std::fixed << std::setprecision(2) << avgDelayMs << " ms\n";
    std::cout << "  Throughput:             " << std::fixed << std::setprecision(2) << throughput << " bps\n";
    std::cout << "  Transmission Efficiency:" << std::fixed << std::setprecision(2) << efficiency << " %\n";
    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

    // Structured CSV log for automated parsing
    std::cout << "CSV_METRICS," << stationId << "," << strategyStr << "," << pVal << "," 
              << frames.size() << "," << delivered << "," << dropped << "," 
              << totalCollisions << "," << totalAttempts << "," << avgDelayMs << "," 
              << throughput << "," << efficiency << "\n";

    return 0;
}
