#include <compression/HuffmanCompressor.hpp>
#include <stdexcept>
#include <algorithm> // for std::transform
#include <vector>
#include <map>
#include <queue>
#include <iterator> // for std::back_inserter
#include <iostream> // Added for std::cerr, std::endl

namespace compression {

namespace {
// --- Bit Manipulation Utilities --- 

// Writes bits to a byte vector
class BitWriter {
    std::vector<std::byte>& buffer;
    uint8_t current_byte = 0;
    int bit_position = 0; // 0-7, next bit to write

public:
    BitWriter(std::vector<std::byte>& buf) : buffer(buf) {}

    void writeBit(bool bit) {
        if (bit) {
            current_byte |= (1 << (7 - bit_position));
        }
        bit_position++;
        if (bit_position == 8) {
            buffer.push_back(static_cast<std::byte>(current_byte));
            current_byte = 0;
            bit_position = 0;
        }
    }

    void writeCode(const HuffmanCompressor::HuffmanCode& code) {
        for (bool bit : code) {
            writeBit(bit);
        }
    }

    // Call at the end to write any remaining bits in the current byte
    void flush() {
        if (bit_position > 0) {
            buffer.push_back(static_cast<std::byte>(current_byte));
        }
        // Optional: Could also write number of padding bits if needed
    }
};

// Reads bits from a byte vector
class BitReader {
    const std::vector<std::byte>& buffer;
    size_t byte_index = 0;
    int bit_position = 0; // 0-7, next bit to read
    size_t total_bits_read = 0;
    size_t max_bits; // Needed to handle padding/end

public:
    BitReader(const std::vector<std::byte>& buf, size_t start_offset, size_t num_bits)
        : buffer(buf), byte_index(start_offset), max_bits(num_bits) {}

    // Reads a single bit
    bool readBit(bool& success) {
        if (total_bits_read >= max_bits || byte_index >= buffer.size()) {
            success = false;
            return false; 
        }

        bool bit = (static_cast<uint8_t>(buffer[byte_index]) >> (7 - bit_position)) & 1;
        bit_position++;
        total_bits_read++;

        if (bit_position == 8) {
            bit_position = 0;
            byte_index++;
        }
        success = true;
        return bit;
    }
    
    // Returns current read position (offset in bytes from start)
    size_t getCurrentOffset() const {
        return byte_index;
    }

    // Returns number of bits read so far
    size_t getBitsRead() const {
        return total_bits_read;
    }
};

// Simple serialization: Size (uint16_t) | (Byte (1), Freq (8))* N
constexpr size_t FREQ_MAP_ENTRY_SIZE = sizeof(uint8_t) + sizeof(uint64_t);
constexpr size_t FREQ_MAP_SIZE_FIELD_SIZE = sizeof(uint16_t);

// Simple serialization of 64-bit value (little-endian)
void serializeUint64(uint64_t value, std::vector<std::byte>& buffer) {
    for (int i = 0; i < 8; ++i) {
        buffer.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xFF));
    }
}

// Simple deserialization of 64-bit value (little-endian)
uint64_t deserializeUint64(const std::vector<std::byte>& buffer, size_t& offset) {
    if (offset + sizeof(uint64_t) > buffer.size()) {
        throw std::runtime_error("Buffer too small to deserialize uint64_t");
    }
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<uint64_t>(buffer[offset++]) << (i * 8));
    }
    return value;
}

} // anonymous namespace

// --- HuffmanCompressor Implementation --- 

HuffmanCompressor::FrequencyMap 
HuffmanCompressor::buildFrequencyMap(const std::vector<std::byte>& data) const {
    FrequencyMap freqMap;
    for (std::byte b : data) {
        freqMap[b]++;
    }
    // Ensure at least two distinct symbols if only one exists, 
    // otherwise tree building fails. Add a dummy symbol with freq 0 if needed.
    // A more robust solution might handle single-symbol files specially.
    if (freqMap.size() == 1) {
         freqMap[static_cast<std::byte>(0)] = 0; // Add dummy if only one symbol
    }
    return freqMap;
}

// buildHuffmanTree uses vector and sort instead of priority_queue
std::unique_ptr<HuffmanCompressor::HuffmanNode> 
HuffmanCompressor::buildHuffmanTree(const FrequencyMap& freqMap) const {
    if (freqMap.empty()) return nullptr;

    // Use a vector to store nodes, manage ownership directly
    std::vector<std::unique_ptr<HuffmanNode>> nodes;
    nodes.reserve(freqMap.size());

    // Create leaf nodes 
    for (const auto& pair : freqMap) {
         // Only add nodes with non-zero frequency OR if it's the only node
         if (pair.second > 0 || freqMap.size() == 1) { 
             nodes.emplace_back(std::make_unique<HuffmanNode>(pair.first, pair.second));
         }
    }

    // Handle edge cases
    if (nodes.empty()) return nullptr; 
    
    if (nodes.size() == 1) {
        // If only one node, create a dummy parent to ensure a binary structure
        auto left = std::move(nodes[0]);
        nodes.clear(); // Clear the vector as we are replacing its content
        auto dummy_right = std::make_unique<HuffmanNode>(static_cast<std::byte>(0), 0);
        nodes.emplace_back(std::make_unique<HuffmanNode>(std::move(left), std::move(dummy_right)));
        // Fall through to return nodes[0] below
    }

    // Comparator for sorting unique_ptr<HuffmanNode> by frequency (descending for easy pop_back)
    auto compareNodes = [](const std::unique_ptr<HuffmanNode>& a, const std::unique_ptr<HuffmanNode>& b) {
        return a->frequency > b->frequency; // Sort descending frequency
    };

    // Build the tree using vector and sort
    while (nodes.size() > 1) {
        // Sort by frequency (highest frequency at the end for std::pop_back)
        std::sort(nodes.begin(), nodes.end(), compareNodes);

        // Take the two lowest frequency nodes (now at the end after sorting descending)
        auto right = std::move(nodes.back());
        nodes.pop_back();
        auto left = std::move(nodes.back());
        nodes.pop_back();

        // Create parent node, moving ownership
        auto parent = std::make_unique<HuffmanNode>(std::move(left), std::move(right));
        
        // Add parent back to the vector
        nodes.emplace_back(std::move(parent));
        // No need to re-sort immediately if we always process the lowest pair
    }

    // The final node in the vector is the root
    // Move ownership out of the vector before returning
    if (nodes.empty()) { // Should not happen if logic is correct
         throw std::logic_error("Node vector became empty during tree build.");
    }
    return std::move(nodes[0]);
}

void HuffmanCompressor::generateCodes(
    const HuffmanNode* node, HuffmanCode prefix, HuffmanCodeMap& codeMap) const 
{
    if (!node) return;

    // Leaf node: store the code
    if (!node->left && !node->right) {
        if (prefix.empty()) {
             if(node->frequency > 0) { 
                  prefix.push_back(false); 
             } else {
                 return;
             }
        }
        if (node->frequency > 0 || codeMap.empty()) { 
            codeMap[node->data] = prefix;
        }
        return;
    }

    // Traverse left (append 0)
    if (node->left) {
        prefix.push_back(false);
        generateCodes(node->left.get(), prefix, codeMap);
        prefix.pop_back();
    }

    // Traverse right (append 1)
    if (node->right) {
        prefix.push_back(true);
        generateCodes(node->right.get(), prefix, codeMap);
        prefix.pop_back();
    }
}

std::vector<std::byte> 
HuffmanCompressor::serializeFrequencyMap(const FrequencyMap& freqMap) const {
    std::vector<std::byte> buffer;
    buffer.reserve(FREQ_MAP_SIZE_FIELD_SIZE + freqMap.size() * FREQ_MAP_ENTRY_SIZE);
    uint16_t mapSize = static_cast<uint16_t>(freqMap.size());
    buffer.push_back(static_cast<std::byte>(mapSize & 0xFF));
    buffer.push_back(static_cast<std::byte>((mapSize >> 8) & 0xFF));
    for (const auto& pair : freqMap) {
        buffer.push_back(pair.first);
        serializeUint64(pair.second, buffer);
    }
    return buffer;
}

HuffmanCompressor::FrequencyMap 
HuffmanCompressor::deserializeFrequencyMap(const std::vector<std::byte>& buffer, size_t& offset) const {
    if (offset + FREQ_MAP_SIZE_FIELD_SIZE > buffer.size()) {
        throw std::runtime_error("Buffer too small for frequency map size field.");
    }
    uint16_t mapSize = static_cast<uint16_t>(buffer[offset++]);
    mapSize |= (static_cast<uint16_t>(buffer[offset++]) << 8);
    FrequencyMap freqMap;
    if (offset + mapSize * FREQ_MAP_ENTRY_SIZE > buffer.size()) {
         throw std::runtime_error("Buffer too small for frequency map entries.");
    }
    for (uint16_t i = 0; i < mapSize; ++i) {
        std::byte b = buffer[offset++];
        uint64_t freq = deserializeUint64(buffer, offset);
        freqMap[b] = freq;
    }
    return freqMap;
}


// compress needs to be non-const now because buildHuffmanTree is non-const
std::vector<std::byte> HuffmanCompressor::compress(const std::vector<std::byte>& data) const {
    if (data.empty()) return {};

    // 1. Build frequency map
    FrequencyMap freqMap = buildFrequencyMap(data);
    // Add dummy node if needed for tree construction (only if exactly one symbol exists)
    bool added_dummy = false;
    FrequencyMap treeBuildFreqMap = freqMap; // Copy for tree building
    if (treeBuildFreqMap.size() == 1 && treeBuildFreqMap.begin()->second > 0) { // Check if single node is real
         treeBuildFreqMap[static_cast<std::byte>(0)] = 0; 
         added_dummy = true;
    } else if (treeBuildFreqMap.empty() && !data.empty()){ // Should not happen
         throw std::runtime_error("Frequency map empty for non-empty data.");
    }
    if (treeBuildFreqMap.empty()) return {}; // Input data was truly empty

    // 2. Serialize frequency map (original map)
    std::vector<std::byte> freqMapBytes = serializeFrequencyMap(freqMap);

    // 3. Build Huffman tree (using map possibly containing dummy)
    std::unique_ptr<HuffmanNode> root = buildHuffmanTree(treeBuildFreqMap);
    if (!root) { 
         if (freqMap.empty()) return {}; // Truly empty input
         else throw std::runtime_error("Failed to build Huffman tree for non-empty data.");
    }

    // 4. Generate Huffman codes
    HuffmanCodeMap codeMap;
    generateCodes(root.get(), {}, codeMap);
    
    // Handle single symbol case explicitly after generating codes
    if (codeMap.empty() && !freqMap.empty()) {
        if (freqMap.size() == 1) {
            codeMap[freqMap.begin()->first] = {false};
        } else if (!data.empty()) {
            throw std::runtime_error("Failed to generate codes for non-empty data (multi-symbol).");
        }
    }

    // 5. Encode data using codes
    std::vector<std::byte> encodedData;
    encodedData.reserve(freqMapBytes.size() + sizeof(uint64_t) + data.size() + 100);
    encodedData.insert(encodedData.end(), freqMapBytes.begin(), freqMapBytes.end());
    size_t bitCountOffset = encodedData.size();
    serializeUint64(0, encodedData); // Placeholder for bit count

    BitWriter writer(encodedData);
    uint64_t totalBitsWritten = 0;
    for (std::byte b : data) {
        auto it = codeMap.find(b);
        if (it == codeMap.end()) {
             // This might happen if single-symbol case wasn't handled correctly
             if (codeMap.size() == 1 && freqMap.size() == 1 && b == freqMap.begin()->first) {
                  it = codeMap.find(b); 
             } 
             if (it == codeMap.end()) { 
                  throw std::runtime_error("Could not find Huffman code for byte during encoding.");
             }
        }
        const auto& code = it->second;
        writer.writeCode(code);
        totalBitsWritten += code.size();
    }
    writer.flush();
    
    // Overwrite the placeholder bit count
    size_t currentOffset = bitCountOffset;
    std::vector<std::byte> bitCountBuffer;
    serializeUint64(totalBitsWritten, bitCountBuffer);
    for(size_t i=0; i < sizeof(uint64_t); ++i) {
        if(currentOffset + i < encodedData.size()) { 
           encodedData[currentOffset + i] = bitCountBuffer[i];
        } else {
            throw std::runtime_error("Buffer overrun when writing bit count.");
        }
    }

    return encodedData;
}

// decompress needs to be non-const now because buildHuffmanTree is non-const
std::vector<std::byte> HuffmanCompressor::decompress(const std::vector<std::byte>& data) const {
     if (data.empty()) return {};

    size_t offset = 0;

    // 1. Deserialize frequency map
    FrequencyMap freqMap = deserializeFrequencyMap(data, offset);
    
     // Add dummy node if needed for tree reconstruction (matches logic in compress)
     FrequencyMap treeBuildFreqMap = freqMap; // Copy for tree building
     bool added_dummy = false;
     if (treeBuildFreqMap.size() == 1 && treeBuildFreqMap.begin()->second > 0) {
          treeBuildFreqMap[static_cast<std::byte>(0)] = 0;
          added_dummy = true;
     }
     
    if (treeBuildFreqMap.empty()) { 
        return {};
    }
    
    // 2. Read total number of bits to decode
    uint64_t totalBitsToRead = deserializeUint64(data, offset);
    
    // Check if the original file was logically empty 
    if (totalBitsToRead == 0) { 
        bool onlyDummy = true;
        for(const auto& pair : freqMap) { // Check original map
            if (pair.second > 0) { 
                 onlyDummy = false;
                 break;
            }
        }
        if (onlyDummy) return {}; 
    }

    // 3. Rebuild Huffman tree
    std::unique_ptr<HuffmanNode> root = buildHuffmanTree(treeBuildFreqMap);
     if (!root) { 
         return {};
     }
     
    // 4. Decode data using the tree and bit reader
    std::vector<std::byte> decompressedData;
    decompressedData.reserve(data.size()); 

    BitReader reader(data, offset, totalBitsToRead);
    const HuffmanNode* currentNode = root.get();
    uint64_t bitsReadCount = 0;

    // Handle single-node tree case explicitly
    if (!root->left && !root->right) {
         // This case implies the original data had only one symbol type.
         // The tree root node holds that symbol.
         if (totalBitsToRead > 0) { 
              if (root->frequency == 0) {
                   // This was the dummy node - indicates original was empty or invalid state?
                   // If totalBitsToRead > 0, this is an error.
                   throw std::runtime_error("Decoding error: Attempting to decode bits with only a dummy node.");
              }
              // Replicate the single byte originalSize times.
              // We need originalSize from the main file header for this.
              // CANNOT reliably determine size just from bitsRead / code length here.
              // Fallback: Assume each bit corresponds to one symbol (incorrect for real Huffman)
              // Let's rely on the main loop structure which should handle this.
             // For a single node tree, any bit read (must be 0) leads back to the root.
             for (uint64_t i=0; i < totalBitsToRead; ++i) {
                 bool success = false;
                 bool bit = reader.readBit(success);
                 if (!success || bit) { 
                     throw std::runtime_error("Decoding error: Invalid bit for single-node tree (expected 0).");
                 }
                 decompressedData.push_back(root->data);
                 bitsReadCount++;
             }
         }
    } else {
        // Normal decoding loop for multi-node trees
        while (bitsReadCount < totalBitsToRead) {
             if (!currentNode) { 
                 throw std::runtime_error("Decoding error: Reached null node unexpectedly.");
             }
             
            if (!currentNode->left && !currentNode->right) {
                decompressedData.push_back(currentNode->data);
                currentNode = root.get(); 
                 if (bitsReadCount >= totalBitsToRead) break; 
            }
            
            bool success = false;
            bool bit = reader.readBit(success);
            if (!success) {
                 if (bitsReadCount == totalBitsToRead && currentNode && !currentNode->left && !currentNode->right) {
                     break; 
                 }
                 throw std::runtime_error("Decoding error: Failed to read expected bit. Bits read: " + std::to_string(bitsReadCount) + "/" + std::to_string(totalBitsToRead));
            }
            bitsReadCount++;

            if (bit) { 
                currentNode = currentNode->right.get();
            } else { 
                currentNode = currentNode->left.get();
            }
            
            if (bitsReadCount == totalBitsToRead && currentNode && !currentNode->left && !currentNode->right) {
                 decompressedData.push_back(currentNode->data);
                 break; 
            }
        }
    }

    // Final check on bits read
    if (reader.getBitsRead() != totalBitsToRead) {
         std::cerr << "Warning: Read " << reader.getBitsRead() << " bits, but expected " << totalBitsToRead << std::endl;
    }

    return decompressedData;
}

} // namespace compression