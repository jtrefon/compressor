#include "compression/HuffmanCompressor.hpp"
#include <bitset>
#include <algorithm>
#include <stdexcept>
#include <sstream>
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

// Helper function to convert a byte to a bitset string representation for debugging
static std::string byteToBitString(uint8_t byte) {
    std::bitset<8> bits(byte);
    return bits.to_string();
}

// Helper function to get the number of bits needed to represent a value
static uint8_t bitsRequired(uint64_t value) {
    if (value == 0) return 1; // Special case: need 1 bit to represent 0
    
    uint8_t bits = 0;
    while (value > 0) {
        value >>= 1;
        bits++;
    }
    return bits;
}

// Helper for debugging: get node description (address + data)
static std::string getNodeDesc(const HuffmanCompressor::HuffmanNode* node) {
    if (!node) return "NULL";
    std::stringstream ss;
    ss << "Node@" << node << "(data=" << static_cast<int>(node->data) 
       << ", freq=" << node->frequency << ")";
    return ss.str();
}

// --- Frequency Map Construction ---
HuffmanCompressor::FrequencyMap HuffmanCompressor::buildFrequencyMap(
    const std::vector<uint8_t>& data) const {
    FrequencyMap freqMap;
    
    // Count occurrences of each byte
    for (const auto& byte : data) {
        freqMap[byte]++;
    }
    
    return freqMap;
}

// --- Huffman Tree Construction ---
// Custom comparator for the priority queue
struct NodePtrGreater {
    bool operator()(
        const std::unique_ptr<HuffmanCompressor::HuffmanNode>& lhs, 
        const std::unique_ptr<HuffmanCompressor::HuffmanNode>& rhs) const {
        return lhs->frequency > rhs->frequency; // Min heap based on frequency
    }
};

std::unique_ptr<HuffmanCompressor::HuffmanNode> HuffmanCompressor::buildHuffmanTree(
    const FrequencyMap& freqMap) const {
    
    // Special case: empty input
    if (freqMap.empty()) {
        return nullptr;
    }
    
    // Special case: only one byte value in the input
    if (freqMap.size() == 1) {
        auto it = freqMap.begin();
        auto node = std::make_unique<HuffmanNode>(it->first, it->second);
        // For single-symbol files, we still need a proper tree structure
        // Create a parent node with our symbol node as a child
        auto root = std::make_unique<HuffmanNode>(nullptr, nullptr);
        root->frequency = node->frequency;
        root->left = std::move(node);
        return root;
    }
    
    // Priority queue containing the nodes of the Huffman tree
    // Using min heap based on frequency
    using MinHeap = std::priority_queue<
        std::unique_ptr<HuffmanNode>, 
        std::vector<std::unique_ptr<HuffmanNode>>, 
        NodePtrGreater
    >;
    
    MinHeap pq;
    
    // Create a leaf node for each symbol and add it to the priority queue
    for (const auto& [byte, freq] : freqMap) {
        pq.push(std::make_unique<HuffmanNode>(byte, freq));
    }
    
    // While there is more than one node in the queue
    while (pq.size() > 1) {
        // Extract the two nodes with lowest frequency
        auto left = std::move(const_cast<std::unique_ptr<HuffmanNode>&>(pq.top()));
        pq.pop();
        
        auto right = std::move(const_cast<std::unique_ptr<HuffmanNode>&>(pq.top()));
        pq.pop();
        
        // Create a new internal node with these two nodes as children
        // and with frequency equal to the sum of their frequencies
        auto parent = std::make_unique<HuffmanNode>(std::move(left), std::move(right));
        
        // Add the new node to the priority queue
        pq.push(std::move(parent));
    }
    
    // The remaining node is the root node of the Huffman tree
    return std::move(const_cast<std::unique_ptr<HuffmanNode>&>(pq.top()));
}

// --- Code Generation ---
void HuffmanCompressor::generateCodes(
    const HuffmanNode* node, 
    HuffmanCode prefix, 
    HuffmanCodeMap& codeMap) const {
    
    if (!node) return;
    
    // Check if this is a leaf node
    if (!node->left && !node->right) {
        // Assign code to symbol
        codeMap[node->data] = prefix;
        return;
    }
    
    // Recursive traversal to build codes
    if (node->left) {
        // Add 0 for left branch
        HuffmanCode leftCode = prefix;
        leftCode.push_back(false);
        generateCodes(node->left.get(), leftCode, codeMap);
    }
    
    if (node->right) {
        // Add 1 for right branch
        HuffmanCode rightCode = prefix;
        rightCode.push_back(true);
        generateCodes(node->right.get(), rightCode, codeMap);
    }
}

// --- Serialization of Frequency Map ---
std::vector<uint8_t> HuffmanCompressor::serializeFrequencyMap(
    const FrequencyMap& freqMap) const {
    
    std::vector<uint8_t> serialized;
    
    // Format: 
    // - Byte: Number of entries (max 256)
    // - For each entry:
    //   - Byte: Symbol
    //   - VarInt: Frequency
    
    // Number of entries (max 256 for byte values)
    serialized.push_back(static_cast<uint8_t>(freqMap.size()));
    
    // For each symbol and its frequency
    for (const auto& [symbol, frequency] : freqMap) {
        // Symbol (byte value)
        serialized.push_back(symbol);
        
        // Frequency (variable-length encoding)
        // Store 7 bits per byte, high bit = "more bytes follow"
        uint64_t value = frequency;
        do {
            uint8_t byte = value & 0x7F; // Get 7 bits
            value >>= 7;  // Shift for next 7 bits
            if (value > 0) byte |= 0x80; // Set high bit if more data follows
            serialized.push_back(byte);
        } while (value > 0);
    }
    
    return serialized;
}

// --- Deserialization of Frequency Map ---
HuffmanCompressor::FrequencyMap HuffmanCompressor::deserializeFrequencyMap(
    const std::vector<uint8_t>& buffer, 
    size_t& offset) const {
    
    FrequencyMap freqMap;
    
    // Check buffer size
    if (offset >= buffer.size()) {
        throw std::runtime_error("Buffer ended unexpectedly during map deserialization");
    }
    
    // Get count of entries
    uint8_t count = buffer[offset++];
    
    // Process each entry
    for (uint8_t i = 0; i < count; i++) {
        // Check buffer size
        if (offset + 1 >= buffer.size()) {
            throw std::runtime_error("Buffer ended unexpectedly during map entry deserialization");
        }
        
        // Read symbol
        uint8_t symbol = buffer[offset++];
        
        // Read frequency (variable-length encoded)
        uint64_t frequency = 0;
        uint8_t shift = 0;
        
        while (offset < buffer.size()) {
            uint8_t byte = buffer[offset++];
            frequency |= (static_cast<uint64_t>(byte & 0x7F) << shift);
            shift += 7;
            
            // Check if this is the last byte of the value
            if ((byte & 0x80) == 0) break;
        }
        
        // Store in the map
        freqMap[symbol] = frequency;
    }
    
    return freqMap;
}

// --- Main Compression Function ---
std::vector<uint8_t> HuffmanCompressor::compress(
    const std::vector<uint8_t>& data) const {
    
    // Handle empty input
    if (data.empty()) {
        return {};
    }
    
    // 1. Build frequency map
    FrequencyMap freqMap = buildFrequencyMap(data);
    
    // 2. Build Huffman tree
    auto treeRoot = buildHuffmanTree(freqMap);
    
    // 3. Generate codes for each symbol
    HuffmanCodeMap codeMap;
    generateCodes(treeRoot.get(), {}, codeMap);
    
    // 4. Serialize the frequency map
    std::vector<uint8_t> result = serializeFrequencyMap(freqMap);
    
    // 5. Write the compressed data
    std::vector<bool> encodedBits;
    
    // Pre-allocate enough space for encoded bits (rough estimate)
    encodedBits.reserve(data.size() * 8); // Worst case: no compression
    
    // Encode each symbol
    for (const auto& byte : data) {
        const auto& code = codeMap[byte];
        encodedBits.insert(encodedBits.end(), code.begin(), code.end());
    }
    
    // Convert bits to bytes (8 bits per byte)
    size_t fullByteCount = encodedBits.size() / 8;
    uint8_t remainingBits = encodedBits.size() % 8;
    
    // Add number of bits in the last byte
    result.push_back(remainingBits);
    
    // Process full bytes
    for (size_t i = 0; i < fullByteCount; i++) {
        uint8_t byte = 0;
        for (size_t bit = 0; bit < 8; bit++) {
            if (encodedBits[i * 8 + bit]) {
                byte |= (1 << (7 - bit));
            }
        }
        result.push_back(byte);
    }
    
    // Process remaining bits (if any)
    if (remainingBits > 0) {
        uint8_t byte = 0;
        for (size_t bit = 0; bit < remainingBits; bit++) {
            if (encodedBits[fullByteCount * 8 + bit]) {
                byte |= (1 << (7 - bit));
            }
        }
        result.push_back(byte);
    }
    
    return result;
}

// --- Main Decompression Function ---
std::vector<uint8_t> HuffmanCompressor::decompress(
    const std::vector<uint8_t>& data) const {
    
    // Handle empty input
    if (data.empty()) {
        return {};
    }
    
    // 1. Read the frequency map
    size_t offset = 0;
    FrequencyMap freqMap = deserializeFrequencyMap(data, offset);
    
    // 2. Rebuild the Huffman tree
    auto treeRoot = buildHuffmanTree(freqMap);
    
    // 3. Validate the data
    if (offset >= data.size()) {
        throw std::runtime_error("Unexpected end of compressed data");
    }
    
    // 4. Get bit count in last byte
    uint8_t lastByteBits = data[offset++];
    if (lastByteBits > 7) {
        throw std::runtime_error("Invalid bit count in last byte (must be 0-7)");
    }
    
    // 5. Calculate total number of bits in encoded data
    size_t totalBits = (data.size() - offset - 1) * 8;
    if (data.size() > offset) {
        totalBits += lastByteBits;
    }
    
    // 6. Decode the data
    std::vector<uint8_t> result;
    
    // No encoded bits? Return empty result
    if (totalBits == 0) {
        return result;
    }
    
    // Start at root node
    const HuffmanNode* currentNode = treeRoot.get();
    
    // Process each bit
    size_t bitsProcessed = 0;
    while (bitsProcessed < totalBits) {
        // Calculate byte index and bit position
        size_t byteIndex = offset + bitsProcessed / 8;
        uint8_t bitPos = 7 - (bitsProcessed % 8); // Most significant bit first
        
        // Get the bit
        bool bit = (data[byteIndex] & (1 << bitPos)) != 0;
        
        // Follow the tree
        currentNode = bit ? currentNode->right.get() : currentNode->left.get();
        
        // Check for null node (corrupted data)
        if (!currentNode) {
            throw std::runtime_error("Invalid Huffman code encountered");
        }
        
        // If leaf node, output symbol and reset to root
        if (!currentNode->left && !currentNode->right) {
            result.push_back(currentNode->data);
            currentNode = treeRoot.get();
        }
        
        bitsProcessed++;
    }
    
    // If we're not at the root, the data is incomplete
    if (currentNode != treeRoot.get() && currentNode->left && currentNode->right) {
        throw std::runtime_error("Incomplete Huffman code at end of data");
    }
    
    return result;
}

} // namespace compression