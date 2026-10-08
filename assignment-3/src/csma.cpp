#include "csma.h"

TransmissionResult NonPersistent::executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) {
    TransmissionResult result;
    auto startTime = SteadyClock::now();
    backoffEngine.reset();

    while (true) {
        // Carrier sensing
        ChannelState state = ChannelState::CHANNEL_STATES;
        while (state != ChannelState::IDLE) {
            state = carrierSense(socket);
            switch (state) {
                case ChannelState::BUSY:
                case ChannelState::COLLISION:
                    backoffEngine.sleepUniform(m_minSlots, m_maxSlots);
                    break;
                case ChannelState::IDLE:
                    break;
                default:
                    result.status = TransmissionStatus::SOCKET_ERROR;
                    auto endTime = SteadyClock::now();
                    result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
                    return result;
            }
        }

        // Transmission attempt
        result.attempts++;
        TransmissionResult transmissionResult = transmitFrameData(socket, backoffEngine, frame);
        if (transmissionResult.status == TransmissionStatus::SUCCESS) {
            result.status = TransmissionStatus::SUCCESS;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }

        // Collision resolution
        result.collisions++;
        if (!backoffEngine.increment()) {
            result.status = TransmissionStatus::DROPPED_COLLISION_LIMIT;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }

        backoffEngine.sleep();
    }
}

TransmissionResult OnePersistent::executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) {
    TransmissionResult result;
    auto startTime = SteadyClock::now();
    backoffEngine.reset();

    while (true) {
        // Carrier sensing
        ChannelState state = ChannelState::CHANNEL_STATES;
        while (state != ChannelState::IDLE) {
            state = carrierSense(socket);
            switch (state) {
                case ChannelState::BUSY:
                case ChannelState::COLLISION:
                    std::this_thread::sleep_for(m_pollInterval);
                    break;
                case ChannelState::IDLE:
                    break;
                default:
                    result.status = TransmissionStatus::SOCKET_ERROR;
                    auto endTime = SteadyClock::now();
                    result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
                    return result;
            }
        }

        // Transmission attempt
        result.attempts++;
        TransmissionResult transmissionResult = transmitFrameData(socket, backoffEngine, frame);
        if (transmissionResult.status == TransmissionStatus::SUCCESS) {
            result.status = TransmissionStatus::SUCCESS;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }

        // Collision resolution
        result.collisions++;
        if (!backoffEngine.increment()) {
            result.status = TransmissionStatus::DROPPED_COLLISION_LIMIT;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }

        backoffEngine.sleep();
    }
}

TransmissionResult PPersistent::executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) {
    TransmissionResult result;
    auto startTime = SteadyClock::now();
    backoffEngine.reset();

    while (true) {
        // Carrier sensing
        ChannelState state = ChannelState::CHANNEL_STATES;
        double q;
        while (true) {
            state = carrierSense(socket);
            if (state == ChannelState::BUSY || state == ChannelState::COLLISION)
                std::this_thread::sleep_for(m_pollInterval);
            else if (state == ChannelState::IDLE) {
                double q = m_probDist(m_rng);
                if (q <= m_p)
                    break;
                else
                    std::this_thread::sleep_for(m_slotDuration);
            } else {
                result.status = TransmissionStatus::SOCKET_ERROR;
                auto endTime = SteadyClock::now();
                result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
                return result;
            }
        }

        // Transmission attempt
        result.attempts++;
        TransmissionResult transmissionResult = transmitFrameData(socket, backoffEngine, frame);
        if (transmissionResult.status == TransmissionStatus::SUCCESS) {
            result.status = TransmissionStatus::SUCCESS;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }

        // Collision resolution
        result.collisions++;
        if (!backoffEngine.increment()) {
            result.status = TransmissionStatus::DROPPED_COLLISION_LIMIT;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }

        backoffEngine.sleep();
    }
}

TransmissionResult CsmaCd::executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) {
    TransmissionResult result;
    auto startTime = SteadyClock::now();
    backoffEngine.reset();

    while (true) {
        // Channel sensing
        ChannelState state = ChannelState::CHANNEL_STATES;
        while (state != ChannelState::IDLE) {
            state = carrierSense(socket);
            switch (state) {
                case ChannelState::BUSY:
                case ChannelState::COLLISION:
                    break;
                case ChannelState::IDLE:
                    break;
                default:
                    result.status = TransmissionStatus::SOCKET_ERROR;
                    auto endTime = SteadyClock::now();
                    result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
                    return result;
            }
        }

        // Transmission attempt
        result.attempts++;
        TransmissionResult transmissionResult = transmitFrameData(socket, backoffEngine, frame);

        if (transmissionResult.status == TransmissionStatus::SUCCESS) {
            result.status = TransmissionStatus::SUCCESS;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        } else if (transmissionResult.status == TransmissionStatus::SOCKET_ERROR) {
            result.status = TransmissionStatus::SOCKET_ERROR;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }

        result.collisions++;
        if (!backoffEngine.increment()) {
            result.status = TransmissionStatus::DROPPED_COLLISION_LIMIT;
            auto endTime = SteadyClock::now();
            result.totalDelay = std::chrono::duration_cast<Milliseconds>(endTime - startTime);
            return result;
        }
        backoffEngine.sleep();
    }
}