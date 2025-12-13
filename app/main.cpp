#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstdint> // For uint8_t (replacing std::byte)
#include <map>
#include <functional>
#include <iterator> // For std::back_inserter
#include <iomanip> // For std::hex

#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/RleCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/FileFormat.hpp> // Include the new header format definitions
#include <compression/Crc32.hpp> // Include CRC32 utility
#include <compression/Lz77Compressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/UltraCompressor.hpp>
#include <compression/ExtremeCompressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <compression/ParallelCompressor.hpp>
#include <compression/SystemInfo.hpp>

// --- Helper Functions --- 

// Reads entire file into a vector of bytes
std::vector<uint8_t> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Error reading file: " + filename);
    }
    return buffer;
}

// Writes a vector of bytes to a file
void writeFile(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!file) {
        throw std::runtime_error("Error writing file: " + filename);
    }
}

// --- Compressor Factory --- 

// Factory function now creates compressor based on AlgorithmID
std::unique_ptr<compression::ICompressor> createCompressor(compression::format::AlgorithmID id) {
    switch (id) {
        case compression::format::AlgorithmID::RLE_COMPRESSOR:
            return std::make_unique<compression::RleCompressor>();
        case compression::format::AlgorithmID::NULL_COMPRESSOR:
            return std::make_unique<compression::NullCompressor>();
        case compression::format::AlgorithmID::HUFFMAN_COMPRESSOR:
            return std::make_unique<compression::HuffmanCompressor>();
        case compression::format::AlgorithmID::LZ77_COMPRESSOR:
            return std::make_unique<compression::Lz77Compressor>(32768, 3, 258, false, true, true);
        case compression::format::AlgorithmID::BWT_COMPRESSOR:
            return std::make_unique<compression::BwtCompressor>();
        case compression::format::AlgorithmID::ULTRA_COMPRESSOR:
            return std::make_unique<compression::UltraCompressor>();
        case compression::format::AlgorithmID::EXTREME_COMPRESSOR:
            return std::make_unique<compression::ExtremeCompressor>();
        case compression::format::AlgorithmID::OPTIMIZED_COMPRESSOR:
            return std::make_unique<compression::OptimizedCompressor>();
        default:
            throw std::invalid_argument("Unknown or unsupported compression algorithm ID: "
                                        + std::to_string(static_cast<uint8_t>(id)));
    }
}

compression::format::AlgorithmID getAlgorithmIdForCompression(const std::string& strategyName) {
    if (strategyName == "default") {
        return compression::format::AlgorithmID::OPTIMIZED_COMPRESSOR;
    }

    compression::format::AlgorithmID id = compression::format::stringToAlgorithmId(strategyName);
    if (id == compression::format::AlgorithmID::NULL_COMPRESSOR ||
        id == compression::format::AlgorithmID::RLE_COMPRESSOR) {
        throw std::invalid_argument("Strategy is disabled: " + strategyName);
    }
    if (id == compression::format::AlgorithmID::UNKNOWN) {
        throw std::invalid_argument("Unknown compression strategy: " + strategyName);
    }
    return id;
}

// Overload for creating based on name (used for compression command)
std::unique_ptr<compression::ICompressor> createCompressorForCompression(
    const std::string& strategyName,
    compression::format::AlgorithmID& outAlgoId) {
    outAlgoId = getAlgorithmIdForCompression(strategyName);
    return createCompressor(outAlgoId);
}

// --- Main Application Logic --- 

void printUsage(const char* appName) {
    std::cerr << "Usage: " << appName
              << " <compress|decompress> <strategy|ignored_on_decompress> <input_file> <output_file> [--threads N|--no-threads]\n"
              << "Strategies: default(optimized), bwt, ultra, extreme, lz77, huffman, optimized\n";
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        printUsage(argv[0]);
        return 1;
    }

    std::string operation = argv[1];
    std::string strategyName = argv[2]; // Used only for compression
    std::string inputFile = argv[3];
    std::string outputFile = argv[4];

    // Default to single-threaded operation for best compression ratio.
    std::size_t threadCount = 1;
    if (argc >= 6) {
        std::string arg = argv[5];
        if (arg == "--no-threads") {
            threadCount = 1;
        } else if (arg == "--threads" && argc >= 7) {
            threadCount = static_cast<std::size_t>(std::stoul(argv[6]));
        } else if (arg.rfind("--threads=", 0) == 0) {
            threadCount = static_cast<std::size_t>(std::stoul(arg.substr(10)));
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (operation != "compress" && operation != "decompress") {
        std::cerr << "Error: Invalid operation. Must be 'compress' or 'decompress'.\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        if (operation == "compress") {
            // 1. Create the compressor strategy from name
            compression::format::AlgorithmID algoId = compression::format::AlgorithmID::UNKNOWN;
            auto compressor = createCompressorForCompression(strategyName, algoId);

            // 2. Read input file
            std::cout << "Reading input file: " << inputFile << "..." << std::endl;
            std::vector<uint8_t> originalData = readFile(inputFile);
            std::cout << "Original size: " << originalData.size() << " bytes." << std::endl;

            // 3. Calculate CRC32 of original data
            uint32_t originalCRC = compression::utils::crc32Calculator.calculate(originalData);
            std::cout << "Original CRC32: 0x" << std::hex << originalCRC << std::dec << std::endl;

            // 4. Compress data
            std::vector<uint8_t> outputData;
            if (threadCount <= 1) {
                std::cout << "Compressing using " << strategyName << " strategy..." << std::endl;
                std::vector<uint8_t> compressedData = compressor->compress(originalData);
                std::cout << "Compressed payload size: " << compressedData.size() << " bytes." << std::endl;

                // 5. Create and serialize header (including checksum)
                compression::format::FileHeader header;
                header.algorithmId = algoId;
                header.originalSize = originalData.size();
                header.originalChecksum = originalCRC; // Store calculated CRC
                header.chunkCount = 1;
                header.chunkSize = static_cast<uint32_t>(originalData.size());
                header.compressedSizes = { static_cast<uint32_t>(compressedData.size()) };
                std::vector<uint8_t> headerBytes = compression::format::serializeHeader(header);
                std::cout << "Header size: " << headerBytes.size() << " bytes." << std::endl;

                // 6. Concatenate header and compressed data
                outputData.reserve(headerBytes.size() + compressedData.size());
                outputData.insert(outputData.end(), headerBytes.begin(), headerBytes.end());
                outputData.insert(outputData.end(), compressedData.begin(), compressedData.end());
            } else {
                std::cout << "Compressing in parallel with " << threadCount << " threads..." << std::endl;
                compression::ParallelCompressor pc(std::move(compressor), algoId, threadCount);
                outputData = pc.compress(originalData);
            }

            std::cout << "Total output size: " << outputData.size() << " bytes." << std::endl;

            // 7. Write output file
            std::cout << "Writing output file: " << outputFile << "..." << std::endl;
            writeFile(outputFile, outputData);

        } else { // operation == "decompress"
            // 1. Read input file (contains header + payload)
            std::cout << "Reading input file: " << inputFile << "..." << std::endl;
            std::vector<uint8_t> inputData = readFile(inputFile);
            std::cout << "Input size: " << inputData.size() << " bytes." << std::endl;

            // 2. Deserialize header
            std::cout << "Deserializing header..." << std::endl;
            compression::format::FileHeader header = compression::format::deserializeHeader(inputData);
            std::string algoName = compression::format::algorithmIdToString(header.algorithmId);
            std::cout << "  Format Version: " << static_cast<int>(header.formatVersion) << std::endl;
            std::cout << "  Algorithm: " << algoName 
                      << " (ID: " << static_cast<int>(header.algorithmId) << ")" << std::endl;
            std::cout << "  Original Size: " << header.originalSize << " bytes." << std::endl;
            std::cout << "  Stored CRC32: 0x" << std::hex << header.originalChecksum << std::dec << std::endl;

            // 3. Create compressor based on header info
            auto compressor = createCompressor(header.algorithmId);

            // 4. Extract compressed payload
            std::size_t headerSizeBytes = compression::format::serializedHeaderSize(header);
            std::vector<uint8_t> compressedPayload(
                inputData.begin() + headerSizeBytes,
                inputData.end()
            );
            std::cout << "Compressed payload size: " << compressedPayload.size() << " bytes." << std::endl;

            // 5. Decompress data
            std::vector<uint8_t> outputData;
            if (threadCount <= 1 && header.chunkCount == 1) {
                std::cout << "Decompressing using " << algoName << " strategy..." << std::endl;
                outputData = compressor->decompress(compressedPayload);
            } else {
                std::cout << "Decompressing in parallel with " << threadCount << " threads..." << std::endl;
                compression::ParallelCompressor pc(std::move(compressor), header.algorithmId, threadCount);
                outputData = pc.decompress(inputData);
            }
            std::cout << "Decompressed size: " << outputData.size() << " bytes." << std::endl;

            // 6. Verify original size
            if (outputData.size() != header.originalSize) {
                std::cerr << "Warning: Decompressed size (" << outputData.size() 
                          << ") does not match original size stored in header (" 
                          << header.originalSize << "). File might be corrupt or header incorrect." 
                          << std::endl;
            }

            // 7. Verify CRC32 Checksum
            uint32_t decompressedCRC = compression::utils::crc32Calculator.calculate(outputData);
            std::cout << "Calculated CRC32: 0x" << std::hex << decompressedCRC << std::dec << std::endl;
            if (decompressedCRC != header.originalChecksum) {
                 std::cerr << "ERROR: Checksum mismatch! Header CRC=0x" << std::hex << header.originalChecksum
                           << ", Calculated CRC=0x" << decompressedCRC << std::dec 
                           << ". File is likely corrupt!" << std::endl;
                 // Decide whether to throw, return error code, or just warn
                 throw std::runtime_error("CRC32 Checksum mismatch"); // Make it an error for now
            } else {
                std::cout << "Checksum verified successfully." << std::endl;
            }

            // 8. Write output file
            std::cout << "Writing output file: " << outputFile << "..." << std::endl;
            writeFile(outputFile, outputData);
        }

        std::cout << operation << " completed successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 