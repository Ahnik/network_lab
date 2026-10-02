#ifndef CLASSES_H
#define CLASSES_H

#include "common.h"
#include <unistd.h>
#include <chrono>
#include <random>
#include <thread>

using Distribution = std::uniform_int_distribution<int>;

enum class ChannelState {
    IDLE = 0,
    BUSY,
    COLLISION,
    CHANNEL_STATES,
};

enum class TransmissionStatus {
    SUCCESS = 0,
    DROPPED_COLLISION_LIMIT,
    SOCKET_ERROR,
    TRANSMISSION_STATUS,
};

enum class Strategy {
    NON_PERSISTENT = 0,
    ONE_PERSISTENT,
    P_PERSISTENT,
    CSMA_CD,
    STRATEGIES,
};

struct TransmissionResult {
    TransmissionStatus status {TransmissionStatus::SUCCESS};
    int attempts {0};
    int collisions {0};
    std::chrono::milliseconds totalDelay {0};
};

class BackoffEngine {
public:
    BackoffEngine(const std::chrono::milliseconds& slotDuration, int maxAttempts = 16) 
        : m_slotDuration(slotDuration), m_maxAttempts(maxAttempts), m_rng(std::random_device{}()), m_dist(Distribution(0, 1))
    {}
    BackoffEngine(const BackoffEngine&)            = delete;
    BackoffEngine& operator=(const BackoffEngine&) = delete;

    int getAttempts() const { return m_attempts; }
    void reset();
    bool increment();
    void sleep();
    void sleepUniform(int minSlots, int maxSlots);
private:
    int m_attempts {0};
    std::chrono::milliseconds m_slotDuration;
    int m_maxAttempts;
    std::mt19937 m_rng;
    Distribution m_dist;
};

class SocketWrapper {
public:
    explicit SocketWrapper(int fd = -1) : m_fd(fd) {}
    ~SocketWrapper() { cleanup(); }

    // Prevent copying
    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;

    // Allow moving
    SocketWrapper(SocketWrapper&& other) noexcept : m_fd(other.m_fd) {
        other.m_fd = -1;
    }

    SocketWrapper& operator=(SocketWrapper&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }

    int getFd() const { return m_fd; }

private:
    void cleanup() {
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
    }

private:
    int m_fd {-1};
};

class MacStrategy {
public:
    explicit MacStrategy(Strategy strategy, int maxAttempts = 16, std::chrono::milliseconds pollInterval = std::chrono::milliseconds(1))
        : m_strategy(strategy), m_maxAttempts(maxAttempts), m_pollInterval(pollInterval)
    {}
    virtual ~MacStrategy()                         = default;
    MacStrategy(const MacStrategy&)                = delete;
    MacStrategy& operator=(const MacStrategy&)     = delete;
    MacStrategy(MacStrategy&&) noexcept            = default;
    MacStrategy& operator=(MacStrategy&&) noexcept = default;
    virtual TransmissionResult executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) = 0;

    // Getter functions
    Strategy getStrategy()   const { return m_strategy; }
    int      getMaxRetries() const { return m_maxAttempts; }
protected:
    virtual ChannelState carrierSense(SocketWrapper& socket);
    TransmissionResult transmitFrameData(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame);
protected:
    Strategy m_strategy;
    int m_maxAttempts;                              // Maximum retries
    std::chrono::milliseconds m_pollInterval;       // Interval between carrier-sense checks
};

#endif