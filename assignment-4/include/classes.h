#ifndef CLASSES_H
#define CLASSES_H

#include "common.h"
#include <unistd.h>
#include <chrono>
#include <random>
#include <thread>
#include <string>

class SocketWrapper {
public:
    explicit SocketWrapper(int fd = -1) : m_fd(fd) {}
    ~SocketWrapper() { cleanup(); }

    // Prevent copying
    SocketWrapper(const SocketWrapper&)            = delete;
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

    int getFd() const noexcept { return m_fd; }
    bool isValid() const noexcept { return m_fd >= 0; }

    void reset(int fd = -1) {
        cleanup();
        m_fd = fd;
    }

private:
    void cleanup() {
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

private:
    int m_fd{-1};
};

// Simple Stopwatch / Timer utility
class Stopwatch {
    using Clock = std::chrono::steady_clock;
public:
    Stopwatch() : m_start(Clock::now()) {}

    void reset() {
        m_start = Clock::now();
    }

    double elapsedMilliseconds() const {
        auto now = Clock::now();
        return std::chrono::duration<double, std::milli>(now - m_start).count();
    }

    double elapsedSeconds() const {
        return elapsedMilliseconds() / 1000.0;
    }

private:
    Clock::time_point m_start;
};

#endif // CLASSES_H
