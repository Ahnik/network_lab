#include "classes.h"
#include "common.h"
#include <sys/socket.h>
#include <sys/poll.h>

void BackoffEngine::reset() {
    m_attempts = 0;
    m_dist.param(Distribution::param_type(0, 1));
}

bool BackoffEngine::increment() {
    if (m_attempts == m_maxAttempts) return false;
    m_attempts++;
    if (m_attempts <= 10)
        m_dist.param(Distribution::param_type(0, (1 << m_attempts) - 1));
    return true;
}

void BackoffEngine::sleep() {
    int R = m_dist(m_rng);
    std::this_thread::sleep_for(R * m_slotDuration);
}

void BackoffEngine::sleepUniform(int minSlots, int maxSlots) {
    Distribution local_dist(minSlots, maxSlots);
    int slots = local_dist(m_rng);
    std::this_thread::sleep_for(slots * m_slotDuration);
}

ChannelState MacStrategy::carrierSense(SocketWrapper& socket) {
    int fd = socket.getFd();
    if (fd < 0) return ChannelState::CHANNEL_STATES;

    // send carrier sense request
    uint8_t req = SENSE_REQ_TYPE;
    if (send(fd, &req, sizeof(req), 0) <= 0)
        return ChannelState::CHANNEL_STATES;

    // Read the channel state response
    uint8_t stateByte = 0;
    if (recv(fd, &stateByte, sizeof(stateByte), 0) <= 0)
        return ChannelState::CHANNEL_STATES;

    switch (stateByte) {
        case 0:     return ChannelState::IDLE;
        case 1:     return ChannelState::BUSY;
        case 2:     return ChannelState::COLLISION;
        default:    return ChannelState::CHANNEL_STATES;
    }
}

TransmissionResult MacStrategy::transmitFrameData(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) {
    TransmissionResult result;
    int fd = socket.getFd();
    if (fd < 0 || frame == nullptr) {
        result.status = TransmissionStatus::SOCKET_ERROR;
        return result;
    }

    if (m_strategy != Strategy::CSMA_CD) {
        if (send(fd, frame, sizeof(Frame), 0) <= 0) {
            result.status = TransmissionStatus::SOCKET_ERROR;
            return result;
        }

        ControlFrame ackFrame;
        if (recv(fd, &ackFrame, sizeof(ControlFrame), 0) <= 0) {
            result.status = TransmissionStatus::SOCKET_ERROR;
            return result;
        }

        if (ackFrame.frame_type == ACK_FRAME)
            result.status = TransmissionStatus::SUCCESS;
        else
            result.status = TransmissionStatus::DROPPED_COLLISION_LIMIT;
        return result;
    } else {
        if (send(fd, frame, sizeof(Frame), 0) <= 0) {
            result.status = TransmissionStatus::SOCKET_ERROR;
            return result;
        }

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 20);

        if (ret > 0 && (pfd.revents & POLLIN)) {
            ControlFrame controlFrame;
            ssize_t bytes = recv(fd, &controlFrame, sizeof(ControlFrame), 0);
            if (bytes <= 0) {
                result.status = TransmissionStatus::SOCKET_ERROR;
                return result;
            }

            if (controlFrame.frame_type == COLLISION_ALERT_TYPE) {
                uint8_t jam = JAM_SIGNAL_TYPE;
                send(fd, &jam, sizeof(jam), 0);
                result.status = TransmissionStatus::DROPPED_COLLISION_LIMIT;
                return result;
            } else if (controlFrame.frame_type == ACK_FRAME) {
                result.status = TransmissionStatus::SUCCESS;
                return result;
            }
        }

        ControlFrame finalAck;
        if (recv(fd, &finalAck, sizeof(finalAck), 0) <= 0)
            result.status = TransmissionStatus::SUCCESS;
        else
            result.status = TransmissionStatus::DROPPED_COLLISION_LIMIT;
        return result;
    }
}