#include "compression/ArithmeticCoder.hpp"
#include <stdexcept>
#include <numeric>
#include <algorithm>

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
    
    // Safety check - limit total frequency to avoid numeric overflow
    if (totalFreq > (1ULL << 32)) {
        // Scale down frequencies if total is too large
        double scale = static_cast<double>((1ULL << 30)) / totalFreq;
        totalFreq = 0;
        
        for (auto& [symbol, freq] : adjustedFreqMap) {
            freq = std::max(static_cast<uint64_t>(freq * scale), MIN_FREQ);
            totalFreq += freq;
        }
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

    // If there are no symbols to encode, return empty result
    if (symbols.empty()) {
        return {};
    }
    
    // Safety check for probModel
    if (probModel.empty()) {
        throw std::runtime_error("Empty probability model");
    }
    
    // Safety check for totalFreq
    if (totalFreq == 0) {
        throw std::runtime_error("Total frequency cannot be zero");
    }
    
    // Initialize arithmetic coding state
    Code low = 0;
    Code high = TOP_VALUE;
    int underflow_bits = 0;
    
    // Output bits
    std::vector<bool> encodedBits;
    
    // Encode each symbol
    for (const auto& symbol : symbols) {
        // Find the probability range for this symbol
        auto it = probModel.find(symbol);
        if (it == probModel.end()) {
            // Symbol not in probability model
            throw std::runtime_error("Symbol not found in probability model");
        }
        
        const auto& [low_range, high_range] = it->second;
        
        // Verify probability ranges
        if (low_range >= high_range || high_range > totalFreq) {
            throw std::runtime_error("Invalid probability ranges");
        }
        
        // Calculate the range for this symbol
        uint64_t range = static_cast<uint64_t>(high - low) + 1;
        
        // Safeguard against range underflow
        if (range <= 1) {
            // Reset the range as emergency measure
            low = 0;
            high = TOP_VALUE;
            range = static_cast<uint64_t>(high - low) + 1;
        }
        
        // Update high and low bounds
        Code new_high = low + (range * high_range) / totalFreq - 1;
        Code new_low = low + (range * low_range) / totalFreq;
        
        // Safety check - verify range is still valid
        if (new_low >= new_high) {
            // Adjust to maintain a minimal valid range
            if (new_high < TOP_VALUE)
                new_high++;
            else
                new_low--;
                
            // If range is still invalid, we have to throw an error
            if (new_low >= new_high) {
                throw std::runtime_error("Range underflow in arithmetic coding");
            }
        }
        
        high = new_high;
        low = new_low;
        
        // E1/E2/E3 scaling for infinite precision estimation
        // Add counter to prevent infinite loops
        int scaleCount = 0;
        const int MAX_SCALING_ITERATIONS = 100; // Set a reasonable limit
        
        for (; scaleCount < MAX_SCALING_ITERATIONS; ++scaleCount) {
            // If high and low share the same MSB
            if ((high & HALF) == (low & HALF)) {
                // Output the MSB and any pending bits
                bool bit = (high & HALF) != 0;
                encodedBits.push_back(bit);
                
                // Output pending underflow bits (all opposite to the current bit)
                for (int i = 0; i < underflow_bits; ++i)
                    encodedBits.push_back(!bit);
                underflow_bits = 0;
                
                // Shift out the common MSB
                low <<= 1;
                high = (high << 1) | 1;
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
            
            // Safety check - ensure range remains valid after scaling
            if ((high - low) <= 1) {
                // Reset the range as emergency measure
                low = 0;
                high = TOP_VALUE;
                break;
            }
        }
        
        // Safety check for scaling iterations
        if (scaleCount >= MAX_SCALING_ITERATIONS) {
            throw std::runtime_error("Too many scaling iterations in arithmetic coding");
        }
    }
    
    // Process EOF symbol if present
    auto eof_it = probModel.find(EOF_SYMBOL);
    if (eof_it != probModel.end()) {
        const auto& [low_range, high_range] = eof_it->second;
        
        // Update range for EOF symbol
        uint64_t range = static_cast<uint64_t>(high - low) + 1;
        
        // Safeguard against range underflow
        if (range <= 1) {
            // Reset the range as emergency measure
            low = 0;
            high = TOP_VALUE;
            range = static_cast<uint64_t>(high - low) + 1;
        }
        
        Code new_high = low + (range * high_range) / (totalFreq + 1) - 1;
        Code new_low = low + (range * low_range) / (totalFreq + 1);
        
        // Ensure range is valid
        if (new_low >= new_high) {
            if (new_high < TOP_VALUE)
                new_high++;
            else
                new_low--;
        }
        
        high = new_high;
        low = new_low;
    }
    
    // Finalize encoding - we need at least one bit to identify the final range
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
    
    // Add one more bit to ensure the decoder can determine the range
    encodedBits.push_back((low < HALF) ? 1 : 0);
    
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
    
    // Safety check for numSymbols
    if (numSymbols == 0) {
        return {};
    }
    
    // Safety check for probModel
    if (probModel.empty()) {
        throw std::runtime_error("Empty probability model");
    }
    
    // Safety check for totalFreq
    if (totalFreq == 0) {
        throw std::runtime_error("Total frequency cannot be zero");
    }
    
    // Limit the number of symbols to decode to prevent excessive memory usage
    const size_t MAX_SYMBOLS = 10000000; // 10 million
    numSymbols = std::min(numSymbols, MAX_SYMBOLS);
    
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
    
    // Safety check for bits
    if (bits.empty()) {
        throw std::runtime_error("No bits to decode");
    }
    
    // Read initial bits to fill the code value
    size_t bitIndex = 0;
    for (int i = 0; i < CODE_BITS && bitIndex < bits.size(); ++i) {
        value = (value << 1) | (bits[bitIndex++] ? 1 : 0);
    }
    
    std::vector<uint32_t> decodedSymbols;
    decodedSymbols.reserve(numSymbols);
    
    // Safety check to prevent infinite loops
    const size_t MAX_ITERATIONS = numSymbols * 2; // Reasonable upper limit
    size_t iterationCount = 0;
    
    // Decode symbols
    while (decodedSymbols.size() < numSymbols && iterationCount < MAX_ITERATIONS) {
        iterationCount++;
        
        // Find the symbol for the current value
        uint64_t range = static_cast<uint64_t>(high - low) + 1;
        
        // Check for range underflow
        if (range <= 1) {
            // If we've decoded most of the expected symbols, we can safely exit
            if (decodedSymbols.size() >= numSymbols * 0.9) {
                break;
            }
            
            // Add some error recovery - try to reset the range
            low = 0;
            high = TOP_VALUE;
            
            // If that doesn't work, we have no choice but to throw an error
            if (static_cast<uint64_t>(high - low) + 1 <= 1) {
                throw std::runtime_error("Zero range in arithmetic decoding");
            }
            
            // Recalculate the range after recovery
            range = static_cast<uint64_t>(high - low) + 1;
        }
        
        // Calculate the scaled value using 128-bit arithmetic to avoid overflow
        // We normalize by totalFreq to find where in the probability model we are
        uint64_t scaledNum = static_cast<uint64_t>(value - low);
        uint64_t scaledDenom = range;
        
        // Use a safe calculation to avoid overflow or division by zero
        uint64_t scaledValue;
        if (scaledDenom > 0) {
            // Use multiplication instead of division to avoid truncation errors
            scaledValue = (scaledNum * totalFreq) / scaledDenom;
            
            // Ensure scaledValue is within bounds
            if (scaledValue >= totalFreq) {
                scaledValue = totalFreq - 1;
            }
        } else {
            // Emergency fallback for range underflow
            if (decodedSymbols.size() >= numSymbols * 0.9) {
                break;
            }
            throw std::runtime_error("Range underflow in arithmetic decoding");
        }
        
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
        
        // If we couldn't find a symbol, check if it's an EOF or handle error
        if (!symbolFound) {
            // Check for EOF
            auto eofIt = probModel.find(EOF_SYMBOL);
            if (eofIt != probModel.end()) {
                uint64_t eofScaledValue = scaledValue;
                if (eofIt->second.first <= eofScaledValue && 
                    eofScaledValue < eofIt->second.second) {
                    // Found EOF, we're done
                    break;
                }
            }
            
            // If we've decoded at least 90% of the expected symbols, we can stop
            if (decodedSymbols.size() >= numSymbols * 0.9) {
                break;
            }
            
            // Try to recover by picking the closest symbol
            uint64_t minDist = UINT64_MAX;
            for (const auto& [sym, range_pair] : probModel) {
                if (sym == EOF_SYMBOL) continue;
                
                const auto& [low_range, high_range] = range_pair;
                uint64_t dist1 = (scaledValue >= low_range) ? (scaledValue - low_range) : (low_range - scaledValue);
                uint64_t dist2 = (scaledValue >= high_range) ? (scaledValue - high_range) : (high_range - scaledValue);
                uint64_t dist = std::min(dist1, dist2);
                
                if (dist < minDist) {
                    minDist = dist;
                    symbol = sym;
                    symbolFound = true;
                }
            }
            
            if (!symbolFound) {
                // Last resort: if we have decoded symbols, use the last one
                if (!decodedSymbols.empty()) {
                    symbol = decodedSymbols.back();
                    symbolFound = true;
                } else {
                    // If we get here, we have an error we can't recover from
                    throw std::runtime_error("Invalid state in arithmetic decoding: no matching symbol");
                }
            }
        }
        
        // Add symbol to decoded output
        decodedSymbols.push_back(symbol);
        
        // Update the range based on the symbol
        const auto& [low_range, high_range] = probModel.at(symbol);
        uint64_t old_range = static_cast<uint64_t>(high - low) + 1;
        
        // Safeguard against range underflow
        if (old_range <= 1) {
            // Reset the range
            low = 0;
            high = TOP_VALUE;
            old_range = static_cast<uint64_t>(high - low) + 1;
        }
        
        // Calculate new high and low with extra precision to avoid underflow
        Code new_high = low + (old_range * high_range) / totalFreq - 1;
        Code new_low = low + (old_range * low_range) / totalFreq;
        
        // Ensure the range remains valid
        if (new_low >= new_high) {
            // Adjust to maintain a minimal valid range
            if (new_high < TOP_VALUE)
                new_high++;
            else
                new_low--;
        }
        
        high = new_high;
        low = new_low;
        
        // E1/E2/E3 scaling to match encoder's scaling
        // Add counter to prevent infinite loops
        int scaleCount = 0;
        const int MAX_SCALING_ITERATIONS = 100; // Set a reasonable limit
        
        for (; scaleCount < MAX_SCALING_ITERATIONS; ++scaleCount) {
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
            
            // Safety check - ensure range remains valid after scaling
            if ((high - low) <= 1) {
                // Reset the range as emergency measure
                low = 0;
                high = TOP_VALUE;
                break;
            }
        }
        
        // Safety check for scaling iterations
        if (scaleCount >= MAX_SCALING_ITERATIONS) {
            // If we've decoded at least 90% of the expected symbols, we can stop
            if (decodedSymbols.size() >= numSymbols * 0.9) {
                break;
            }
            throw std::runtime_error("Too many scaling iterations in arithmetic decoding");
        }
    }
    
    // Safety check for iteration count
    if (iterationCount >= MAX_ITERATIONS) {
        throw std::runtime_error("Too many iterations in arithmetic decoding - possible infinite loop");
    }
    
    // If we've decoded fewer symbols than expected, we need to pad the output
    while (decodedSymbols.size() < numSymbols) {
        // Pad with the last symbol (or 0 if there is none)
        uint32_t padSymbol = decodedSymbols.empty() ? 0 : decodedSymbols.back();
        decodedSymbols.push_back(padSymbol);
    }
    
    return decodedSymbols;
}

} // namespace compression 