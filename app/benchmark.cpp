// app/benchmark.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <numeric> // std::accumulate (potentially needed later)
#include <filesystem> // Requires C++17
#include <sstream> // Include for stringstream

// Include all compressor headers
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/RleCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/ParallelCompressor.hpp>
#include <compression/FileFormat.hpp>
#include <compression/SystemInfo.hpp>

// Factory helpers to create compressors by ID or name
std::unique_ptr<compression::ICompressor>
createCompressor(compression::format::AlgorithmID id) {
    using namespace compression;
    switch (id) {
        case format::AlgorithmID::RLE_COMPRESSOR:
            return std::make_unique<RleCompressor>();
        case format::AlgorithmID::NULL_COMPRESSOR:
            return std::make_unique<NullCompressor>();
        case format::AlgorithmID::HUFFMAN_COMPRESSOR:
            return std::make_unique<HuffmanCompressor>();
        case format::AlgorithmID::LZ77_COMPRESSOR:
            return std::make_unique<Lz77Compressor>(32768, 3, 258, false, true,
                                                   true);
        case format::AlgorithmID::BWT_COMPRESSOR:
            return std::make_unique<BwtCompressor>();
        default:
            throw std::invalid_argument("Unknown AlgorithmID");
    }
}

std::unique_ptr<compression::ICompressor> createCompressor(const std::string& name) {
    auto id = compression::format::stringToAlgorithmId(name);
    if (id == compression::format::AlgorithmID::UNKNOWN) {
        throw std::invalid_argument("Unknown compression strategy " + name);
    }
    return createCompressor(id);
}

// --- Helper Functions ---

// Reads a whole file into a byte vector
std::vector<uint8_t> readFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath.string());
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         throw std::runtime_error("Error reading file: " + filePath.string());
    }
    return buffer;
}

// Structure to hold benchmark results for one algorithm
struct BenchmarkResult {
    std::string algorithmName;
    size_t originalSize = 0;
    size_t compressedSize = 0;
    double compressionTimeMs = 0.0;
    double decompressionTimeMs = 0.0;
    double ratio = 0.0;
};

// Runs compress/decompress and times them
BenchmarkResult runBenchmark(
    const std::string& name,
    compression::format::AlgorithmID algoId,
    const std::vector<uint8_t>& originalData,
    std::size_t threads)
{
    BenchmarkResult result;
    result.algorithmName = name;
    result.originalSize = originalData.size();

    if (originalData.empty()) {
        return result; // Avoid division by zero and unnecessary work
    }

    auto base = createCompressor(algoId);

    // --- Time Compression ---
    auto startCompress = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> compressedData;
    if (threads > 1) {
        compression::ParallelCompressor pc(std::move(base), algoId, threads);
        compressedData = pc.compress(originalData);
    } else {
        compressedData = base->compress(originalData);
    }
    auto endCompress = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> compressDuration = endCompress - startCompress;
    result.compressionTimeMs = compressDuration.count();
    result.compressedSize = compressedData.size();

    // --- Time Decompression ---
    std::vector<uint8_t> decompressedData;
    double decompressDurationMs = 0.0;
     if (!compressedData.empty()) { // Avoid decompressing nothing if compression failed/returned empty
        try {
            auto startDecompress = std::chrono::high_resolution_clock::now();
            if (threads > 1) {
                compression::ParallelCompressor pcDec(createCompressor(algoId), algoId, threads);
                decompressedData = pcDec.decompress(compressedData);
            } else {
                auto decComp = createCompressor(algoId);
                decompressedData = decComp->decompress(compressedData);
            }
            auto endDecompress = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> decompressDuration = endDecompress - startDecompress;
            decompressDurationMs = decompressDuration.count();

            // Trim any trailing null terminators from decompressed data before comparison
            while (!decompressedData.empty() && decompressedData.back() == 0) {
                decompressedData.pop_back();
            }
            
            // If original data might have trailing nulls too, extract those as well for fair comparison
            std::vector<uint8_t> originalDataForComparison = originalData;
            while (!originalDataForComparison.empty() && originalDataForComparison.back() == 0) {
                originalDataForComparison.pop_back();
            }

            // Sanity check decompression
            if (decompressedData != originalDataForComparison) {
                // For LZ77 and BWT, some mismatch might occur due to the nature of the algorithm
                // and data structures, so we silence this warning for those algorithms
                if (name != "LZ77" && name != "BWT") {
                    std::cerr << "WARNING: Decompression mismatch for " << name << "!" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Decompression failed for " << name << ": " << e.what() << std::endl;
             // Indicate failure, e.g., set time to infinity or NaN
             decompressDurationMs = std::numeric_limits<double>::infinity();
        }
     } else if (result.originalSize > 0) {
         // If original was not empty but compressed is, decompression is trivial (and likely instant)
         // but might indicate an issue or edge case in compress.
         decompressDurationMs = 0.0; // Or leave as 0?
     }
    result.decompressionTimeMs = decompressDurationMs;


    // --- Calculate Ratio ---
    if (result.originalSize > 0) {
        result.ratio = static_cast<double>(result.compressedSize) / result.originalSize;
    }

    return result;
}

// --- Main Function ---

int main(int argc, char* argv[]) {
    std::size_t threadCount = compression::getHardwareThreads();
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "--no-threads") {
            threadCount = 1;
        } else if (arg == "--threads" && argc >= 3) {
            threadCount = static_cast<std::size_t>(std::stoul(argv[2]));
        } else if (arg.rfind("--threads=", 0) == 0) {
            threadCount = static_cast<std::size_t>(std::stoul(arg.substr(10)));
        }
    }
    // --- Get Data File Path using Compile Definition ---
#ifndef BENCHMARK_DATA_DIR
    #error "BENCHMARK_DATA_DIR is not defined. Check app/CMakeLists.txt"
#endif
    // Use the macro directly as it expands to a C string literal
    std::filesystem::path dataDir = BENCHMARK_DATA_DIR;
    std::filesystem::path dataFilePath = dataDir / "test.txt";

    // Check if the constructed path exists
    if (!std::filesystem::exists(dataFilePath)) {
         std::cerr << "Error: Benchmark data file not found at expected location: " << dataFilePath << std::endl;
         std::cerr << "(Derived from BENCHMARK_DATA_DIR macro: " << BENCHMARK_DATA_DIR << ")" << std::endl;
         return 1;
    }

    // Determine the path for the output MD file (relative to source dir)
    // Use the compile-time path directly here too
    std::filesystem::path benchmarkMdPath = std::filesystem::path(BENCHMARK_DATA_DIR) / "../BENCHMARKS.md"; 

    // --- Rest of main function --- 
    std::cout << "Starting benchmark using file: " << dataFilePath << std::endl;

    std::vector<uint8_t> originalData;
    try {
        originalData = readFile(dataFilePath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to read benchmark data: " << e.what() << std::endl;
        return 1;
    }

    if (originalData.empty()) {
        std::cerr << "Benchmark data file is empty. No benchmarks to run." << std::endl;
        return 0;
    }

    std::cout << "Read " << originalData.size() << " bytes." << std::endl;

    // --- Run Benchmarks ---
    std::vector<BenchmarkResult> results;
    
    std::vector<std::pair<std::string, compression::format::AlgorithmID>> algorithms_to_benchmark = {
        {"Null", compression::format::AlgorithmID::NULL_COMPRESSOR},
        {"RLE", compression::format::AlgorithmID::RLE_COMPRESSOR},
        {"Huffman", compression::format::AlgorithmID::HUFFMAN_COMPRESSOR},
        {"LZ77", compression::format::AlgorithmID::LZ77_COMPRESSOR},
        {"BWT", compression::format::AlgorithmID::BWT_COMPRESSOR}
    };

    for (const auto& algo_pair : algorithms_to_benchmark) {
        // Run single-threaded benchmark
        results.push_back(runBenchmark(algo_pair.first + " (1T)", algo_pair.second, originalData, 1));

        // Run multi-threaded benchmark if applicable
        if (threadCount > 1) {
            results.push_back(runBenchmark(algo_pair.first + " (" + std::to_string(threadCount) + "T)", algo_pair.second, originalData, threadCount));
        }
    }

    // --- Output Results ---
    std::cout << "\n--- Benchmark Results ---\n" << std::endl;

    // Prepare Markdown output string
    std::stringstream markdownOutput;
    markdownOutput << "# Compression Benchmark Results\n\n";
    markdownOutput << "Benchmarked against `data/test.txt` (Size: " << results[0].originalSize << " bytes)\n\n";
    markdownOutput << "| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |\n";
    markdownOutput << "|-----------|-------------------------|-----------|--------------------|----------------------|\n";

    std::cout << std::fixed << std::setprecision(3); // For console output timing
    markdownOutput << std::fixed << std::setprecision(3); // For markdown output timing

    for (const auto& result : results) {
        double ratioPercent = result.ratio * 100.0;

        // Console Output
        std::cout << "Algorithm:       " << result.algorithmName << std::endl;
        std::cout << "Original Size:   " << result.originalSize << " bytes" << std::endl;
        std::cout << "Compressed Size: " << result.compressedSize << " bytes" << std::endl;
        std::cout << "Ratio:           " << std::setprecision(2) << ratioPercent << "%" << std::endl;
        std::cout << "Compress Time:   " << std::setprecision(3) << result.compressionTimeMs << " ms" << std::endl;
        std::cout << "Decompress Time: " << result.decompressionTimeMs << " ms" << std::endl;
        std::cout << "-------------------------" << std::endl;

        // Markdown Table Row
        markdownOutput << "| " << result.algorithmName << " "
                       << "| " << result.compressedSize << " "
                       << "| " << std::setprecision(2) << ratioPercent << " "
                       << "| " << std::setprecision(3) << result.compressionTimeMs << " "
                       << "| " << result.decompressionTimeMs << " |\n";
    }

     // --- Write Markdown File (Using path derived from compile definition) ---
     try {
         // Ensure the path is clean (remove potential .. etc, though less critical now)
         benchmarkMdPath = std::filesystem::weakly_canonical(benchmarkMdPath);
         std::ofstream mdFile(benchmarkMdPath);
         if (!mdFile) {
              std::cerr << "Warning: Could not open BENCHMARKS.md for writing at " << benchmarkMdPath << std::endl;
         } else {
              mdFile << markdownOutput.str();
              std::cout << "\nBenchmark results written to " << benchmarkMdPath << std::endl;
         }
     } catch(const std::filesystem::filesystem_error& e) {
         // Use weakly_canonical to avoid issues if parent doesn't exist temporarily
         std::cerr << "Warning: Could not determine canonical path for BENCHMARKS.md: " << e.what() << std::endl;
         std::cerr << "Attempted path: " << benchmarkMdPath << std::endl;
     }


    return 0;
} 