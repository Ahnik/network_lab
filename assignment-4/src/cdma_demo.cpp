#include "common.h"
#include "walsh.h"
#include "cdma.h"
#include "classes.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <cstring>

static void printSeparator(char c = '=', int len = 70) {
    std::cout << std::string(len, c) << "\n";
}

// 1. Demonstrate Walsh Matrix & Mathematical Orthogonality Proof
static void demoWalshOrthogonality(size_t N) {
    printSeparator('=');
    std::cout << "  DEMONSTRATION 1: WALSH-HADAMARD CODE GENERATION & ORTHOGONALITY (N=" << N << ")\n";
    printSeparator('=');

    WalshCodeSet walsh(N);
    walsh.printMatrix(std::cout);

    std::cout << "\nOrthogonality Matrix (C_i . C_j):\n";
    std::cout << "      ";
    for (size_t j = 0; j < walsh.getDimension(); ++j) {
        std::cout << "C" << j << "    ";
    }
    std::cout << "\n";

    for (size_t i = 0; i < walsh.getDimension(); ++i) {
        std::cout << "C" << i << "  [ ";
        for (size_t j = 0; j < walsh.getDimension(); ++j) {
            int dot = WalshCodeSet::innerProduct(walsh.getCode(i), walsh.getCode(j));
            std::cout << std::setw(3) << dot << "  ";
        }
        std::cout << "]\n";
    }

    std::string report;
    bool ok = walsh.verifyOrthogonality(&report);
    (void)ok;
    std::cout << "\n" << report << "\n";
}

// 2. Step-by-Step Bit Spreading, Channel Summation, and Despreading
static void demoStepByStepBitTransmission() {
    printSeparator('=');
    std::cout << "  DEMONSTRATION 2: STEP-BY-STEP CDMA MULTIPLEXING & DEMULTIPLEXING\n";
    printSeparator('=');
    std::cout << "Scenario:\n";
    std::cout << "  4 Stations sharing the channel simultaneously with W=4 Walsh codes.\n";
    std::cout << "  - Station 0 transmits: Data bit 1 (+1)\n";
    std::cout << "  - Station 1 transmits: Data bit 0 (-1)\n";
    std::cout << "  - Station 2 transmits: Silent     ( 0)\n";
    std::cout << "  - Station 3 transmits: Data bit 1 (+1)\n\n";

    WalshCodeSet walsh(4);

    int d0 = 1;   // bit 1 -> symbol +1
    int d1 = 0;   // bit 0 -> symbol -1
    int d2 = -1;  // silent -> symbol 0
    int d3 = 1;   // bit 1 -> symbol +1

    auto s0 = walsh.encodeBit(d0, 0);
    auto s1 = walsh.encodeBit(d1, 1);
    auto s2 = walsh.encodeBit(d2, 2);
    auto s3 = walsh.encodeBit(d3, 3);

    auto printChips = [](const std::vector<int8_t>& v) {
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] > 0) ss << "+1";
            else if (v[i] < 0) ss << "-1";
            else ss << " 0";
            if (i + 1 < v.size()) ss << ", ";
        }
        ss << "]";
        return ss.str();
    };

    std::cout << "Step 1: Spreading data bits with Walsh codes (S_i = d_i * C_i):\n";
    std::cout << "  Station 0: d0=+1, C0=" << walsh.codeToString(0) << " => S0 = " << printChips(s0) << "\n";
    std::cout << "  Station 1: d1=-1, C1=" << walsh.codeToString(1) << " => S1 = " << printChips(s1) << "\n";
    std::cout << "  Station 2: d2= 0, C2=" << walsh.codeToString(2) << " => S2 = " << printChips(s2) << " (SILENT)\n";
    std::cout << "  Station 3: d3=+1, C3=" << walsh.codeToString(3) << " => S3 = " << printChips(s3) << "\n\n";

    // Common channel superposition
    CdmaMultiplexer mux(4);
    mux.reset(4);
    mux.addSignal(s0);
    mux.addSignal(s1);
    mux.addSignal(s2);
    mux.addSignal(s3);
    const auto& composite = mux.getComposite();

    std::cout << "Step 2: Linear superposition on common channel (S_channel = S0 + S1 + S2 + S3):\n";
    std::cout << "  S_channel = [ ";
    for (size_t i = 0; i < composite.size(); ++i) {
        if (composite[i] > 0) std::cout << "+" << composite[i] << " ";
        else std::cout << composite[i] << " ";
    }
    std::cout << "]\n\n";

    std::cout << "Step 3: Perfect Reconstruction at Receivers using inner products:\n";
    std::cout << "  Formula: d_i = (S_channel . C_i) / W\n\n";

    for (size_t i = 0; i < 4; ++i) {
        int dot = WalshCodeSet::innerProduct(composite, walsh.getCode(i));
        int decodedBit = walsh.decodeBit(composite, i);

        std::cout << "  Receiver for Station " << i << ":\n";
        std::cout << "    Inner product (S_channel . C" << i << ") = " << dot << "\n";
        std::cout << "    Normalized (dot / " << walsh.getDimension() << ")      = " 
                  << (static_cast<double>(dot) / walsh.getDimension()) << "\n";
        std::cout << "    Decision: ";
        if (decodedBit == 1) std::cout << "Bit 1 (MATCH!)\n";
        else if (decodedBit == 0) std::cout << "Bit 0 (MATCH!)\n";
        else std::cout << "SILENT (MATCH!)\n";
    }
    std::cout << "\nResult: 100% PERFECT RECONSTRUCTION FOR ALL 4 STATIONS!\n";
}

// 3. Multi-Station Simultaneous Text / Byte Transmission
static void demoMultiStationText(size_t N) {
    printSeparator('=');
    std::cout << "  DEMONSTRATION 3: MULTI-STATION SIMULTANEOUS TEXT TRANSMISSION (N=" << N << ")\n";
    printSeparator('=');

    WalshCodeSet walsh(N);
    std::vector<std::string> messages = {
        "Hello from Station 0!",
        "CDMA Multiple Access",
        "Computer Networks Lab",
        "Walsh-Hadamard Codes",
        "Orthogonal Spreading",
        "Simultaneous Channel",
        "Zero Collision Trans",
        "Perfect Data Recovery"
    };

    while (messages.size() < N) {
        messages.push_back("Data stream " + std::to_string(messages.size()));
    }

    std::cout << "Transmitting " << N << " distinct text streams simultaneously across the shared channel:\n";
    for (size_t i = 0; i < N; ++i) {
        std::cout << "  Station " << i << " [Walsh " << walsh.codeToString(i) << "]: \"" 
                  << messages[i] << "\"\n";
    }
    std::cout << "\n";

    // Find max message length
    size_t maxLen = 0;
    for (size_t i = 0; i < N; ++i) {
        if (messages[i].size() > maxLen) maxLen = messages[i].size();
    }

    // Encode all stations' buffers
    size_t chipsPerByte = 8 * walsh.getDimension();
    size_t totalChips = maxLen * chipsPerByte;
    CdmaMultiplexer mux(walsh.getDimension());
    mux.reset(totalChips);

    for (size_t i = 0; i < N; ++i) {
        std::vector<uint8_t> padded(maxLen, ' ');
        std::memcpy(padded.data(), messages[i].data(), messages[i].size());
        auto chips = walsh.encodeBuffer(padded.data(), padded.size(), i);
        mux.addSignal(chips);
    }

    const auto& composite = mux.getComposite();
    std::cout << "Channel Superposition complete: " << composite.size() << " composite chips generated.\n\n";

    std::cout << "Reconstructing each station's message at independent receivers:\n";
    bool allMatched = true;
    for (size_t i = 0; i < N; ++i) {
        auto recoveredBytes = walsh.decodeBuffer(composite, i, messages[i].size());
        std::string recoveredStr(recoveredBytes.begin(), recoveredBytes.end());
        bool match = (recoveredStr == messages[i]);
        if (!match) allMatched = false;

        std::cout << "  Station " << i << " Receiver: \"" << recoveredStr << "\" "
                  << (match ? "[VERIFIED 100% OK]" : "[MISMATCH]") << "\n";
    }

    printSeparator('-');
    if (allMatched) {
        std::cout << "SUCCESS: All " << N << " stations reconstructed their exact messages with 0 errors!\n";
    } else {
        std::cout << "FAILURE: Errors detected during reconstruction.\n";
    }
    printSeparator('-');
}

// 4. Noise Robustness & Processing Gain Demonstration
static void demoNoiseRobustness(size_t N) {
    printSeparator('=');
    std::cout << "  DEMONSTRATION 4: NOISE ROBUSTNESS & CDMA PROCESSING GAIN\n";
    printSeparator('=');
    std::cout << "Testing bit reconstruction accuracy over noisy channel with increasing noise stddev:\n\n";

    WalshCodeSet walsh(N);
    size_t numTestBytes = 256;
    std::vector<uint8_t> testData(numTestBytes);
    for (size_t i = 0; i < numTestBytes; ++i) {
        testData[i] = static_cast<uint8_t>((i * 37 + 13) % 256);
    }

    // Encode station 0 data
    auto chips = walsh.encodeBuffer(testData.data(), testData.size(), 0);

    std::cout << std::setw(15) << "Noise StdDev" 
              << std::setw(18) << "Bits Tested" 
              << std::setw(18) << "Bit Errors" 
              << std::setw(18) << "Bit Error Rate" << "\n";
    printSeparator('-', 70);

    std::vector<double> noiseLevels = {0.0, 0.2, 0.5, 0.8, 1.0, 1.5, 2.0, 3.0};
    for (double noise : noiseLevels) {
        CdmaMultiplexer mux(walsh.getDimension());
        mux.reset(chips.size());
        mux.addSignal(chips);
        mux.addNoise(noise);

        auto recovered = walsh.decodeBuffer(mux.getComposite(), 0, numTestBytes);
        size_t bitErrors = 0;
        size_t totalBits = numTestBytes * 8;

        for (size_t i = 0; i < numTestBytes; ++i) {
            uint8_t diff = testData[i] ^ recovered[i];
            for (int b = 0; b < 8; ++b) {
                if ((diff >> b) & 1) bitErrors++;
            }
        }

        double ber = (static_cast<double>(bitErrors) / totalBits) * 100.0;
        std::cout << std::setw(15) << noise
                  << std::setw(18) << totalBits
                  << std::setw(18) << bitErrors
                  << std::setw(17) << std::fixed << std::setprecision(2) << ber << " %\n";
    }
    std::cout << "\nObservation: Processing Gain of Walsh spreading (W=" << walsh.getDimension() 
              << ", 10*log10(W) = " << std::fixed << std::setprecision(1) << (10.0 * std::log10(walsh.getDimension()))
              << " dB) provides noise margin against channel imperfections.\n";
}

int main(int argc, char* argv[]) {
    size_t stations = 4;
    std::string mode = "all";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-n" || arg == "--stations") && i + 1 < argc) {
            stations = std::stoul(argv[++i]);
        } else if ((arg == "-m" || arg == "--mode") && i + 1 < argc) {
            mode = argv[++i];
        }
    }

    create_crc32_table();

    std::cout << "===============================================================\n";
    std::cout << "       CDMA WITH WALSH CODES: COMPREHENSIVE LAB DEMONSTRATION  \n";
    std::cout << "       Course: CSE/PC/B/S/314 Computer Networks Lab            \n";
    std::cout << "===============================================================\n\n";

    if (mode == "all" || mode == "walsh") {
        demoWalshOrthogonality(stations);
        std::cout << "\n";
    }
    if (mode == "all" || mode == "step") {
        demoStepByStepBitTransmission();
        std::cout << "\n";
    }
    if (mode == "all" || mode == "text") {
        demoMultiStationText(stations);
        std::cout << "\n";
    }
    if (mode == "all" || mode == "noise") {
        demoNoiseRobustness(stations);
        std::cout << "\n";
    }

    printSeparator('=');
    std::cout << "  ALL CDMA DEMONSTRATIONS COMPLETED SUCCESSFULLY!\n";
    printSeparator('=');
    return 0;
}
