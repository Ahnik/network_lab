#ifndef WALSH_H
#define WALSH_H

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <stdexcept>

class WalshCodeSet {
public:
    // Construct Walsh codes for at least numStations (W is smallest power of 2 >= numStations)
    explicit WalshCodeSet(size_t numStations = 4);
    WalshCodeSet(size_t numStations, size_t explicitDimension);

    // Getters
    size_t getDimension() const noexcept { return m_dimension; }
    size_t getNumStations() const noexcept { return m_numStations; }
    const std::vector<int8_t>& getCode(size_t stationId) const;

    // Mathematical verification of orthogonality
    // Returns true if C_i . C_j = 0 for all i != j, and C_i . C_i = W for all i
    bool verifyOrthogonality(std::string* report = nullptr) const;

    // Compute inner product of two vectors of chips
    static int innerProduct(const int* v1, const int8_t* v2, size_t len);
    static int innerProduct(const std::vector<int>& v1, const std::vector<int8_t>& v2);
    static int innerProduct(const std::vector<int8_t>& v1, const std::vector<int8_t>& v2);

    // Spreading (Encoding) operations
    // bitValue: 1 -> +1, 0 -> -1, -1 -> silent (0)
    std::vector<int8_t> encodeBit(int bitValue, size_t stationId) const;

    // Encode a single byte (8 bits MSB first) into 8 * W chips
    std::vector<int8_t> encodeByte(uint8_t byteVal, size_t stationId) const;

    // Encode a byte buffer into buffer.size() * 8 * W chips
    std::vector<int8_t> encodeBuffer(const uint8_t* data, size_t size, size_t stationId) const;

    // Despreading (Decoding) operations
    // Decodes a W-length slice of composite channel chips
    // Returns: 1 (bit 1), 0 (bit 0), or -1 (silent/no transmission)
    int decodeBit(const int* compositeChips, size_t stationId) const;
    int decodeBit(const std::vector<int>& compositeChips, size_t stationId) const;

    // Decode 8 * W composite chips back into a single byte
    uint8_t decodeByte(const int* compositeChips, size_t stationId) const;

    // Decode composite chips back into a vector of bytes
    std::vector<uint8_t> decodeBuffer(const std::vector<int>& compositeChips, size_t stationId, size_t numBytes) const;

    // Utility formatting and display
    std::string codeToString(size_t stationId) const;
    void printMatrix(std::ostream& os = std::cout) const;

    // Smallest power of 2 >= n (minimum 2)
    static size_t calculateDimension(size_t numStations);

private:
    void generateMatrix();

private:
    size_t m_numStations;
    size_t m_dimension;                               // W (code length / spreading factor)
    std::vector<std::vector<int8_t>> m_matrix;        // H_W matrix (W x W), elements +1 or -1
};

#endif // WALSH_H
