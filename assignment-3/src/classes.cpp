#include "classes.h"

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