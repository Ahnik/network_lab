#ifndef CSMA_H
#define CSMA_H

#include "classes.h"

class NonPersistent : public MacStrategy {
public:
    explicit NonPersistent(int maxAttempts = 16,
        Milliseconds pollInterval = Milliseconds(1),
        int minSlots = 1, int maxSlots = 8)
            : MacStrategy(Strategy::NON_PERSISTENT, maxAttempts, pollInterval), m_minSlots(minSlots), m_maxSlots(maxSlots)
    {}
    TransmissionResult executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) override;
private:
    int m_minSlots, m_maxSlots;
};

class OnePersistent : public MacStrategy {
public:
    explicit OnePersistent(int maxAttempts = 16, Milliseconds pollInterval = Milliseconds(1))
        : MacStrategy(Strategy::ONE_PERSISTENT, maxAttempts, pollInterval)
    {}
    TransmissionResult executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) override;
};

class PPersistent : public MacStrategy {
    using RealDistribution = std::uniform_real_distribution<double>;
public:
    explicit PPersistent(int maxAttempts = 16, double p = 0.5,
        Milliseconds pollInterval = Milliseconds(1),
        Milliseconds slotDuration = Milliseconds(10))
            : MacStrategy(Strategy::P_PERSISTENT, maxAttempts, pollInterval), m_p(p), m_rng(std::random_device{}()), m_slotDuration(slotDuration)
    {}
    TransmissionResult executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) override;
    double getP() const { return m_p; }
private:
    double m_p;
    std::mt19937 m_rng;
    Milliseconds m_slotDuration;
    RealDistribution m_probDist {0.0, 1.0};
};

class CsmaCd : public MacStrategy {
public:
    explicit CsmaCd(int maxAttempts = 16, Milliseconds pollInterval = Milliseconds(1))
        : MacStrategy(Strategy::CSMA_CD, maxAttempts, pollInterval)
    {}
    TransmissionResult executeTransmission(SocketWrapper& socket, BackoffEngine& backoffEngine, const Frame *frame) override;
};

#endif