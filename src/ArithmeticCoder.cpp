#include "compression/ArithmeticCoder.hpp"
#include <stdexcept>
#include <numeric>

namespace compression {

std::map<uint32_t, std::pair<uint64_t, uint64_t>> ArithmeticCoder::buildProbabilityModel(
    const FrequencyMap& freqMap) const {
    
    // If the frequency map is empty, return an empty model
    if (freqMap.empty()) {
        return {};
    }
    
    // Ensure each symbol has at least a minimum frequency to prevent numeric issues
    // This is especially important for compression of varied data
    FrequencyMap adjustedFreqMap;
    constexpr uint64_t MIN_FREQ = 1;
    
    // First pass: ensure minimum frequency and calculate total
    uint64_t totalFreq = 0;
    for (const auto& [symbol, freq] : freqMap) {
        uint64_t adjustedFreq = std::max(freq, MIN_FREQ);
        adjustedFreqMap[symbol] = adjustedFreq;
        totalFreq += adjustedFreq;
    }
    
    // Create probability model with cumulative frequency ranges
    std::map<uint32_t, std::pair<uint64_t, uint64_t>> probModel;
    uint64_t cumFreq = 0;
    
    // Second pass: calculate cumulative ranges
    for (const auto& [symbol, freq] : adjustedFreqMap) {
        uint64_t low = cumFreq;
        cumFreq += freq;
        uint64_t high = cumFreq;
        probModel[symbol] = {low, high};
    }
    
    // Add EOF symbol with a small frequency
    probModel[EOF_SYMBOL] = {cumFreq, cumFreq + MIN_FREQ};
    
    return probModel;
}

std::vector<uint8_t> ArithmeticCoder::encode(
    const std::vector<uint32_t>& symbols,
    const std::map<uint32_t, std::pair<uint64_t, uint64_t>>& probModel,
    uint64_t totalFreq) const {
    
    // If input is empty, return empty output
    if (symbols.empty()) {
        return {};
    }
    
    // Initialize arithmetic coding state
    Code low = 0;
    Code high = TOP_VALUE;
    int underflow_bits = 0;
    
    std::vector<bool> encodedBits;
    
    // Process each symbol
    for (const auto& symbol : symbols) {
        // Look up symbol in probability model
        auto it = probModel.find(symbol);
        if (it == probModel.end()) {
            throw std::runtime_error("Symbol not found in probability model");
        }
        
        const auto& [low_range, high_range] = it->second;
        
        // Update range based on symbol probabilities
        // Using the standard arithmetic coding range narrowing formula
        uint64_t range = static_cast<uint64_t>(high - low) + 1;
        high = low + (range * high_range) / totalFreq - 1;
        low = low + (range * low_range) / totalFreq;
        
        // E1/E2/E3 scaling for infinite precision estimation
        for (;;) {
            // If high and low share the same MSB
            if ((high & HALF) == (low & HALF)) {
                // Output the MSB and any pending bits
                bool bit = (high & HALF) != 0;
                encodedBits.push_back(bit);
                
                // Output pending underflow bits (all opposite to the current bit)
                for (int i = 0; i < underflow_bits; ++i)
                    encodedBits.push_back(!bit);
                underflow_bits = 0;
            }
            // Check for underflow condition (E2 scaling)
            else if ((low & FIRST_QTR) && !(high & FIRST_QTR)) {
                // The MSBs are different, but the second bits are the same (opposite)
                // This is the underflow case - need to track pending bits
                underflow_bits++;
                // Remove second MSB of low, set second MSB of high
                low &= ~HALF;
                high |= HALF;
                // Shift away the MSB (which is now guaranteed to be the same)
                low &= ~FIRST_QTR;
                high &= ~FIRST_QTR;
                low <<= 1;
                high = (high << 1) | 1;
            }
            else {
                // No more scaling possible at this step
                break;
            }
        }
    }
    
    // Process EOF symbol
    auto eof_it = probModel.find(EOF_SYMBOL);
    if (eof_it != probModel.end()) {
        const auto& [low_range, high_range] = eof_it->second;
        
        // Update range for EOF symbol
        uint64_t range = static_cast<uint64_t>(high - low) + 1;
        high = low + (range * high_range) / (totalFreq + 1) - 1;
        low = low + (range * low_range) / (totalFreq + 1);
    }
    
    // Finalize encoding - we need at least two bits to identify the final range
    underflow_bits += 2;
    if (low < FIRST_QTR) {
        // Output 0 followed by underflow_bits ones
        encodedBits.push_back(0);
        for (int i = 0; i < underflow_bits; ++i)
            encodedBits.push_back(1);
    } else {
        // Output 1 followed by underflow_bits zeros
        encodedBits.push_back(1);
        for (int i = 0; i < underflow_bits; ++i)
            encodedBits.push_back(0);
    }
    
    // Compress the bit stream into bytes
    std::vector<uint8_t> compressedData;
    uint8_t currentByte = 0;
    int bitPosition = 0;
    
    for (const auto& bit : encodedBits) {
        // Set the bit in the current byte
        if (bit) {
            currentByte |= (1 << (7 - bitPosition));
        }
        
        // Move to next bit position
        ++bitPosition;
        
        // If we have a complete byte, add it to the output
        if (bitPosition == 8) {
            compressedData.push_back(currentByte);
            currentByte = 0;
            bitPosition = 0;
        }
    }
    
    // Add any remaining bits as a partial byte
    if (bitPosition > 0) {
        compressedData.push_back(currentByte);
    }
    
    return compressedData;
}

std::map<uint64_t, uint32_t> ArithmeticCoder::createReverseMapping(
    const std::map<uint32_t, std::pair<uint64_t, uint64_t>>& probModel) const {
    
    std::map<uint64_t, uint32_t> reverseMap;
    
    for (const auto& [symbol, range] : probModel) {
        reverseMap[range.first] = symbol;
    }
    
    return reverseMap;
}

std::vector<uint32_t> ArithmeticCoder::decode(
    const std::vector<uint8_t>& encodedData,
    const std::map<uint32_t, std::pair<uint64_t, uint64_t>>& probModel,
    uint64_t totalFreq,
    size_t numSymbols) const {
    
    // If encoded data is empty, return empty output
    if (encodedData.empty()) {
        return {};
    }
    
    // Decompress bytes into bits
    std::vector<bool> bits;
    bits.reserve(encodedData.size() * 8);
    
    // Extract individual bits from the bytes
    for (const auto& byte : encodedData) {
        for (int i = 0; i < 8; ++i) {
            bool bit = (byte & (1 << (7 - i))) != 0;
            bits.push_back(bit);
        }
    }
    
    // Initialize arithmetic decoding state
    Code low = 0;
    Code high = TOP_VALUE;
    Code value = 0;
    
    // Read initial bits to fill the code value
    size_t bitIndex = 0;
    for (int i = 0; i < CODE_BITS && bitIndex < bits.size(); ++i) {
        value = (value << 1) | (bits[bitIndex++] ? 1 : 0);
    }
    
    std::vector<uint32_t> decodedSymbols;
    decodedSymbols.reserve(numSymbols);
    
    // Decode symbols
    while (decodedSymbols.size() < numSymbols) {
        // Find the symbol for the current value
        uint64_t range = static_cast<uint64_t>(high - low) + 1;
        uint64_t scaledValue = ((static_cast<uint64_t>(value) - low) * totalFreq) / range;
        
        // Find the symbol that corresponds to this value
        uint32_t symbol = 0;
        bool symbolFound = false;
        
        for (const auto& [sym, range_pair] : probModel) {
            // Skip the EOF symbol for regular decoding
            if (sym == EOF_SYMBOL) {
                continue;
            }
            
            const auto& [low_range, high_range] = range_pair;
            if (low_range <= scaledValue && scaledValue < high_range) {
                symbol = sym;
                symbolFound = true;
                break;
            }
        }
        
        // If we couldn't find a symbol, check if it's an EOF
        if (!symbolFound) {
            // Check for EOF
            uint64_t eofScaledValue = ((static_cast<uint64_t>(value) - low) * (totalFreq + 1)) / range;
            auto eofIt = probModel.find(EOF_SYMBOL);
            if (eofIt != probModel.end() && 
                eofIt->second.first <= eofScaledValue && 
                eofScaledValue < eofIt->second.second) {
                // Found EOF, we're done
                break;
            }
            
            // If we get here, we have an error
            throw std::runtime_error("Invalid state in arithmetic decoding: no matching symbol");
        }
        
        // Add symbol to decoded output
        decodedSymbols.push_back(symbol);
        
        // Update the range based on the symbol
        const auto& [low_range, high_range] = probModel.at(symbol);
        uint64_t old_range = static_cast<uint64_t>(high - low) + 1;
        high = low + (old_range * high_range) / totalFreq - 1;
        low = low + (old_range * low_range) / totalFreq;
        
        // E1/E2/E3 scaling to match encoder's scaling
        for (;;) {
            // E1/E3 scaling - MSBs are the same
            if ((high & HALF) == (low & HALF)) {
                // Shift out the MSB
                low = (low << 1) & TOP_VALUE;
                high = ((high << 1) & TOP_VALUE) | 1;
                value = ((value << 1) & TOP_VALUE) | (bitIndex < bits.size() ? (bits[bitIndex++] ? 1 : 0) : 0);
            }
            // E2 scaling - Underflow prevention
            else if ((low & FIRST_QTR) && !(high & FIRST_QTR)) {
                // Shift out the second MSB, complemented
                low &= ~HALF;     // Remove second MSB of low
                high |= HALF;     // Set second MSB of high
                value ^= HALF;    // Flip second MSB of value
                
                // Now shift normally
                low = (low << 1) & TOP_VALUE;
                high = ((high << 1) & TOP_VALUE) | 1;
                value = ((value << 1) & TOP_VALUE) | (bitIndex < bits.size() ? (bits[bitIndex++] ? 1 : 0) : 0);
            }
            else {
                break;
            }
        }
    }
    
    return decodedSymbols;
}

} // namespace compression 