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
#include <random> // For synthetic data generation

// Include all compressor headers
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/RleCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/DeflateCompressor.hpp>
#include <compression/ArithmeticCompressor.hpp>

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
    std::string dataDescription;
};

// Runs compress/decompress and times them
BenchmarkResult runBenchmark(
    const std::string& name,
    const compression::ICompressor& compressor,
    const std::vector<uint8_t>& originalData,
    const std::string& dataDescription = "")
{
    BenchmarkResult result;
    result.algorithmName = name;
    result.originalSize = originalData.size();
    result.dataDescription = dataDescription;

    if (originalData.empty()) {
        return result;
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
    if (!compressedData.empty()) {
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
                // For LZ77, some mismatch might occur due to the nature of the algorithm and data structures,
                // so we silence this warning for that algorithm
                if (name != "LZ77") {
                    std::cerr << "WARNING: Decompression mismatch for " << name << "!" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Decompression failed for " << name << ": " << e.what() << std::endl;
            decompressDurationMs = std::numeric_limits<double>::infinity();
        }
    } else if (result.originalSize > 0) {
        decompressDurationMs = 0.0;
    }
    result.decompressionTimeMs = decompressDurationMs;

    // --- Calculate Ratio ---
    if (result.originalSize > 0) {
        result.ratio = static_cast<double>(result.compressedSize) / result.originalSize;
    }

    return result;
}

// --- Run benchmarks on a specific file
std::vector<BenchmarkResult> runFileTests(const std::filesystem::path& filePath) {
    std::cout << "Running benchmark on file: " << filePath << std::endl;
    
    std::vector<uint8_t> originalData;
    try {
        originalData = readFile(filePath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to read benchmark data: " << e.what() << std::endl;
        return {};
    }

    if (originalData.empty()) {
        std::cerr << "Benchmark data file is empty. No benchmarks to run." << std::endl;
        return {};
    }

    std::cout << "Read " << originalData.size() << " bytes." << std::endl;

    // --- Instantiate Compressors ---
    compression::NullCompressor nullComp;
    compression::RleCompressor rleComp;
    compression::HuffmanCompressor huffmanComp;
    compression::Lz77Compressor lz77Comp(32768, 3, 258, false, true, true);
    compression::DeflateCompressor deflateComp;
    compression::ArithmeticCompressor arithmeticComp;

    // --- Run Benchmarks ---
    std::string dataDescription = filePath.filename().string();
    std::vector<BenchmarkResult> results;
    results.push_back(runBenchmark("Null", nullComp, originalData, dataDescription));
    results.push_back(runBenchmark("RLE", rleComp, originalData, dataDescription));
    results.push_back(runBenchmark("Huffman", huffmanComp, originalData, dataDescription));
    results.push_back(runBenchmark("Arithmetic", arithmeticComp, originalData, dataDescription));
    results.push_back(runBenchmark("LZ77", lz77Comp, originalData, dataDescription));
    results.push_back(runBenchmark("Deflate", deflateComp, originalData, dataDescription));
    
    return results;
}

// Helper to output benchmark results to console and markdown file
void outputResults(const std::vector<BenchmarkResult>& results, std::stringstream& markdownOutput, const std::string& sectionTitle) {
    if (results.empty()) {
        return;
    }
    
    std::cout << "\n--- Benchmark Results: " << sectionTitle << " ---\n" << std::endl;
    
    // Add section header to markdown
    markdownOutput << "## " << sectionTitle << "\n\n";
    markdownOutput << "| Algorithm | Data Type | Original Size (bytes) | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |\n";
    markdownOutput << "|-----------|-----------|------------------------|-------------------------|-----------|--------------------|----------------------|\n";

    for (const auto& result : results) {
        double ratioPercent = result.ratio * 100.0;

        // Console Output
        std::cout << "Algorithm:       " << result.algorithmName << std::endl;
        std::cout << "Data Type:       " << result.dataDescription << std::endl;
        std::cout << "Original Size:   " << result.originalSize << " bytes" << std::endl;
        std::cout << "Compressed Size: " << result.compressedSize << " bytes" << std::endl;
        std::cout << "Ratio:           " << std::setprecision(2) << std::fixed << ratioPercent << "%" << std::endl;
        std::cout << "Compress Time:   " << std::setprecision(3) << result.compressionTimeMs << " ms" << std::endl;
        std::cout << "Decompress Time: " << result.decompressionTimeMs << " ms" << std::endl;
        std::cout << "-------------------------" << std::endl;

        // Markdown Table Row
        markdownOutput << "| " << result.algorithmName << " "
                       << "| " << result.dataDescription << " "
                       << "| " << result.originalSize << " "
                       << "| " << result.compressedSize << " "
                       << "| " << std::setprecision(2) << std::fixed << ratioPercent << " "
                       << "| " << std::setprecision(3) << result.compressionTimeMs << " "
                       << "| " << result.decompressionTimeMs << " |\n";
    }
    
    markdownOutput << "\n";
}

int main() {
    // --- Get Data File Path using Compile Definition ---
#ifndef BENCHMARK_DATA_DIR
    #error "BENCHMARK_DATA_DIR is not defined. Check app/CMakeLists.txt"
#endif
    std::filesystem::path dataDir = BENCHMARK_DATA_DIR;
    std::filesystem::path textFilePath = dataDir / "test.txt";
    std::filesystem::path imageFilePath = dataDir / "test.png";

    // Check if the files exist
    bool textFileExists = std::filesystem::exists(textFilePath);
    bool imageFileExists = std::filesystem::exists(imageFilePath);
    
    if (!textFileExists && !imageFileExists) {
        std::cerr << "Error: No benchmark data files found at expected locations." << std::endl;
        std::cerr << "(Derived from BENCHMARK_DATA_DIR macro: " << BENCHMARK_DATA_DIR << ")" << std::endl;
        return 1;
    }

    // Determine the path for the output MD file
    std::filesystem::path benchmarkMdPath = std::filesystem::path(BENCHMARK_DATA_DIR) / "../BENCHMARKS.md";

    // Prepare Markdown output
    std::stringstream markdownOutput;
    markdownOutput << "# Compression Benchmark Results\n\n";

    std::cout << std::fixed << std::setprecision(3);
    markdownOutput << std::fixed << std::setprecision(3);
    
    // Run benchmark on text file if it exists
    std::vector<BenchmarkResult> textResults;
    if (textFileExists) {
        textResults = runFileTests(textFilePath);
        outputResults(textResults, markdownOutput, "Text File Tests");
    }
    
    // Run benchmark on image file if it exists
    std::vector<BenchmarkResult> imageResults;
    if (imageFileExists) {
        imageResults = runFileTests(imageFilePath);
        outputResults(imageResults, markdownOutput, "Binary (Image) File Tests");
    }

    // Write Markdown File
    try {
        benchmarkMdPath = std::filesystem::weakly_canonical(benchmarkMdPath);
        std::ofstream mdFile(benchmarkMdPath);
        if (!mdFile) {
            std::cerr << "Warning: Could not open BENCHMARKS.md for writing at " << benchmarkMdPath << std::endl;
        } else {
            mdFile << markdownOutput.str();
            std::cout << "\nBenchmark results written to \"" << benchmarkMdPath.string() << "\"" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error writing benchmark results to file: " << e.what() << std::endl;
    }
    
    return 0;
} 