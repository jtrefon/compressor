#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>

namespace compression {

/**
 * @brief Class for bit-level reading and writing operations
 *
 * This follows the Adapter pattern to provide bit-level operations
 * over byte-level storage.
 */
class BitIO {
public:
    /**
     * @brief Class for writing individual bits to a byte buffer
     */
    class BitWriter {
    public:
        /**
         * @brief Default constructor
         */
        BitWriter() : buffer_{}, bitPosition_(0) {}
        
        /**
         * @brief Write a single bit to the buffer
         * 
         * @param bit true for 1, false for 0
         */
        void writeBit(bool bit) {
            size_t bytePos = bitPosition_ / 8;
            uint8_t bitPos = bitPosition_ % 8;
            
            // Expand buffer if needed
            if (bytePos >= buffer_.size()) {
                buffer_.push_back(0);
            }
            
            // Set the bit
            if (bit) {
                buffer_[bytePos] |= (1 << (7 - bitPos));
            }
            
            bitPosition_++;
        }
        
        /**
         * @brief Write multiple bits to the buffer
         * 
         * @param bits Vector of bits to write
         */
        void writeBits(const std::vector<bool>& bits) {
            for (bool bit : bits) {
                writeBit(bit);
            }
        }
        
        /**
         * @brief Write a number using a specific number of bits
         * 
         * @param value The number to write
         * @param numBits Number of bits to use
         */
        void writeNumber(uint32_t value, uint8_t numBits) {
            if (numBits > 32) {
                throw std::invalid_argument("Cannot write more than 32 bits");
            }
            
            for (int i = numBits - 1; i >= 0; i--) {
                writeBit((value >> i) & 1);
            }
        }
        
        /**
         * @brief Get the current buffer
         * 
         * @return std::vector<uint8_t> The buffer containing written bits
         */
        std::vector<uint8_t> getBuffer() const {
            return buffer_;
        }
        
    private:
        std::vector<uint8_t> buffer_;
        size_t bitPosition_;
    };
    
    /**
     * @brief Class for reading individual bits from a byte buffer
     */
    class BitReader {
    public:
        /**
         * @brief Constructor with a buffer
         * 
         * @param buffer The buffer to read bits from
         */
        explicit BitReader(const std::vector<uint8_t>& buffer) 
            : buffer_(buffer), bytePos_(0), bitPos_(0) {}
            
        /**
         * @brief Read a single bit
         * 
         * @return bool true for 1, false for 0
         */
        bool readBit() {
            if (isEnd()) {
                throw std::out_of_range("End of bit stream reached");
            }
            
            bool bit = (buffer_[bytePos_] & (1 << (7 - bitPos_))) != 0;
            
            // Move to next bit
            bitPos_++;
            if (bitPos_ == 8) {
                bitPos_ = 0;
                bytePos_++;
            }
            
            return bit;
        }
        
        /**
         * @brief Read multiple bits and interpret as an unsigned integer
         * 
         * @param count Number of bits to read
         * @return uint32_t The value read
         */
        uint32_t readBits(uint8_t count) {
            uint32_t result = 0;
            
            for (uint8_t i = 0; i < count; i++) {
                result = (result << 1) | (readBit() ? 1 : 0);
            }
            
            return result;
        }
        
        /**
         * @brief Check if we've reached the end of the stream
         * 
         * @return bool true if at end, false otherwise
         */
        bool isEnd() const {
            return bytePos_ >= buffer_.size();
        }
        
    private:
        std::vector<uint8_t> buffer_;
        size_t bytePos_;
        uint8_t bitPos_;
    };
};

} // namespace compression 