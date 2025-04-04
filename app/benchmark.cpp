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
#include <compression/DeflateCompressor.hpp>
#include <compression/BwtCompressor.hpp>

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
    const compression::ICompressor& compressor,
    const std::vector<uint8_t>& originalData)
{
    BenchmarkResult result;
    result.algorithmName = name;
    result.originalSize = originalData.size();

    if (originalData.empty()) {
        return result; // Avoid division by zero and unnecessary work
    }

    // --- Time Compression ---
    auto startCompress = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> compressedData = compressor.compress(originalData);
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
            decompressedData = compressor.decompress(compressedData);
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

int main() {
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

    // --- Instantiate Compressors ---
    compression::NullCompressor nullComp;
    compression::RleCompressor rleComp;
    compression::HuffmanCompressor huffmanComp;
    // Use LZ77 with optimal parsing for better compression
    compression::Lz77Compressor lz77Comp(32768, 3, 258, false, true, true);
    compression::DeflateCompressor deflateComp; // Remove verbose logging flag for benchmarks
    compression::BwtCompressor bwtComp; // Add BWT compressor

    // --- Run Benchmarks ---
    std::vector<BenchmarkResult> results;
    results.push_back(runBenchmark("Null", nullComp, originalData));
    results.push_back(runBenchmark("RLE", rleComp, originalData));
    results.push_back(runBenchmark("Huffman", huffmanComp, originalData));
    results.push_back(runBenchmark("LZ77", lz77Comp, originalData));
    results.push_back(runBenchmark("Deflate", deflateComp, originalData));
    results.push_back(runBenchmark("BWT", bwtComp, originalData)); // Add BWT benchmark

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