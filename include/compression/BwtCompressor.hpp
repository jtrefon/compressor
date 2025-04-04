#ifndef COMPRESSION_BWTCOMPRESSOR_HPP
#define COMPRESSION_BWTCOMPRESSOR_HPP

#include "ICompressor.hpp"
#include <vector>
#include <cstdint>
#include <memory>

namespace compression {

/**
 * @brief MTF (Move-To-Front) encoder used with BWT for better compression
 *
 * The MTF transform maps each character to its rank in a list of recently used characters.
 * This transformation increases the frequency of small values in the output,
 * making it more compressible with entropy coding.
 */
class MoveToFrontEncoder {
public:
    /**
     * @brief Encode data using Move-To-Front transform
     * 
     * @param data Input data to encode
     * @return Encoded data
     */
    std::vector<uint8_t> encode(const std::vector<uint8_t>& data) const;

    /**
     * @brief Decode data that was encoded with Move-To-Front transform
     * 
     * @param data Input data to decode
     * @return Decoded data
     */
    std::vector<uint8_t> decode(const std::vector<uint8_t>& data) const;
};

/**
 * @brief Implements the Burrows-Wheeler Transform (BWT) compression algorithm.
 * 
 * BWT is a block sorting algorithm that rearranges characters to group similar
 * characters together, making the data more compressible with entropy coding.
 * This implementation combines BWT with Move-To-Front transform and entropy coding
 * to achieve high compression ratios for text data.
 */
class BwtCompressor : public ICompressor {
public:
    /**
     * @brief Construct a BWT compressor with default settings
     */
    BwtCompressor();
    
    /**
     * @brief Destructor with default implementation
     */
    ~BwtCompressor() override = default;
    
    /**
     * @brief Compresses data using the BWT algorithm
     * 
     * The compression pipeline is:
     * 1. Burrows-Wheeler Transform
     * 2. Move-To-Front Transform
     * 3. Run-Length Encoding (optional)
     * 4. Entropy coding (typically Huffman or Arithmetic)
     * 
     * @param data The data to compress
     * @return The compressed data
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompresses data that was compressed with the BWT algorithm
     * 
     * The decompression pipeline is the reverse of compression:
     * 1. Entropy decoding
     * 2. Run-Length decoding (if used)
     * 3. Move-To-Front decoding
     * 4. Inverse Burrows-Wheeler Transform
     * 
     * @param data The compressed data
     * @return The decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    /**
     * @brief Apply Burrows-Wheeler Transform to input data
     * 
     * @param block Data block to transform
     * @return Pair of transformed block and primary index
     */
    std::pair<std::vector<uint8_t>, uint32_t> bwtEncode(const std::vector<uint8_t>& block) const;
    
    /**
     * @brief Apply inverse Burrows-Wheeler Transform to restore original data
     * 
     * @param block Transformed data block
     * @param primaryIndex Primary index from forward transform
     * @return Original data block
     */
    std::vector<uint8_t> bwtDecode(const std::vector<uint8_t>& block, uint32_t primaryIndex) const;
    
    /**
     * @brief Apply run-length encoding to data
     * 
     * @param data Input data to compress
     * @return RLE-compressed data
     */
    std::vector<uint8_t> runLengthEncode(const std::vector<uint8_t>& data) const;
    
    /**
     * @brief Decode run-length encoded data
     * 
     * @param data RLE-compressed data
     * @return Original data
     */
    std::vector<uint8_t> runLengthDecode(const std::vector<uint8_t>& data) const;
    
    // Block size for BWT (larger blocks give better compression but use more memory)
    size_t blockSize_;
    
    // MTF encoder/decoder
    MoveToFrontEncoder mtfCoder_;
    
    // Secondary compressor for entropy coding (typically Huffman)
    std::unique_ptr<ICompressor> entropyCompressor_;
};

} // namespace compression

#endif // COMPRESSION_BWTCOMPRESSOR_HPP
