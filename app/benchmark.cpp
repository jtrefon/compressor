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

// Include all compressor headers
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/RleCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>

// --- Helper Functions ---

// Reads a whole file into a byte vector
std::vector<std::byte> readFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath.string());
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::byte> buffer(size);
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
    const std::vector<std::byte>& originalData)
{
    BenchmarkResult result;
    result.algorithmName = name;
    result.originalSize = originalData.size();

    if (originalData.empty()) {
        return result; // Avoid division by zero and unnecessary work
    }

    // --- Time Compression ---
    auto startCompress = std::chrono::high_resolution_clock::now();
    std::vector<std::byte> compressedData = compressor.compress(originalData);
    auto endCompress = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> compressDuration = endCompress - startCompress;
    result.compressionTimeMs = compressDuration.count();
    result.compressedSize = compressedData.size();

    // --- Time Decompression ---
    std::vector<std::byte> decompressedData;
    double decompressDurationMs = 0.0;
     if (!compressedData.empty()) { // Avoid decompressing nothing if compression failed/returned empty
        try {
            auto startDecompress = std::chrono::high_resolution_clock::now();
            decompressedData = compressor.decompress(compressedData);
            auto endDecompress = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> decompressDuration = endDecompress - startDecompress;
            decompressDurationMs = decompressDuration.count();

            // Sanity check decompression
            if (decompressedData != originalData) {
                 std::cerr << "WARNING: Decompression mismatch for " << name << "!" << std::endl;
                 // Handle error state? Set times to NaN or similar? For now, just report time.
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
    // Determine the path to the data file relative to the executable location
    // Assume executable is in 'build/app/', data is in 'data/'
    std::filesystem::path executablePath = std::filesystem::current_path(); // Path where benchmark is run (usually build/app)
    std::filesystem::path dataFilePath = executablePath / "../../data/test.txt"; // Go up two levels from build/app

     // Canonical path helps resolve relative paths robustly
    try {
         dataFilePath = std::filesystem::canonical(dataFilePath);
    } catch(const std::filesystem::filesystem_error& e) {
         std::cerr << "Error finding data file: " << e.what() << std::endl;
         std::cerr << "Attempted path: " << dataFilePath << std::endl;
         std::cerr << "Current directory: " << executablePath << std::endl;
         return 1;
    }


    std::cout << "Starting benchmark using file: " << dataFilePath << std::endl;

    std::vector<std::byte> originalData;
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
    compression::Lz77Compressor lz77Comp; // Using default window sizes

    // --- Run Benchmarks ---
    std::vector<BenchmarkResult> results;
    results.push_back(runBenchmark("Null", nullComp, originalData));
    results.push_back(runBenchmark("RLE", rleComp, originalData));
    results.push_back(runBenchmark("Huffman", huffmanComp, originalData));
    results.push_back(runBenchmark("LZ77", lz77Comp, originalData));

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

     // --- Write Markdown File ---
     std::filesystem::path benchmarkMdPath = executablePath / "../../BENCHMARKS.md";
     try {
         benchmarkMdPath = std::filesystem::canonical(benchmarkMdPath.parent_path()) / benchmarkMdPath.filename();
         std::ofstream mdFile(benchmarkMdPath);
         if (!mdFile) {
              std::cerr << "Warning: Could not open BENCHMARKS.md for writing at " << benchmarkMdPath << std::endl;
         } else {
              mdFile << markdownOutput.str();
              std::cout << "\nBenchmark results written to " << benchmarkMdPath << std::endl;
         }
     } catch(const std::filesystem::filesystem_error& e) {
         std::cerr << "Warning: Could not determine canonical path for BENCHMARKS.md: " << e.what() << std::endl;
         std::cerr << "Attempted path: " << benchmarkMdPath << std::endl;
     }


    return 0;
} 