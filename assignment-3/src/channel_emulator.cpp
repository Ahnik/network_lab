#include "common.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <unistd.h>
#include <iomanip>

using SteadyClock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

enum class MediumState : uint8_t {
    IDLE = 0,
    BUSY = 1,
    COLLISION = 2
};

struct TransmitterSession {
    int fd{-1};
    bool isTransmitting{false};
    SteadyClock::time_point txStartTime;
    bool collided{false};
    bool alertSent{false};
    Frame frame{};
    size_t bytesReceived{0};
};

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int) {
    g_running = 0;
}

// Helper to construct a ControlFrame with valid CRC-32 FCS
static ControlFrame makeControlFrame(uint8_t type, uint8_t seq_no = 0) {
    ControlFrame ctrl{};
    ctrl.frame_type = type;
    ctrl.seq_no = seq_no;

    uint32_t fcs = compute_crc32(reinterpret_cast<const uint8_t*>(&ctrl), 
                                 sizeof(ctrl.frame_type) + sizeof(ctrl.seq_no));
    ctrl.fcs[0] = static_cast<uint8_t>((fcs >> 24) & 0xFF);
    ctrl.fcs[1] = static_cast<uint8_t>((fcs >> 16) & 0xFF);
    ctrl.fcs[2] = static_cast<uint8_t>((fcs >> 8) & 0xFF);
    ctrl.fcs[3] = static_cast<uint8_t>(fcs & 0xFF);
    return ctrl;
}

int main(int argc, char* argv[]) {
    int port = RECEIVER_PORT;
    int txDurationMs = 25; // Simulated frame transmission airtime

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (std::string(argv[i]) == "--tx-ms" && i + 1 < argc) {
            txDurationMs = std::stoi(argv[++i]);
        }
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    create_crc32_table();

    int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "[ChannelEmulator] Error creating server socket\n";
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

    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    std::cout << "  SHARED CHANNEL EMULATOR INITIALIZED\n";
    std::cout << "  Port: " << port << " | Frame Duration: " << txDurationMs << "ms\n";
    std::cout << "  Channel States: IDLE (0), BUSY (1), COLLISION (2)\n";
    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

    std::vector<pollfd> pollFds;
    pollFds.push_back({serverFd, POLLIN, 0});

    std::unordered_map<int, TransmitterSession> sessions;
    std::unordered_set<int> activeTransmitters;

    // Global performance statistics
    uint64_t totalSenseReqs = 0;
    uint64_t totalCollisions = 0;
    uint64_t totalSuccessfulFrames = 0;
    uint64_t totalJamsReceived = 0;

    auto updateChannelState = [&]() -> MediumState {
        if (activeTransmitters.empty()) {
            return MediumState::IDLE;
        } else if (activeTransmitters.size() == 1) {
            return MediumState::BUSY;
        } else {
            return MediumState::COLLISION;
        }
    };

    while (g_running) {
        int pollRes = ::poll(pollFds.data(), pollFds.size(), 5 /* 5 ms tick */);
        if (pollRes < 0 && g_running) {
            break;
        }

        auto now = SteadyClock::now();

        // Check for incoming connections and data
        for (size_t i = 0; i < pollFds.size(); ++i) {
            if (!(pollFds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
                continue;
            }

            int fd = pollFds[i].fd;

            // New client station connecting
            if (fd == serverFd) {
                sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                int clientFd = ::accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
                if (clientFd >= 0) {
                    pollFds.push_back({clientFd, POLLIN, 0});
                    sessions[clientFd] = TransmitterSession{clientFd, false, {}, false, false, {}, 0};
                }
                continue;
            }

            // Existing client activity
            if (pollFds[i].revents & POLLIN) {
                uint8_t peekByte = 0;
                ssize_t n = ::recv(fd, &peekByte, 1, MSG_PEEK);

                if (n <= 0) {
                    // Client disconnected
                    close(fd);
                    activeTransmitters.erase(fd);
                    sessions.erase(fd);
                    pollFds.erase(pollFds.begin() + i);
                    --i;
                    continue;
                }

                // Handle CARRIER SENSE request (1 byte)
                if (peekByte == SENSE_REQ_TYPE) {
                    uint8_t req = 0;
                    ::recv(fd, &req, 1, 0);
                    totalSenseReqs++;

                    MediumState curState = updateChannelState();
                    uint8_t resp = static_cast<uint8_t>(curState);
                    ::send(fd, &resp, 1, 0);
                    continue;
                }

                // Handle JAM SIGNAL (1 byte)
                if (peekByte == JAM_SIGNAL_TYPE) {
                    uint8_t jam = 0;
                    ::recv(fd, &jam, 1, 0);
                    totalJamsReceived++;

                    if (activeTransmitters.count(fd)) {
                        activeTransmitters.erase(fd);
                        sessions[fd].isTransmitting = false;
                        sessions[fd].collided = false;
                        sessions[fd].alertSent = false;
                        sessions[fd].bytesReceived = 0;
                    }
                    continue;
                }

                // Handle incoming Frame data
                auto& sess = sessions[fd];
                if (sess.bytesReceived >= sizeof(Frame)) {
                    // Entire frame already received, awaiting transmission completion
                    continue;
                }

                uint8_t* targetPtr = reinterpret_cast<uint8_t*>(&sess.frame) + sess.bytesReceived;
                size_t needed = sizeof(Frame) - sess.bytesReceived;

                ssize_t bytesRead = ::recv(fd, targetPtr, needed, 0);
                if (bytesRead <= 0) {
                    ::close(fd);
                    activeTransmitters.erase(fd);
                    sessions.erase(fd);
                    pollFds.erase(pollFds.begin() + i);
                    --i;
                    continue;
                }

                sess.bytesReceived += bytesRead;

                // Frame reception just initiated
                if (!sess.isTransmitting) {
                    sess.isTransmitting = true;
                    sess.txStartTime = now;
                    sess.collided = false;
                    sess.alertSent = false;
                    activeTransmitters.insert(fd);

                    // Check if starting this transmission causes a collision
                    if (activeTransmitters.size() > 1) {
                        totalCollisions++;
                        // Flag all active transmitters as collided
                        for (int activeFd : activeTransmitters) {
                            sessions[activeFd].collided = true;
                            // For CSMA/CD: notify them of the collision immediately!
                            if (!sessions[activeFd].alertSent) {
                                sessions[activeFd].alertSent = true;
                                ControlFrame alert = makeControlFrame(COLLISION_ALERT_TYPE);
                                ::send(activeFd, &alert, sizeof(alert), MSG_DONTWAIT);
                            }
                        }
                    }
                }
            } else if (pollFds[i].revents & (POLLHUP | POLLERR)) {
                ::close(fd);
                activeTransmitters.erase(fd);
                sessions.erase(fd);
                pollFds.erase(pollFds.begin() + i);
                --i;
            }
        }

        // Process elapsed transmission times
        std::vector<int> finishedFds;
        for (int txFd : activeTransmitters) {
            auto& sess = sessions[txFd];
            auto elapsed = std::chrono::duration_cast<Milliseconds>(now - sess.txStartTime).count();

            // Check if full frame transmission airtime has elapsed
            if (sess.bytesReceived == sizeof(Frame) && elapsed >= txDurationMs) {
                finishedFds.push_back(txFd);
            }
        }

        // Finalize completed transmissions
        for (int txFd : finishedFds) {
            auto& sess = sessions[txFd];

            if (sess.collided) {
                // If collision alert was already sent (CSMA/CD), do not send duplicate NAK
                if (!sess.alertSent) {
                    ControlFrame nak = makeControlFrame(NAK_FRAME, sess.frame.header.seq_no);
                    ::send(txFd, &nak, sizeof(nak), 0);
                }
            } else {
                // Succeeded: verify FCS
                uint32_t computedFcs = compute_crc32(reinterpret_cast<const uint8_t*>(&sess.frame),
                                                     sizeof(Header) + PAYLOAD_SIZE);
                uint32_t receivedFcs = ((uint32_t)sess.frame.trailer.fcs[0] << 24) |
                                       ((uint32_t)sess.frame.trailer.fcs[1] << 16) |
                                       ((uint32_t)sess.frame.trailer.fcs[2] << 8)  |
                                       ((uint32_t)sess.frame.trailer.fcs[3]);

                if (computedFcs == receivedFcs) {
                    totalSuccessfulFrames++;
                    ControlFrame ack = makeControlFrame(ACK_FRAME, sess.frame.header.seq_no);
                    ::send(txFd, &ack, sizeof(ack), 0);
                } else {
                    totalCollisions++;
                    ControlFrame nak = makeControlFrame(NAK_FRAME, sess.frame.header.seq_no);
                    ::send(txFd, &nak, sizeof(nak), 0);
                }
            }

            // Reset session for this transmitter
            sess.isTransmitting = false;
            sess.collided = false;
            sess.alertSent = false;
            sess.bytesReceived = 0;
            activeTransmitters.erase(txFd);
        }
    }

    uint64_t totalAttemptsHandled = totalSuccessfulFrames + totalCollisions;
    double channelEfficiency = (totalAttemptsHandled > 0)
        ? (static_cast<double>(totalSuccessfulFrames) / totalAttemptsHandled * 100.0)
        : 0.0;

    std::cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    std::cout << "  CHANNEL EMULATOR SUMMARY REPORT\n";
    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    std::cout << "  Total Carrier Sense Inquiries: " << totalSenseReqs << "\n";
    std::cout << "  Total Frames Successfully Delivered: " << totalSuccessfulFrames << "\n";
    std::cout << "  Total Collision Events: " << totalCollisions << "\n";
    std::cout << "  Total Jam Signals Handled: " << totalJamsReceived << "\n";
    std::cout << "  Channel Efficiency: " << std::fixed << std::setprecision(2) << channelEfficiency << " %\n";
    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

    for (auto& pfd : pollFds) {
        ::close(pfd.fd);
    }

    return 0;
}
