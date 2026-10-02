#pragma once

#include <string>
#include <chrono>
#include <cstdint>

// Forward declarations of collaborator classes
class SocketClient;
class BackoffEngine;
struct Frame;

// Channel state enumeration
enum class ChannelState : uint8_t {
    Idle = 0,
    Busy = 1,
    Collision = 2,
    Error = 3
};

// Transmission outcome status
enum class TransmissionStatus : uint8_t {
    Success = 0,
    DroppedCollisionLimit = 1,
    SocketError = 2
};

// Summary metrics resulting from a single frame transmission attempt
struct TransmissionResult {
    TransmissionStatus status{TransmissionStatus::Success};
    int attempts{0};                                            // Total attempts made (initial + retries)
    int collisions{0};                                          // Total collision events experienced
    std::chrono::microseconds totalDelay{0};                    // Total elapsed time from first sense to completion
};

/**
 * @brief Abstract Base Class representing the Medium Access Control (MAC) Strategy.
 * 
 * Subclasses implement specific contention and persistence algorithms:
 * - NonPersistentCsma
 * - OnePersistentCsma
 * - PPersistentCsma
 * - CsmaCd
 */
class MacStrategy {
protected:
    std::string m_strategyName;
    int m_maxRetries;                                           // Maximum retransmission attempts before dropping
    std::chrono::milliseconds m_pollInterval;                   // Interval between carrier-sense checks

public:
    /**
     * @brief Construct a new MacStrategy object
     * 
     * @param name Name of the strategy (for logging/identification)
     * @param maxRetries Maximum retry limit before dropping packet (default: 16)
     * @param pollInterval Carrier sensing poll interval (default: 1ms)
     */
    explicit MacStrategy(std::string name, 
                         int maxRetries = 16, 
                         std::chrono::milliseconds pollInterval = std::chrono::milliseconds(1))
        : m_strategyName(std::move(name)),
          m_maxRetries(maxRetries),
          m_pollInterval(pollInterval) {}

    // Virtual destructor is mandatory for polymorphic base classes
    virtual ~MacStrategy() = default;

    // Prevent slicing (Rule of 5)
    MacStrategy(const MacStrategy&) = delete;
    MacStrategy& operator=(const MacStrategy&) = delete;
    MacStrategy(MacStrategy&&) noexcept = default;
    MacStrategy& operator=(MacStrategy&&) noexcept = default;

    /**
     * @brief Primary Pure Virtual Method: Orchestrates frame transmission according
     *        to the specific contention, sensing, and backoff policy.
     * 
     * @param frame The data frame to be transmitted
     * @param socket The client network socket connected to the Channel Emulator
     * @param backoffEngine PRNG and backoff calculation engine
     * @return TransmissionResult Result statistics for this transmission
     */
    virtual TransmissionResult executeTransmission(const Frame& frame, 
                                                   SocketClient& socket, 
                                                   BackoffEngine& backoffEngine) = 0;

    // Getters for strategy configuration
    const std::string& getName() const { return m_strategyName; }
    int getMaxRetries() const { return m_maxRetries; }
    std::chrono::milliseconds getPollInterval() const { return m_pollInterval; }

protected:
    /**
     * @brief Common protected helper: Queries the Channel Emulator for medium status.
     *        Can be overridden by derived classes if custom sensing is needed.
     * 
     * @param socket The connected client socket
     * @return ChannelState The instantaneous channel state (Idle, Busy, Collision, Error)
     */
    virtual ChannelState carrierSense(SocketClient& socket);
};
