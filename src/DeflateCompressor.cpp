#include "compression/DeflateCompressor.hpp"
#include "compression/Lz77Compressor.hpp"
#include <stdexcept>

namespace compression {

// Implementation of buildDecodingTree function
std::unique_ptr<HuffmanDecoderNode> buildDecodingTree(const HuffmanCodeMap& codeMap) {
    auto root = std::make_unique<HuffmanDecoderNode>();
    
    // For each symbol and its code
    for (const auto& [symbol, code] : codeMap) {
        HuffmanDecoderNode* node = root.get();
        
        // Traverse/build the tree according to the code
        for (size_t i = 0; i < code.size(); ++i) {
            bool bit = code[i];
            
            // Last bit determines where to insert the leaf node
            if (i == code.size() - 1) {
                auto leaf = std::make_unique<HuffmanDecoderNode>();
                leaf->symbol = symbol;
                leaf->isLeaf = true;
                
                if (bit) { // Right child
                    node->right = std::move(leaf);
                } else {   // Left child
                    node->left = std::move(leaf);
                }
            } else {
                // Internal node - create if it doesn't exist
                if (bit) { // Right path
                    if (!node->right) {
                        node->right = std::make_unique<HuffmanDecoderNode>();
                    }
                    node = node->right.get();
                } else {   // Left path
                    if (!node->left) {
                        node->left = std::make_unique<HuffmanDecoderNode>();
                    }
                    node = node->left.get();
                }
            }
        }
    }
    
    return root;
}

DeflateCompressor::DeflateCompressor() 
    : lz77_(std::make_unique<Lz77Compressor>(32768, 258, false, true)) // Use adaptive minimum length
{
    // Initialized with improved LZ77 compressor settings
}

DeflateCompressor::~DeflateCompressor() = default;

std::vector<uint8_t> DeflateCompressor::compress(const std::vector<uint8_t>& data) const {
    if (!lz77_) {
        throw std::runtime_error("LZ77 compressor not initialized");
    }
    
    // Use our optimized LZ77 compressor
    return lz77_->compress(data);
}

std::vector<uint8_t> DeflateCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (!lz77_) {
        throw std::runtime_error("LZ77 compressor not initialized");
    }
    
    try {
        // Use our optimized LZ77 decompressor
        return lz77_->decompress(data);
    } catch (const std::exception& e) {
        // Add some context to the error
        throw std::runtime_error(std::string("Deflate decompression error: ") + e.what());
    }
}

// Implementation of helper methods
void DeflateCompressor::buildFrequencyMaps(
    const std::vector<Lz77Compressor::Lz77Symbol>& symbols,
    FrequencyMap& litLenFreqMap,
    FrequencyMap& distFreqMap) const {
    
    // Count literal/length frequencies
    for (const auto& symbol : symbols) {
        if (symbol.isLiteral()) {
            // It's a literal
            uint32_t litValue = static_cast<uint32_t>(symbol.symbol);
            litLenFreqMap[litValue]++;
        } else {
            // It's a length code
            uint32_t lengthCode = symbol.symbol;
            litLenFreqMap[lengthCode]++;
            
            // Also count the distance
            distFreqMap[symbol.distance]++;
        }
    }
    
    // Make sure EOB symbol is included in literal/length map
    if (litLenFreqMap.find(Lz77Compressor::EOB_SYMBOL) == litLenFreqMap.end()) {
        litLenFreqMap[Lz77Compressor::EOB_SYMBOL] = 1;
    }
}

void DeflateCompressor::encodeSymbols(
    BitIO::BitWriter& bitWriter,
    const std::vector<Lz77Compressor::Lz77Symbol>& symbols,
    const HuffmanCodeMap& litLenCodeMap,
    const HuffmanCodeMap& distCodeMap) const {
    
    // Encode each symbol
    for (const auto& symbol : symbols) {
        // Find code for this symbol
        auto litLenCodeIt = litLenCodeMap.find(symbol.symbol);
        if (litLenCodeIt == litLenCodeMap.end()) {
            throw std::runtime_error("Symbol not found in Huffman code map");
        }
        
        // Write code for literal/length
        bitWriter.writeBits(litLenCodeIt->second);
        
        // If it's a length code, also write the distance
        if (!symbol.isLiteral()) {
            auto distCodeIt = distCodeMap.find(symbol.distance);
            if (distCodeIt == distCodeMap.end()) {
                throw std::runtime_error("Distance not found in Huffman code map");
            }
            
            // Write code for distance
            bitWriter.writeBits(distCodeIt->second);
        }
    }
    
    // Write end-of-block symbol
    auto eobCodeIt = litLenCodeMap.find(Lz77Compressor::EOB_SYMBOL);
    if (eobCodeIt != litLenCodeMap.end()) {
        bitWriter.writeBits(eobCodeIt->second);
    }
}

void DeflateCompressor::writeDynamicTables(
    BitIO::BitWriter& writer, 
    const HuffmanCodeMap& litLenCodes, 
    const HuffmanCodeMap& distCodes) const {
    
    // This is a stub function for now
    // Would need to implement according to Deflate spec
    
    // For example:
    // Write number of literal/length codes - 257
    writer.writeNumber(litLenCodes.size() - 257, 5);
    
    // Write number of distance codes - 1
    writer.writeNumber(distCodes.size() - 1, 5);
    
    // Write number of code length codes - 4 (could vary)
    writer.writeNumber(4 - 4, 4);
    
    // Followed by code lengths for the code length alphabet...
    // Then the Huffman-encoded code lengths for the literal/length and distance alphabets
}

std::pair<HuffmanCodeMap, HuffmanCodeMap> DeflateCompressor::readDynamicTables(
    BitIO::BitReader& reader) const {
    
    // This is a stub function for now
    // Would need to implement according to Deflate spec
    
    // For example:
    uint32_t hlit = reader.readBits(5) + 257;
    uint32_t hdist = reader.readBits(5) + 1;
    uint32_t hclen = reader.readBits(4) + 4;
    
    // Read code lengths for the code length alphabet...
    // Then decode the Huffman-encoded code lengths for literal/length and distance alphabets
    
    // Return empty maps for now
    return {HuffmanCodeMap(), HuffmanCodeMap()};
}

void DeflateCompressor::decodeSymbols(
    BitIO::BitReader& reader,
    const HuffmanDecoderNode& litLenTreeRoot,
    const HuffmanDecoderNode& distTreeRoot, 
    std::vector<uint8_t>& output) const {
    
    // This is a stub function for now
    // Would need to implement according to Deflate spec
    
    // Continue reading symbols until we hit EOB
    while (!reader.isEnd()) {
        uint32_t symbol = decodeSymbol(reader, &litLenTreeRoot);
        
        if (symbol == Lz77Compressor::EOB_SYMBOL) {
            break; // End of block
        }
        
        if (symbol < Lz77Compressor::LENGTH_CODE_BASE) {
            // It's a literal - add to output
            output.push_back(static_cast<uint8_t>(symbol));
        } else {
            // It's a length code
            uint32_t length = Lz77Compressor::getLengthFromCode(symbol);
            
            // Decode distance
            uint32_t distCode = decodeSymbol(reader, &distTreeRoot);
            
            // Copy bytes from output buffer at (current position - distance) for length bytes
            size_t pos = output.size() - distCode;
            for (size_t i = 0; i < length; ++i) {
                output.push_back(output[pos + i]);
            }
        }
    }
}

uint32_t DeflateCompressor::decodeSymbol(
    BitIO::BitReader& reader, 
    const HuffmanDecoderNode* root) const {
    
    // Navigate the tree based on bits read
    const HuffmanDecoderNode* node = root;
    
    while (!node->isLeaf) {
        bool bit = reader.readBit();
        
        if (bit) {
            // Go right
            node = node->right.get();
        } else {
            // Go left
            node = node->left.get();
        }
        
        if (!node) {
            throw std::runtime_error("Invalid Huffman code encountered");
        }
    }
    
    return node->symbol;
}

std::vector<RleSymbol> DeflateCompressor::runLengthEncodeCodeLengths(
    const std::vector<uint8_t>& lengths) const {
    
    std::vector<RleSymbol> result;
    
    // Simple RLE implementation
    for (size_t i = 0; i < lengths.size();) {
        uint8_t currentLength = lengths[i];
        
        if (currentLength == 0) {
            // Count consecutive zeros
            size_t zeroCount = 0;
            while (i < lengths.size() && lengths[i] == 0) {
                zeroCount++;
                i++;
            }
            
            // Encode zeros
            while (zeroCount > 0) {
                if (zeroCount < 3) {
                    // Direct zeros
                    for (size_t j = 0; j < zeroCount; j++) {
                        result.push_back({0, 0, 0});
                    }
                    zeroCount = 0;
                } else if (zeroCount <= 10) {
                    // Use code 17 (3-10 zeros)
                    result.push_back({17, static_cast<uint8_t>(zeroCount - 3), 3});
                    zeroCount = 0;
                } else {
                    // Use code 18 (11-138 zeros)
                    uint8_t count = std::min<uint8_t>(138, zeroCount);
                    result.push_back({18, static_cast<uint8_t>(count - 11), 7});
                    zeroCount -= count;
                }
            }
        } else {
            // Output the current length
            result.push_back({currentLength, 0, 0});
            i++;
            
            // Count repeated values
            size_t repeatCount = 0;
            while (i < lengths.size() && lengths[i] == currentLength) {
                repeatCount++;
                i++;
            }
            
            // Encode repeats
            while (repeatCount > 0) {
                if (repeatCount < 3) {
                    // Direct repeats
                    for (size_t j = 0; j < repeatCount; j++) {
                        result.push_back({currentLength, 0, 0});
                    }
                    repeatCount = 0;
                } else {
                    // Use code 16 (3-6 repeats)
                    uint8_t count = std::min<uint8_t>(6, repeatCount);
                    result.push_back({16, static_cast<uint8_t>(count - 3), 2});
                    repeatCount -= count;
                }
            }
        }
    }
    
    return result;
}

} // namespace compression