#include "walsh.h"
#include <sstream>
#include <iomanip>

size_t WalshCodeSet::calculateDimension(size_t numStations) {
    if (numStations <= 2) return 2;
    size_t dim = 1;
    while (dim < numStations) {
        dim <<= 1;
    }
    return dim;
}

WalshCodeSet::WalshCodeSet(size_t numStations)
    : m_numStations(numStations),
      m_dimension(calculateDimension(numStations)) {
    generateMatrix();
}

WalshCodeSet::WalshCodeSet(size_t numStations, size_t explicitDimension)
    : m_numStations(numStations),
      m_dimension(explicitDimension) {
    if (m_dimension < m_numStations) {
        m_dimension = calculateDimension(m_numStations);
    }
    generateMatrix();
}

void WalshCodeSet::generateMatrix() {
    // Sylvester's recursive construction
    // Start with H_1 = [[+1]]
    m_matrix = {{1}};
    size_t currentDim = 1;

    while (currentDim < m_dimension) {
        size_t nextDim = currentDim * 2;
        std::vector<std::vector<int8_t>> nextMatrix(nextDim, std::vector<int8_t>(nextDim, 0));

        for (size_t r = 0; r < currentDim; ++r) {
            for (size_t c = 0; c < currentDim; ++c) {
                int8_t val = m_matrix[r][c];
                nextMatrix[r][c]                         = val;   // Top-Left (H_k)
                nextMatrix[r][c + currentDim]             = val;   // Top-Right (H_k)
                nextMatrix[r + currentDim][c]             = val;   // Bottom-Left (H_k)
                nextMatrix[r + currentDim][c + currentDim] = -val;  // Bottom-Right (-H_k)
            }
        }
        m_matrix = std::move(nextMatrix);
        currentDim = nextDim;
    }
}

const std::vector<int8_t>& WalshCodeSet::getCode(size_t stationId) const {
    if (stationId >= m_dimension) {
        throw std::out_of_range("Station ID exceeds Walsh matrix dimension W=" + std::to_string(m_dimension));
    }
    return m_matrix[stationId];
}

bool WalshCodeSet::verifyOrthogonality(std::string* report) const {
    std::ostringstream ss;
    bool allOrthogonal = true;

    ss << "=== Walsh Code Set Orthogonality Verification (W = " << m_dimension << ") ===\n";
    for (size_t i = 0; i < m_dimension; ++i) {
        for (size_t j = i; j < m_dimension; ++j) {
            int dot = 0;
            for (size_t k = 0; k < m_dimension; ++k) {
                dot += m_matrix[i][k] * m_matrix[j][k];
            }

            if (i == j) {
                if (dot != static_cast<int>(m_dimension)) {
                    allOrthogonal = false;
                    ss << "FAIL: Self dot-product C_" << i << " . C_" << i 
                       << " = " << dot << " (expected " << m_dimension << ")\n";
                }
            } else {
                if (dot != 0) {
                    allOrthogonal = false;
                    ss << "FAIL: Cross dot-product C_" << i << " . C_" << j 
                       << " = " << dot << " (expected 0)\n";
                }
            }
        }
    }

    if (allOrthogonal) {
        ss << "SUCCESS: All " << m_dimension << " codes are mutually orthogonal.\n";
        ss << "  Autocorrelation (C_i . C_i) = " << m_dimension << " for all i\n";
        ss << "  Cross-correlation (C_i . C_j) = 0 for all i != j\n";
    }

    if (report) {
        *report = ss.str();
    }
    return allOrthogonal;
}

int WalshCodeSet::innerProduct(const int* v1, const int8_t* v2, size_t len) {
    int sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += v1[i] * v2[i];
    }
    return sum;
}

int WalshCodeSet::innerProduct(const std::vector<int>& v1, const std::vector<int8_t>& v2) {
    size_t len = std::min(v1.size(), v2.size());
    return innerProduct(v1.data(), v2.data(), len);
}

int WalshCodeSet::innerProduct(const std::vector<int8_t>& v1, const std::vector<int8_t>& v2) {
    size_t len = std::min(v1.size(), v2.size());
    int sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += static_cast<int>(v1[i]) * static_cast<int>(v2[i]);
    }
    return sum;
}

std::vector<int8_t> WalshCodeSet::encodeBit(int bitValue, size_t stationId) const {
    const auto& code = getCode(stationId);
    std::vector<int8_t> chips(m_dimension, 0);

    if (bitValue == 1) {
        // Bit 1 -> +1 symbol
        for (size_t i = 0; i < m_dimension; ++i) {
            chips[i] = code[i];
        }
    } else if (bitValue == 0) {
        // Bit 0 -> -1 symbol
        for (size_t i = 0; i < m_dimension; ++i) {
            chips[i] = -code[i];
        }
    } else {
        // Silent / no transmission -> 0 symbol
        // chips remain all 0
    }
    return chips;
}

std::vector<int8_t> WalshCodeSet::encodeByte(uint8_t byteVal, size_t stationId) const {
    std::vector<int8_t> chips;
    chips.reserve(8 * m_dimension);

    // Encode from MSB (bit 7) down to LSB (bit 0)
    for (int bitIdx = 7; bitIdx >= 0; --bitIdx) {
        int bit = (byteVal >> bitIdx) & 1;
        auto bitChips = encodeBit(bit, stationId);
        chips.insert(chips.end(), bitChips.begin(), bitChips.end());
    }
    return chips;
}

std::vector<int8_t> WalshCodeSet::encodeBuffer(const uint8_t* data, size_t size, size_t stationId) const {
    std::vector<int8_t> totalChips;
    totalChips.reserve(size * 8 * m_dimension);

    for (size_t i = 0; i < size; ++i) {
        auto byteChips = encodeByte(data[i], stationId);
        totalChips.insert(totalChips.end(), byteChips.begin(), byteChips.end());
    }
    return totalChips;
}

int WalshCodeSet::decodeBit(const int* compositeChips, size_t stationId) const {
    const auto& code = getCode(stationId);
    int dot = innerProduct(compositeChips, code.data(), m_dimension);

    // Decision rule:
    // If station did not transmit, dot == 0
    // If dot > 0 -> bit 1
    // If dot < 0 -> bit 0
    if (dot == 0) return -1; // Silent
    return (dot > 0) ? 1 : 0;
}

int WalshCodeSet::decodeBit(const std::vector<int>& compositeChips, size_t stationId) const {
    if (compositeChips.size() < m_dimension) {
        throw std::invalid_argument("Composite chips size smaller than dimension W");
    }
    return decodeBit(compositeChips.data(), stationId);
}

uint8_t WalshCodeSet::decodeByte(const int* compositeChips, size_t stationId) const {
    uint8_t byteVal = 0;
    for (int bitIdx = 7; bitIdx >= 0; --bitIdx) {
        const int* bitSlice = compositeChips + (7 - bitIdx) * m_dimension;
        int bit = decodeBit(bitSlice, stationId);
        if (bit == 1) {
            byteVal |= (1 << bitIdx);
        }
        // If bit == 0 or -1, bit remains 0 in byteVal
    }
    return byteVal;
}

std::vector<uint8_t> WalshCodeSet::decodeBuffer(const std::vector<int>& compositeChips, size_t stationId, size_t numBytes) const {
    std::vector<uint8_t> reconstructed;
    reconstructed.reserve(numBytes);

    size_t chipsPerByte = 8 * m_dimension;
    if (compositeChips.size() < numBytes * chipsPerByte) {
        throw std::invalid_argument("Not enough composite chips for requested byte count");
    }

    for (size_t i = 0; i < numBytes; ++i) {
        const int* byteSlice = compositeChips.data() + (i * chipsPerByte);
        reconstructed.push_back(decodeByte(byteSlice, stationId));
    }
    return reconstructed;
}

std::string WalshCodeSet::codeToString(size_t stationId) const {
    const auto& code = getCode(stationId);
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < code.size(); ++i) {
        if (code[i] > 0) ss << "+1";
        else ss << "-1";
        if (i + 1 < code.size()) ss << ", ";
    }
    ss << "]";
    return ss.str();
}

void WalshCodeSet::printMatrix(std::ostream& os) const {
    os << "Walsh-Hadamard Matrix H_" << m_dimension << " (" << m_dimension << "x" << m_dimension << "):\n";
    for (size_t i = 0; i < m_dimension; ++i) {
        os << "  Station " << std::setw(2) << i << " Code: " << codeToString(i) << "\n";
    }
}
