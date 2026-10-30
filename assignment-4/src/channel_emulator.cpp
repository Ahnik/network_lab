#include "common.h"
#include "walsh.h"
#include "cdma.h"
#include "classes.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int) {
    g_running = 0;
}

struct StationConnection {
    int fd{-1};
    uint8_t stationId{0};
    std::string ip;
    int port{0};
    bool registered{false};
};

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    size_t numStations = 4;
    size_t walshDim = 0;
    int maxRounds = -1; // -1 = run until clients disconnect
    double noiseStdDev = 0.0;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if ((arg == "-n" || arg == "--stations") && i + 1 < argc) {
            numStations = std::stoul(argv[++i]);
        } else if ((arg == "-w" || arg == "--walsh-dim") && i + 1 < argc) {
            walshDim = std::stoul(argv[++i]);
        } else if ((arg == "-r" || arg == "--rounds") && i + 1 < argc) {
            maxRounds = std::stoi(argv[++i]);
        } else if ((arg == "--noise") && i + 1 < argc) {
            noiseStdDev = std::stod(argv[++i]);
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    create_crc32_table();

    WalshCodeSet walsh(numStations, walshDim > 0 ? walshDim : WalshCodeSet::calculateDimension(numStations));
    std::string orthoReport;
    bool orthoOk = walsh.verifyOrthogonality(&orthoReport);

    std::cout << "===============================================================\n";
    std::cout << "            CDMA COMMON CHANNEL EMULATOR INITIALIZED           \n";
    std::cout << "===============================================================\n";
    std::cout << "  Port:                " << port << "\n";
    std::cout << "  Expected Stations:   " << numStations << "\n";
    std::cout << "  Walsh Dimension (W): " << walsh.getDimension() << "\n";
    std::cout << "  Noise StdDev:        " << noiseStdDev << "\n";
    std::cout << "  Orthogonality Check: " << (orthoOk ? "[PASS]" : "[FAIL]") << "\n";
    std::cout << "---------------------------------------------------------------\n";
    walsh.printMatrix(std::cout);
    std::cout << "===============================================================\n\n";

    int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "[ChannelEmulator] Socket creation failed\n";
        return 1;
    }

    int opt = 1;
    ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (::bind(serverFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "[ChannelEmulator] Bind failed on port " << port << "\n";
        ::close(serverFd);
        return 1;
    }

    if (::listen(serverFd, 64) < 0) {
        std::cerr << "[ChannelEmulator] Listen failed\n";
        ::close(serverFd);
        return 1;
    }

    std::cout << "[ChannelEmulator] Listening for " << numStations << " stations on port " << port << "...\n";

    // 1. Accept and register all expected stations
    std::vector<StationConnection> stations;
    stations.reserve(numStations);

    for (size_t i = 0; i < numStations && g_running; ++i) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = ::accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (!g_running) break;
            std::cerr << "[ChannelEmulator] Accept failed\n";
            continue;
        }

        // Receive RegisterRequest
        RegisterMsg reqMsg{};
        if (receive_in_buffer(reinterpret_cast<uint8_t*>(&reqMsg), sizeof(reqMsg), clientFd) < 0) {
            std::cerr << "[ChannelEmulator] Failed to receive register request\n";
            ::close(clientFd);
            --i;
            continue;
        }

        uint8_t assignedId = static_cast<uint8_t>(i);
        if (reqMsg.station_id < numStations) {
            assignedId = reqMsg.station_id;
        }

        // Send RegisterResponse
        RegisterMsg respMsg{};
        respMsg.type = REGISTER_RESP_TYPE;
        respMsg.station_id = assignedId;
        respMsg.num_stations = htons(static_cast<uint16_t>(numStations));
        respMsg.walsh_dimension = htons(static_cast<uint16_t>(walsh.getDimension()));

        if (send_from_buffer(reinterpret_cast<const uint8_t*>(&respMsg), sizeof(respMsg), clientFd) < 0) {
            std::cerr << "[ChannelEmulator] Failed to send register response\n";
            ::close(clientFd);
            --i;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
        StationConnection conn{clientFd, assignedId, clientIp, ntohs(clientAddr.sin_port), true};
        stations.push_back(conn);

        std::cout << "[ChannelEmulator] Station " << static_cast<int>(assignedId)
                  << " connected from " << clientIp << ":" << ntohs(clientAddr.sin_port)
                  << " -> Assigned Walsh Code: " << walsh.codeToString(assignedId)
                  << " (" << stations.size() << "/" << numStations << " ready)\n";
    }

    if (!g_running || stations.size() < numStations) {
        std::cerr << "[ChannelEmulator] Aborted before all stations connected.\n";
        for (auto& s : stations) ::close(s.fd);
        ::close(serverFd);
        return 1;
    }

    std::cout << "\n>>> All " << numStations << " stations connected! Channel synchronizer active. <<<\n\n";

    // 2. Main Synchronous CDMA Transmission Loop
    uint16_t currentRound = 0;
    uint64_t totalChipsMultiplexed = 0;
    uint64_t totalFramesMultiplexed = 0;
    CdmaMultiplexer multiplexer(walsh.getDimension());
    Stopwatch emulatorTimer;

    while (g_running) {
        if (maxRounds >= 0 && currentRound >= static_cast<uint16_t>(maxRounds)) {
            std::cout << "[ChannelEmulator] Reached maximum rounds limit (" << maxRounds << "). Ending simulation.\n";
            break;
        }

        // Receive chip packets from all stations for this round
        std::vector<std::vector<int8_t>> stationChips(numStations);
        std::vector<ChipPacketHeader> headers(numStations);
        size_t maxChipsInRound = 0;
        size_t activeCount = 0;
        bool allClosed = true;

        for (size_t i = 0; i < numStations; ++i) {
            int fd = stations[i].fd;
            ChipPacketHeader hdr{};
            std::vector<int8_t> chips;

            if (!CdmaNet::recvStationChips(fd, hdr, chips)) {
                // Client closed socket or error
                continue;
            }

            allClosed = false;
            headers[i] = hdr;
            stationChips[i] = std::move(chips);

            if (hdr.type == TERMINATE_TYPE) {
                // Station finished
                continue;
            }

            if (!stationChips[i].empty()) {
                activeCount++;
                if (stationChips[i].size() > maxChipsInRound) {
                    maxChipsInRound = stationChips[i].size();
                }
            }
        }

        if (allClosed) {
            std::cout << "[ChannelEmulator] All stations disconnected. Finished.\n";
            break;
        }

        // Superimpose all signals on the common physical channel
        multiplexer.reset(maxChipsInRound);

        for (size_t i = 0; i < numStations; ++i) {
            if (!stationChips[i].empty()) {
                multiplexer.addSignal(stationChips[i]);
                totalChipsMultiplexed += stationChips[i].size();
                totalFramesMultiplexed++;
            }
        }

        if (noiseStdDev > 0.0) {
            multiplexer.addNoise(noiseStdDev);
        }

        const auto& composite = multiplexer.getComposite();

        // Print real-time round details
        uint16_t numBytes = (headers[0].num_bytes > 0) ? headers[0].num_bytes : static_cast<uint16_t>(FRAME_SIZE);
        std::cout << "[Round " << std::setw(3) << currentRound << "] "
                  << "Active Transmitters: " << activeCount << "/" << numStations
                  << " | Composite Chips: " << composite.size();

        if (composite.size() >= 8 && verbose) {
            std::cout << "\n  Sample S_channel[0..7]: [";
            for (size_t k = 0; k < std::min<size_t>(8, composite.size()); ++k) {
                if (composite[k] > 0) std::cout << "+" << composite[k];
                else std::cout << composite[k];
                if (k + 1 < std::min<size_t>(8, composite.size())) std::cout << ", ";
            }
            std::cout << "]";
        }
        std::cout << "\n";

        // Broadcast composite channel signal to all stations
        for (size_t i = 0; i < numStations; ++i) {
            CdmaNet::sendCompositeChips(stations[i].fd, currentRound, numBytes, composite);
        }

        currentRound++;
    }

    double elapsedSec = emulatorTimer.elapsedSeconds();
    std::cout << "\n===============================================================\n";
    std::cout << "               CDMA CHANNEL EMULATOR SUMMARY                   \n";
    std::cout << "===============================================================\n";
    std::cout << "  Total Synchronous Rounds:  " << currentRound << "\n";
    std::cout << "  Total Frames Multiplexed:  " << totalFramesMultiplexed << "\n";
    std::cout << "  Total Chips Processed:     " << totalChipsMultiplexed << "\n";
    std::cout << "  Simulation Elapsed Time:   " << std::fixed << std::setprecision(2) << elapsedSec << " s\n";
    if (elapsedSec > 0.0) {
        double chipRate = static_cast<double>(totalChipsMultiplexed) / elapsedSec;
        double frameRate = static_cast<double>(totalFramesMultiplexed) / elapsedSec;
        std::cout << "  Channel Chip Rate:         " << std::fixed << std::setprecision(1) << chipRate << " chips/sec\n";
        std::cout << "  Channel Aggregate Frames:  " << std::fixed << std::setprecision(1) << frameRate << " frames/sec\n";
    }
    std::cout << "  Zero Collision Occurrences: CDMA Orthogonal Simultaneous Access\n";
    std::cout << "===============================================================\n";

    for (auto& s : stations) {
        ::close(s.fd);
    }
    ::close(serverFd);
    return 0;
}
