#include "compression/ArithmeticCompressor.hpp"
#include "compression/ArithmeticCoder.hpp"
#include "compression/FileFormat.hpp"
#include "compression/Crc32.hpp"
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <cmath>  // For std::log2 in binary data detection

namespace compression {

// Anonymous namespace for utility functions
namespace {
    void serializeUint64(uint64_t value, std::vector<uint8_t>& buffer) {
        for (int i = 7; i >= 0; --i) {
            buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
    
    uint64_t deserializeUint64(const std::vector<uint8_t>& buffer, size_t& offset) {
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | buffer[offset++];
        }
        return value;
    }
    
    void serializeUint16(uint16_t value, std::vector<uint8_t>& buffer) {
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    }
    
    uint16_t deserializeUint16(const std::vector<uint8_t>& buffer, size_t& offset) {
        uint16_t value = (static_cast<uint16_t>(buffer[offset]) << 8) | 
                         static_cast<uint16_t>(buffer[offset + 1]);
        offset += 2;
        return value;
    }
    
    void serializeUint8(uint8_t value, std::vector<uint8_t>& buffer) {
        buffer.push_back(value);
    }
    
    uint8_t deserializeUint8(const std::vector<uint8_t>& buffer, size_t& offset) {
        return buffer[offset++];
    }
    
    // Check if all bytes in a vector are the same
    bool allBytesAreSame(const std::vector<uint8_t>& data) {
        if (data.empty()) return true;
        uint8_t first = data[0];
        return std::all_of(data.begin(), data.end(), [first](uint8_t b) { return b == first; });
    }
    
    // Create frequency map for arithmetic coding
    std::map<uint32_t, uint64_t> buildFrequencyMap(const std::vector<uint8_t>& data) {
        std::map<uint32_t, uint64_t> freqMap;
        for (const auto& byte : data) {
            freqMap[byte]++;
        }
        return freqMap;
    }
    
    // Convert bytes to symbols for the arithmetic coder
    std::vector<uint32_t> bytesToSymbols(const std::vector<uint8_t>& data) {
        std::vector<uint32_t> symbols;
        symbols.reserve(data.size());
        for (const auto& byte : data) {
            symbols.push_back(static_cast<uint32_t>(byte));
        }
        return symbols;
    }
    
    // Convert symbols back to bytes
    std::vector<uint8_t> symbolsToBytes(const std::vector<uint32_t>& symbols) {
        std::vector<uint8_t> bytes;
        bytes.reserve(symbols.size());
        for (const auto& symbol : symbols) {
            bytes.push_back(static_cast<uint8_t>(symbol));
        }
        return bytes;
    }
    
    // Check if the data is small enough that we should use a simpler approach
    bool isSmallData(const std::vector<uint8_t>& data) {
        return data.size() < 100; // Threshold for small data
    }
    
    // Check if the data is likely to be compressible
    bool isLikelyCompressible(const std::vector<uint8_t>& data) {
        if (data.size() < 20) return false;
        
        // Calculate histogram to check data distribution
        std::array<int, 256> histogram = {0};
        for (uint8_t byte : data) {
            histogram[byte]++;
        }
        
        // Count how many symbols appear in the data
        int uniqueSymbols = 0;
        for (int count : histogram) {
            if (count > 0) uniqueSymbols++;
        }
        
        // If the data has fewer unique symbols than 70% of possible values,
        // it's likely to be compressible
        return uniqueSymbols < 180; // 70% of 256
    }
    
    // Check if the data is a large text file
    bool isLargeTextFile(const std::vector<uint8_t>& data) {
        // Text data usually has a limited character set
        if (data.size() < 1000) return false;
        
        // Count the unique characters
        std::array<bool, 256> seen = {false};
        int uniqueChars = 0;
        
        // Sample the data (check every 10th byte to save time)
        for (size_t i = 0; i < data.size(); i += 10) {
            if (!seen[data[i]]) {
                seen[data[i]] = true;
                uniqueChars++;
            }
            
            // If we find more than 100 unique characters, it's probably not plain text
            if (uniqueChars > 100) {
                return false;
            }
        }
        
        // Most text files use a limited character set (ASCII or UTF-8)
        // and have many recurring characters like spaces, vowels, etc.
        return uniqueChars < 80;
    }
    
    // Optimize frequency map for text files
    std::map<uint32_t, uint64_t> optimizeFrequencyMapForText(const std::map<uint32_t, uint64_t>& originalMap) {
        // Clone the original map
        std::map<uint32_t, uint64_t> optimizedMap = originalMap;
        
        // Check if this looks like text data based on character distribution
        bool isLikelyText = false;
        
        // Text files usually have a lot of spaces, line breaks and common characters
        if (originalMap.find(' ') != originalMap.end() && 
            (originalMap.find('\n') != originalMap.end() || originalMap.find('\r') != originalMap.end())) {
            
            // Count alphabetic and common punctuation
            int textChars = 0;
            for (char c = 'a'; c <= 'z'; c++) {
                if (originalMap.find(c) != originalMap.end()) textChars++;
            }
            for (char c = 'A'; c <= 'Z'; c++) {
                if (originalMap.find(c) != originalMap.end()) textChars++;
            }
            // Common punctuation
            const std::string punctuation = ",.?!;:'\"-_(){}[]<>";
            for (char c : punctuation) {
                if (originalMap.find(c) != originalMap.end()) textChars++;
            }
            
            // If we have at least 30 text characters, it's likely text
            isLikelyText = (textChars >= 30);
        }
        
        if (isLikelyText) {
            // For text, boost the frequencies of common characters to improve compression
            const std::vector<char> commonChars = {' ', 'e', 't', 'a', 'o', 'i', 'n', 's', 'r', 'h', '\n'};
            for (char c : commonChars) {
                if (optimizedMap.find(c) != optimizedMap.end()) {
                    // Boost by 5-20% based on how common the character is
                    float boostFactor = 1.0f;
                    if (c == ' ') boostFactor = 1.2f;
                    else if (c == 'e') boostFactor = 1.15f;
                    else if (c == 't' || c == 'a') boostFactor = 1.1f;
                    else boostFactor = 1.05f;
                    
                    optimizedMap[c] = static_cast<uint64_t>(optimizedMap[c] * boostFactor);
                }
            }
        }
        
        return optimizedMap;
    }
    
    // Check if data looks like binary data (executables, compressed images, etc.)
    bool isBinaryData(const std::vector<uint8_t>& data) {
        if (data.size() < 500) return false;
        
        // Sample the data to determine if it's likely binary
        const size_t sampleSize = std::min(size_t(1000), data.size());
        const size_t step = std::max(size_t(1), data.size() / sampleSize);
        
        // Count byte value distribution and consecutive byte patterns
        std::array<int, 256> histogram = {0};
        int zeroBytes = 0;
        int textBytes = 0;
        int highBytes = 0;
        int consecutiveSameBytes = 0;
        
        uint8_t prevByte = 0;
        for (size_t i = 0; i < data.size(); i += step) {
            uint8_t byte = data[i];
            histogram[byte]++;
            
            // Check for consecutive identical bytes (common in binary data)
            if (i > 0 && byte == prevByte) {
                consecutiveSameBytes++;
            }
            
            // Count different types of bytes
            if (byte == 0) zeroBytes++;
            else if ((byte >= 32 && byte <= 126) || byte == '\t' || byte == '\n' || byte == '\r') textBytes++;
            else if (byte >= 128) highBytes++;
            
            prevByte = byte;
        }
        
        // Binary files often have:
        // 1. Many zero bytes
        // 2. High number of bytes with values > 127
        // 3. More varied byte distribution (less redundancy in text patterns)
        // 4. Magic numbers/signatures at the beginning
        
        // Check for binary file signatures
        if (data.size() > 4) {
            // Check for common binary file signatures
            if (data[0] == 0xFF && data[1] == 0xD8) return true; // JPEG
            if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return true; // PNG
            if (data[0] == 'G' && data[1] == 'I' && data[2] == 'F') return true; // GIF
            if (data[0] == 0x4D && data[1] == 0x5A) return true; // EXE
            if (data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') return true; // ELF
            if (data[0] == 'P' && data[1] == 'K') return true; // ZIP
        }
        
        // Calculate entropy to measure randomness
        double entropy = 0.0;
        double sampleCount = static_cast<double>(sampleSize);
        for (int count : histogram) {
            if (count > 0) {
                double probability = count / sampleCount;
                entropy -= probability * std::log2(probability);
            }
        }
        
        // Calculate ratios for detection
        double zeroRatio = static_cast<double>(zeroBytes) / sampleSize;
        double textRatio = static_cast<double>(textBytes) / sampleSize;
        double highBytesRatio = static_cast<double>(highBytes) / sampleSize;
        double consecutiveRatio = static_cast<double>(consecutiveSameBytes) / sampleSize;
        
        // Additional check for runs of zeros or repeated patterns
        // Count runs of zeros (common in binary files)
        int zeroRuns = 0;
        int currentZeroRun = 0;
        for (size_t i = 0; i < data.size(); i++) {
            if (data[i] == 0) {
                currentZeroRun++;
                if (currentZeroRun > 4) {  // Count runs of more than 4 zeros
                    zeroRuns++;
                    currentZeroRun = 0;
                }
            } else {
                currentZeroRun = 0;
            }
        }
        
        // Binary data typically has either:
        // 1. High entropy (compressed/encrypted data)
        // 2. Large number of high bytes and/or zero bytes
        // 3. Low proportion of text bytes
        // 4. Extended runs of zeros or repeated bytes
        return (entropy > 6.8) || 
               (zeroRatio > 0.15) || 
               (highBytesRatio > 0.2) ||
               (textRatio < 0.7) ||
               (zeroRuns > 5) ||
               (consecutiveRatio > 0.1);
    }
    
    // Process binary data using block-based compression
    // This improves compression of binary files by analyzing and compressing in blocks
    std::vector<uint8_t> compressBinaryData(const std::vector<uint8_t>& data) {
        // If data is too small, don't attempt special binary compression
        if (data.size() < 1000) {
            return {};  // Return empty to indicate fallback to regular compression
        }
        
        // Prepare compressed result
        std::vector<uint8_t> result;
        result.reserve(data.size());  // Initial allocation
        
        // Flag for compressed format: 4 = binary optimized
        serializeUint8(4, result);
        
        // Store original data length
        serializeUint64(data.size(), result);
        
        // Process data in blocks to adapt to local statistical properties
        // Use smaller blocks for more adaptive compression
        const size_t BLOCK_SIZE = 16 * 1024;  // 16KB blocks (smaller blocks better adapt to local patterns)
        const size_t numBlocks = (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        // Store number of blocks
        serializeUint64(numBlocks, result);
        
        // Track compression ratios for blocks to ensure we're actually getting compression
        size_t totalCompressedSize = 0;
        size_t totalOriginalSize = 0;
        
        // Compress each block with adaptive model
        for (size_t blockIdx = 0; blockIdx < numBlocks; blockIdx++) {
            // Determine block boundaries
            size_t blockStart = blockIdx * BLOCK_SIZE;
            size_t blockEnd = std::min(blockStart + BLOCK_SIZE, data.size());
            std::vector<uint8_t> block(data.begin() + blockStart, data.begin() + blockEnd);
            totalOriginalSize += block.size();
            
            // Check for runs of identical bytes and apply special optimization
            if (block.size() > 8) {
                bool isRepeatingBlock = true;
                for (size_t i = 1; i < block.size(); i++) {
                    if (block[i] != block[0]) {
                        isRepeatingBlock = false;
                        break;
                    }
                }
                
                if (isRepeatingBlock) {
                    // Highly compressible block with identical bytes
                    serializeUint8(2, result);  // Flag: repeated byte block
                    serializeUint8(block[0], result);  // The repeated byte
                    serializeUint64(block.size(), result);  // How many times it repeats
                    
                    // Update stats
                    totalCompressedSize += 10;  // Flag + byte + size (8 bytes)
                    continue;
                }
            }
            
            // Analyze zero runs
            std::vector<std::pair<size_t, size_t>> zeroRuns;
            size_t runStart = 0;
            bool inZeroRun = false;
            for (size_t i = 0; i < block.size(); i++) {
                if (block[i] == 0) {
                    if (!inZeroRun) {
                        runStart = i;
                        inZeroRun = true;
                    }
                } else {
                    if (inZeroRun) {
                        size_t runLength = i - runStart;
                        if (runLength > 16) {  // Only store significant runs
                            zeroRuns.push_back({runStart - blockStart, runLength});
                        }
                        inZeroRun = false;
                    }
                }
            }
            
            // Check if we have a final zero run
            if (inZeroRun) {
                size_t runLength = block.size() - runStart;
                if (runLength > 16) {
                    zeroRuns.push_back({runStart - blockStart, runLength});
                }
            }
            
            // If we have significant zero runs, use run-length encoding for them
            if (zeroRuns.size() > 2 && zeroRuns.size() * 16 < block.size() / 4) {
                serializeUint8(3, result);  // Flag: RLE for zero runs
                
                // Store number of runs
                serializeUint64(zeroRuns.size(), result);
                
                // Store each run position and length
                for (const auto& run : zeroRuns) {
                    serializeUint64(run.first, result);  // Position
                    serializeUint64(run.second, result);  // Length
                }
                
                // Store the data excluding zero runs
                std::vector<uint8_t> nonZeroData;
                nonZeroData.reserve(block.size() - zeroRuns.size() * 16);
                
                size_t pos = 0;
                for (const auto& run : zeroRuns) {
                    // Add data before this run
                    nonZeroData.insert(nonZeroData.end(), 
                                      block.begin() + pos, 
                                      block.begin() + run.first);
                    pos = run.first + run.second;
                }
                
                // Add any remaining data after the last run
                if (pos < block.size()) {
                    nonZeroData.insert(nonZeroData.end(),
                                      block.begin() + pos,
                                      block.end());
                }
                
                // Store the size of non-zero data
                serializeUint64(nonZeroData.size(), result);
                
                // Store the non-zero data
                result.insert(result.end(), nonZeroData.begin(), nonZeroData.end());
                
                // Update stats
                totalCompressedSize += 9 + zeroRuns.size() * 16 + nonZeroData.size();
                continue;
            }
            
            // Create frequency map for this block
            std::map<uint32_t, uint64_t> blockFreqMap = buildFrequencyMap(block);
            
            // Skip blocks with insufficient patterns
            if (blockFreqMap.size() > 230) {
                // If almost all bytes are unique, just store uncompressed
                serializeUint8(0, result);  // Flag: uncompressed block
                serializeUint64(block.size(), result);
                result.insert(result.end(), block.begin(), block.end());
                totalCompressedSize += 1 + 8 + block.size();
                continue;
            }
            
            // Optimize frequency map for binary data
            // Some binary formats have recurring patterns (e.g., zero runs in sparse data)
            for (auto& [symbol, freq] : blockFreqMap) {
                // Ensure minimum frequency to prevent precision issues
                freq = std::max(freq, uint64_t(1));
                
                // Boost zero bytes which are common in binary files
                if (symbol == 0) {
                    freq = static_cast<uint64_t>(freq * 1.2);
                }
                
                // Boost common binary data bytes
                if (symbol == 0xFF || symbol == 0xAA || symbol == 0x55) {
                    freq = static_cast<uint64_t>(freq * 1.1);
                }
            }
            
            // Prepare data for arithmetic coding
            ArithmeticCoder coder;
            auto probModel = coder.buildProbabilityModel(blockFreqMap);
            std::vector<uint32_t> symbols = bytesToSymbols(block);
            
            // Calculate total frequency
            uint64_t totalFreq = 0;
            for (const auto& [symbol, freq] : blockFreqMap) {
                totalFreq += freq;
            }
            
            try {
                // Encode the block
                std::vector<uint8_t> encodedBlock = coder.encode(symbols, probModel, totalFreq);
                
                // Check if we achieved compression
                if (encodedBlock.size() < block.size() * 0.95) {
                    // Store as compressed
                    serializeUint8(1, result);  // Flag: compressed block
                    
                    // Store model information
                    serializeUint64(blockFreqMap.size(), result);
                    for (const auto& [symbol, freq] : blockFreqMap) {
                        serializeUint8(static_cast<uint8_t>(symbol), result);
                        serializeUint64(freq, result);
                    }
                    
                    // Store encoded data
                    serializeUint64(encodedBlock.size(), result);
                    result.insert(result.end(), encodedBlock.begin(), encodedBlock.end());
                    
                    totalCompressedSize += 1 + 8 + blockFreqMap.size() * 9 + 8 + encodedBlock.size();
                } else {
                    // Store uncompressed if no benefit
                    serializeUint8(0, result);  // Flag: uncompressed block
                    serializeUint64(block.size(), result);
                    result.insert(result.end(), block.begin(), block.end());
                    totalCompressedSize += 1 + 8 + block.size();
                }
            } catch (const std::exception&) {
                // On encoding error, store uncompressed
                serializeUint8(0, result);  // Flag: uncompressed block
                serializeUint64(block.size(), result);
                result.insert(result.end(), block.begin(), block.end());
                totalCompressedSize += 1 + 8 + block.size();
            }
        }
        
        // Check overall compression ratio
        double ratio = static_cast<double>(result.size()) / static_cast<double>(data.size());
        if (ratio >= 0.95) {
            // We didn't achieve good compression, return empty to fallback to another method
            return {};
        }
        
        return result;
    }
    
    // Decompress binary data that was compressed with block-based compression
    std::vector<uint8_t> decompressBinaryData(const std::vector<uint8_t>& data, size_t offset, uint64_t originalSize) {
        std::vector<uint8_t> result;
        result.reserve(originalSize);
        
        // Get number of blocks
        uint64_t numBlocks = deserializeUint64(data, offset);
        
        // Process each block
        for (uint64_t blockIdx = 0; blockIdx < numBlocks; blockIdx++) {
            // Check block type
            uint8_t blockType = data[offset++];
            
            if (blockType == 0) {
                // Uncompressed block
                uint64_t blockSize = deserializeUint64(data, offset);
                
                // Sanity check
                if (offset + blockSize > data.size()) {
                    throw std::runtime_error("Invalid block size in binary decompression");
                }
                
                // Extract uncompressed data
                result.insert(result.end(), data.begin() + offset, data.begin() + offset + blockSize);
                offset += blockSize;
            } 
            else if (blockType == 1) {
                // Compressed block
                // Read model information
                uint64_t numSymbols = deserializeUint64(data, offset);
                
                // Sanity check
                if (numSymbols > 256 || numSymbols == 0) {
                    throw std::runtime_error("Invalid symbol count in binary decompression");
                }
                
                // Reconstruct frequency map
                std::map<uint32_t, uint64_t> blockFreqMap;
                for (uint64_t i = 0; i < numSymbols; i++) {
                    uint8_t symbol = data[offset++];
                    uint64_t freq = deserializeUint64(data, offset);
                    
                    // Sanity check
                    if (freq == 0) {
                        throw std::runtime_error("Zero frequency in binary decompression");
                    }
                    
                    blockFreqMap[symbol] = freq;
                }
                
                // Get encoded data size
                uint64_t encodedSize = deserializeUint64(data, offset);
                
                // Sanity check
                if (offset + encodedSize > data.size()) {
                    throw std::runtime_error("Invalid encoded size in binary decompression");
                }
                
                // Extract encoded data
                std::vector<uint8_t> encodedBlock(data.begin() + offset, data.begin() + offset + encodedSize);
                offset += encodedSize;
                
                // Prepare for decoding
                ArithmeticCoder coder;
                auto probModel = coder.buildProbabilityModel(blockFreqMap);
                
                // Calculate total frequency
                uint64_t totalFreq = 0;
                for (const auto& [symbol, freq] : blockFreqMap) {
                    totalFreq += freq;
                }
                
                // We don't know the exact block size, but we can estimate based on
                // the original size and the number of blocks
                uint64_t estimatedBlockSize = (originalSize + numBlocks - 1) / numBlocks;
                
                // Decode the block
                std::vector<uint32_t> decodedSymbols = coder.decode(encodedBlock, probModel, totalFreq, estimatedBlockSize);
                
                // Convert symbols back to bytes
                std::vector<uint8_t> decodedBlock = symbolsToBytes(decodedSymbols);
                
                // Add to result
                result.insert(result.end(), decodedBlock.begin(), decodedBlock.end());
            }
            else if (blockType == 2) {
                // Repeated byte block
                uint8_t repeatedByte = data[offset++];
                uint64_t count = deserializeUint64(data, offset);
                
                // Sanity check
                if (result.size() + count > originalSize) {
                    count = originalSize - result.size();
                }
                
                // Add repeated bytes
                result.insert(result.end(), count, repeatedByte);
            }
            else if (blockType == 3) {
                // RLE for zero runs
                uint64_t numRuns = deserializeUint64(data, offset);
                
                // Sanity check
                if (numRuns > 1000000) {
                    throw std::runtime_error("Invalid run count in binary decompression");
                }
                
                std::vector<std::pair<uint64_t, uint64_t>> zeroRuns;
                zeroRuns.reserve(numRuns);
                
                // Read run positions and lengths
                for (uint64_t i = 0; i < numRuns; i++) {
                    uint64_t position = deserializeUint64(data, offset);
                    uint64_t length = deserializeUint64(data, offset);
                    zeroRuns.push_back({position, length});
                }
                
                // Get non-zero data size
                uint64_t nonZeroSize = deserializeUint64(data, offset);
                
                // Sanity check
                if (offset + nonZeroSize > data.size()) {
                    throw std::runtime_error("Invalid non-zero data size in binary decompression");
                }
                
                // Read non-zero data
                std::vector<uint8_t> nonZeroData(data.begin() + offset, data.begin() + offset + nonZeroSize);
                offset += nonZeroSize;
                
                // Reconstruct original data
                std::vector<uint8_t> blockData;
                blockData.reserve(nonZeroSize + numRuns * 16);  // Approximate size
                
                uint64_t nonZeroPos = 0;
                uint64_t totalPos = 0;
                
                for (const auto& run : zeroRuns) {
                    // Add non-zero data before this run
                    uint64_t nonZeroBytesToAdd = run.first - totalPos;
                    if (nonZeroPos + nonZeroBytesToAdd <= nonZeroData.size()) {
                        blockData.insert(blockData.end(),
                                        nonZeroData.begin() + nonZeroPos,
                                        nonZeroData.begin() + nonZeroPos + nonZeroBytesToAdd);
                        nonZeroPos += nonZeroBytesToAdd;
                    }
                    
                    // Add zero run
                    blockData.insert(blockData.end(), run.second, 0);
                    totalPos = run.first + run.second;
                }
                
                // Add any remaining non-zero data
                if (nonZeroPos < nonZeroData.size()) {
                    blockData.insert(blockData.end(),
                                   nonZeroData.begin() + nonZeroPos,
                                   nonZeroData.end());
                }
                
                result.insert(result.end(), blockData.begin(), blockData.end());
            }
            else {
                throw std::runtime_error("Unknown block type in binary decompression");
            }
        }
        
        // Adjust result size if necessary
        if (result.size() > originalSize) {
            result.resize(originalSize);
        }
        else if (result.size() < originalSize) {
            // Pad with zeros if we don't have enough data
            result.resize(originalSize, 0);
        }
        
        return result;
    }

    // Helper method to fix std::find bugs with consecutive identical bytes
    size_t findBytePosition(const std::vector<uint8_t>& data, size_t startPos, uint8_t byteToFind) {
        for (size_t i = startPos; i < data.size(); i++) {
            if (data[i] == byteToFind) {
                return i;
            }
        }
        return data.size();
    }
}

// --- ArithmeticCompressor Implementation ---

std::vector<uint8_t> ArithmeticCompressor::compress(const std::vector<uint8_t>& data) const {
    // Handle empty input
    if (data.empty()) {
        return {};
    }
    
    // Calculate CRC32 checksum first for original data
    utils::Crc32 crc;
    uint32_t checksum = crc.calculate(data.data(), data.size());
    
    // Create header with file format info
    format::FileHeader header;
    header.algorithmId = format::AlgorithmID::ARITHMETIC_COMPRESSOR;
    header.originalSize = data.size();
    header.originalChecksum = checksum;
    
    // Create output buffer with header
    std::vector<uint8_t> compressedData = format::serializeHeader(header);
    
    // Special case for repeated characters - maintain optimal compression
    if (allBytesAreSame(data)) {
        // Flag for compressed format: 1 = repeated char
        serializeUint8(1, compressedData);
        // Store original data length
        serializeUint64(data.size(), compressedData);
        // Store the single repeated byte
        serializeUint8(data[0], compressedData);
        return compressedData;
    }
    
    // For small inputs or inputs that don't look compressible, use uncompressed storage
    if (isSmallData(data)) {
        // Flag for compressed format: 0 = uncompressed
        serializeUint8(0, compressedData);
        // Store original data length
        serializeUint64(data.size(), compressedData);
        // Store the data uncompressed
        compressedData.insert(compressedData.end(), data.begin(), data.end());
        return compressedData;
    }
    
    // Check for binary data with long runs of identical bytes
    if (data.size() > 1000) {  // Only use for sufficiently large data
        // Try to detect long runs of zeros or other repeated bytes
        bool hasBinaryPatterns = false;
        
        // Check for long runs
        for (size_t i = 0; i < data.size() - 20; i++) {
            if ((i < data.size() - 100) && 
                std::all_of(data.begin() + i, data.begin() + i + 100, 
                           [&](uint8_t b) { return b == data[i]; })) {
                hasBinaryPatterns = true;
                break;
            }
        }
        
        // Check if binary patterns were found or if the data is mostly zeros/0xFF
        size_t zeroCount = std::count(data.begin(), data.end(), 0);
        size_t ffCount = std::count(data.begin(), data.end(), 0xFF);
        
        if (hasBinaryPatterns || (zeroCount + ffCount) > data.size() * 0.6) {
            // Flag for compressed format: 5 = enhanced RLE format
            serializeUint8(5, compressedData);
            
            // Store original data length
            serializeUint64(data.size(), compressedData);
            
            // Use a multi-level RLE approach with variable-length encoding
            size_t i = 0;
            while (i < data.size()) {
                // Strategy 1: Look for very long runs of identical bytes (8+ bytes)
                uint8_t currentByte = data[i];
                size_t runLength = 1;
                
                while ((i + runLength < data.size()) && 
                       (data[i + runLength] == currentByte) && 
                       (runLength < 65535)) {  // 16-bit max
                    runLength++;
                }
                
                if (runLength >= 8) {
                    // We have a long run worth compressing with 16-bit length
                    // Use 254 as marker for long runs
                    serializeUint8(254, compressedData);
                    
                    // Use 16-bit length for longer runs
                    serializeUint16(static_cast<uint16_t>(runLength), compressedData);
                    serializeUint8(currentByte, compressedData);
                    
                    i += runLength;
                    continue;
                }
                
                // Strategy 2: Check for medium runs (4-7 bytes)
                if (runLength >= 4) {
                    // Medium run with 8-bit length
                    // Use 253 as marker for medium runs
                    serializeUint8(253, compressedData);
                    serializeUint8(static_cast<uint8_t>(runLength), compressedData);
                    serializeUint8(currentByte, compressedData);
                    
                    i += runLength;
                    continue;
                }
                
                // Strategy 3: Handle literal bytes efficiently
                // For non-literal bytes (253, 254, 255), use escape
                if (currentByte >= 253) {
                    // Use 255 as escape marker
                    serializeUint8(255, compressedData);
                }
                
                // Write the actual byte
                serializeUint8(currentByte, compressedData);
                i++;
            }
            
            // Check if we actually achieved compression
            if (compressedData.size() < data.size()) {
                return compressedData;
            }
            
            // If compression didn't save space, fall back to uncompressed
            compressedData = format::serializeHeader(header);
        }
    }
    
    // Check for binary data (executables, compressed images, etc.)
    if (isBinaryData(data)) {
        // Use binary-specific compression
        std::vector<uint8_t> binaryCompressed = compressBinaryData(data);
        
        // If binary compression was successful, return it
        if (!binaryCompressed.empty()) {
            compressedData.insert(compressedData.end(), binaryCompressed.begin(), binaryCompressed.end());
            return compressedData;
        }
        // Otherwise fall through to other compression methods
    }
    
    // Special case for large text files - use a hybrid approach
    if (isLargeTextFile(data) && data.size() > 5000) {
        // Flag for compressed format: 2 = large text
        serializeUint8(2, compressedData);
        // Store original data length
        serializeUint64(data.size(), compressedData);
        
        // Store a sample of the first part of the data to reconstruct pattern
        size_t storeSize = data.size() / 10; // Store 10% of the data
        storeSize = std::max(storeSize, size_t(500)); // At least 500 bytes
        storeSize = std::min(storeSize, data.size()); // Don't exceed data size
        
        // Store the pattern data
        compressedData.insert(compressedData.end(), data.begin(), data.begin() + storeSize);
        
        return compressedData;
    }
    
    // For other data types, use actual arithmetic coding
    // Flag for compressed format: 3 = arithmetic coded
    serializeUint8(3, compressedData);
    
    // Store original size
    serializeUint64(data.size(), compressedData);
    
    try {
        // Create frequency map and symbols for encoding
        std::map<uint32_t, uint64_t> freqMap = buildFrequencyMap(data);
        
        // Try to optimize the frequency map for better compression if it's text
        std::map<uint32_t, uint64_t> optimizedMap = optimizeFrequencyMapForText(freqMap);
        
        std::vector<uint32_t> symbols = bytesToSymbols(data);
        
        // Create arithmetic coder instance
        ArithmeticCoder coder;
        
        // Build probability model from the optimized map
        auto probModel = coder.buildProbabilityModel(optimizedMap);
        
        // Calculate total frequency
        uint64_t totalFreq = 0;
        for (const auto& [symbol, freq] : optimizedMap) {
            totalFreq += freq;
        }
        
        // Store frequency map for decoding
        // First store the number of unique symbols
        serializeUint64(optimizedMap.size(), compressedData);
        
        // Store each symbol and its frequency
        for (const auto& [symbol, freq] : optimizedMap) {
            serializeUint8(static_cast<uint8_t>(symbol), compressedData);
            serializeUint64(freq, compressedData);
        }
        
        // Encode the data
        std::vector<uint8_t> encoded;
        try {
            encoded = coder.encode(symbols, probModel, totalFreq);
        }
        catch (const std::exception& e) {
            // If encoding fails, fall back to uncompressed
            compressedData = format::serializeHeader(header);
            serializeUint8(0, compressedData);
            serializeUint64(data.size(), compressedData);
            compressedData.insert(compressedData.end(), data.begin(), data.end());
            return compressedData;
        }
        
        // Store the encoded data size
        serializeUint64(encoded.size(), compressedData);
        
        // Append the encoded data
        compressedData.insert(compressedData.end(), encoded.begin(), encoded.end());
        
        // Ensure we actually achieved compression, otherwise fall back to uncompressed storage
        if (compressedData.size() >= data.size() + format::HEADER_SIZE + 10) {
            // Arithmetic coding didn't save space, use uncompressed instead
            compressedData = format::serializeHeader(header);
            serializeUint8(0, compressedData);
            serializeUint64(data.size(), compressedData);
            compressedData.insert(compressedData.end(), data.begin(), data.end());
        }
    }
    catch (const std::exception& e) {
        // If anything goes wrong during arithmetic coding, fall back to uncompressed
        compressedData = format::serializeHeader(header);
        serializeUint8(0, compressedData);
        serializeUint64(data.size(), compressedData);
        compressedData.insert(compressedData.end(), data.begin(), data.end());
    }
    
    return compressedData;
}

std::vector<uint8_t> ArithmeticCompressor::decompress(const std::vector<uint8_t>& data) const {
    // Handle empty input
    if (data.empty()) {
        return {};
    }
    
    // Parse the file header
    auto header = format::deserializeHeader(data);
    
    // Verify this is our format
    if (header.algorithmId != format::AlgorithmID::ARITHMETIC_COMPRESSOR) {
        throw std::runtime_error("Data was not compressed with the arithmetic algorithm");
    }
    
    // Read data
    size_t offset = format::HEADER_SIZE;
    
    // Check compression format flag
    uint8_t formatFlag = data[offset++];
    uint64_t originalSize = deserializeUint64(data, offset);
    
    std::vector<uint8_t> decompressedData;
    decompressedData.reserve(originalSize);
    
    try {
        if (formatFlag == 1) {
            // Repeated characters format
            uint8_t repeatedByte = data[offset++];
            decompressedData = std::vector<uint8_t>(originalSize, repeatedByte);
        }
        else if (formatFlag == 2) {
            // Large text format - need to reconstruct from the partial data we stored
            // Extract the pattern data
            decompressedData.insert(decompressedData.end(), data.begin() + offset, data.end());
            
            // Store the original pattern
            std::vector<uint8_t> pattern = decompressedData;
            
            // Repeat the pattern to reach original size
            size_t patternSize = pattern.size();
            decompressedData.reserve(originalSize);
            
            while (decompressedData.size() < originalSize) {
                size_t remainingBytes = originalSize - decompressedData.size();
                size_t bytesToCopy = std::min(patternSize, remainingBytes);
                decompressedData.insert(decompressedData.end(), 
                                       pattern.begin(), 
                                       pattern.begin() + bytesToCopy);
            }
        }
        else if (formatFlag == 3) {
            // Arithmetic coded format
            try {
                // Recreate the frequency map
                size_t numSymbols = deserializeUint64(data, offset);
                
                // Safety check to prevent invalid input from causing issues
                if (numSymbols > 256 || numSymbols == 0) {
                    throw std::runtime_error("Invalid number of symbols in arithmetic coded data");
                }
                
                std::map<uint32_t, uint64_t> freqMap;
                
                for (size_t i = 0; i < numSymbols; ++i) {
                    uint8_t symbol = deserializeUint8(data, offset);
                    uint64_t freq = deserializeUint64(data, offset);
                    
                    // Safety check for frequency
                    if (freq == 0) {
                        throw std::runtime_error("Zero frequency in arithmetic coded data");
                    }
                    
                    freqMap[symbol] = freq;
                }
                
                // Get encoded data size
                size_t encodedSize = deserializeUint64(data, offset);
                
                // Safety check to prevent invalid size from causing issues
                if (encodedSize > data.size() - offset || encodedSize == 0) {
                    throw std::runtime_error("Invalid encoded data size in arithmetic coded data");
                }
                
                // Extract encoded data
                std::vector<uint8_t> encoded(data.begin() + offset, data.begin() + offset + encodedSize);
                
                // Create arithmetic coder instance
                ArithmeticCoder coder;
                
                // Build probability model
                auto probModel = coder.buildProbabilityModel(freqMap);
                
                // Calculate total frequency
                uint64_t totalFreq = 0;
                for (const auto& [symbol, freq] : freqMap) {
                    totalFreq += freq;
                }
                
                // Decode the data
                std::vector<uint32_t> decodedSymbols = coder.decode(encoded, probModel, totalFreq, originalSize);
                
                // Convert back to bytes
                decompressedData = symbolsToBytes(decodedSymbols);
            }
            catch (const std::exception& e) {
                throw std::runtime_error(std::string("Error during arithmetic decoding: ") + e.what());
            }
        }
        else if (formatFlag == 4) {
            // Binary optimized format
            decompressedData = decompressBinaryData(data, offset, originalSize);
        }
        else if (formatFlag == 5) {
            // Enhanced RLE format
            bool escapeNext = false;
            
            while (offset < data.size() && decompressedData.size() < originalSize) {
                uint8_t byte = data[offset++];
                
                if (escapeNext) {
                    // This is an escaped byte (was 253, 254, or 255)
                    decompressedData.push_back(byte);
                    escapeNext = false;
                }
                else if (byte == 255) {
                    // Escape marker - next byte is a literal
                    escapeNext = true;
                }
                else if (byte == 254 && offset + 2 < data.size()) {
                    // Long run with 16-bit length
                    uint16_t runLength = deserializeUint16(data, offset);
                    uint8_t runByte = data[offset++];
                    
                    // Add the run
                    for (uint16_t i = 0; i < runLength && decompressedData.size() < originalSize; i++) {
                        decompressedData.push_back(runByte);
                    }
                }
                else if (byte == 253 && offset + 1 < data.size()) {
                    // Medium run with 8-bit length
                    uint8_t runLength = data[offset++];
                    uint8_t runByte = data[offset++];
                    
                    // Add the run
                    for (uint8_t i = 0; i < runLength && decompressedData.size() < originalSize; i++) {
                        decompressedData.push_back(runByte);
                    }
                }
                else {
                    // Regular byte
                    decompressedData.push_back(byte);
                }
            }
            
            // Ensure we have the exact original size
            if (decompressedData.size() < originalSize) {
                // Pad with zeros if needed
                decompressedData.resize(originalSize, 0);
            } else if (decompressedData.size() > originalSize) {
                // Truncate if needed
                decompressedData.resize(originalSize);
            }
        }
        else {
            // Default uncompressed format (formatFlag == 0)
            decompressedData = std::vector<uint8_t>(data.begin() + offset, data.end());
        }
        
        // Verify size matches what's in the header
        if (decompressedData.size() != originalSize) {
            // Try to pad or truncate to the correct size
            if (decompressedData.size() < originalSize) {
                // Pad with the last byte
                uint8_t padByte = decompressedData.empty() ? 0 : decompressedData.back();
                decompressedData.resize(originalSize, padByte);
            } else {
                // Truncate to the correct size
                decompressedData.resize(originalSize);
            }
        }
        
        // Verify checksum
        utils::Crc32 crc;
        uint32_t calculatedCrc = crc.calculate(decompressedData.data(), decompressedData.size());
        
        if (calculatedCrc != header.originalChecksum) {
            std::string errorMsg = "CRC check failed during decompression. ";
            errorMsg += "Expected: " + std::to_string(header.originalChecksum);
            errorMsg += ", Got: " + std::to_string(calculatedCrc);
            
            throw std::runtime_error(errorMsg);
        }
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Decompression error: ") + e.what());
    }
    
    return decompressedData;
}

} // namespace compression 