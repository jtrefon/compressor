#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <memory>
#include <functional>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <limits>

#include "compression/ICompressor.hpp"
#include "compression/ArithmeticCompressor.hpp"
#include "compression/HuffmanCompressor.hpp"
#include "compression/RleCompressor.hpp"
#include "compression/Lz77Compressor.hpp"
#include "compression/NullCompressor.hpp"
#include "compression/Crc32.hpp"

// Helper struct to store benchmark results
struct BenchmarkResult {
    std::string algorithm;
    std::string dataDescription;
    std::string filePath;
    size_t originalSize;
    size_t compressedSize;
    double compressionRatio;
    double compressionTimeMs;
    double decompressionTimeMs;
    bool validData;
};

// Function to read a file into a vector
std::vector<uint8_t> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read into vector
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    
    if (!file) {
        throw std::runtime_error("Failed to read file: " + filename);
    }
    
    return data;
}

// Function to run a compression benchmark
BenchmarkResult runBenchmark(const std::shared_ptr<compression::ICompressor>& compressor, 
                            const std::vector<uint8_t>& data,
                            const std::string& algorithm,
                            const std::string& dataDescription,
                            const std::string& filePath) {
    BenchmarkResult result;
    result.algorithm = algorithm;
    result.dataDescription = dataDescription;
    result.filePath = filePath;
    result.originalSize = data.size();
    
    // Calculate CRC32 for data integrity check
    compression::utils::Crc32 crc32;
    uint32_t originalCrc = crc32.calculate(data);
    
    // Measure compression time
    auto compressStart = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> compressedData;
    
    try {
        compressedData = compressor->compress(data);
    } catch (const std::exception& e) {
        std::cerr << "Compression error for " << algorithm << ": " << e.what() << std::endl;
        result.compressedSize = 0;
        result.compressionRatio = 0;
        result.compressionTimeMs = 0;
        result.decompressionTimeMs = 0;
        result.validData = false;
        return result;
    }
    
    auto compressEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> compressTime = compressEnd - compressStart;
    
    result.compressedSize = compressedData.size();
    result.compressionRatio = 100.0 * compressedData.size() / data.size();
    result.compressionTimeMs = compressTime.count();
    
    // Measure decompression time
    auto decompressStart = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> decompressedData;
    
    try {
        decompressedData = compressor->decompress(compressedData);
    } catch (const std::exception& e) {
        std::cerr << "Decompression error for " << algorithm << ": " << e.what() << std::endl;
        result.decompressionTimeMs = 0;
        result.validData = false;
        return result;
    }
    
    auto decompressEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> decompressTime = decompressEnd - decompressStart;
    
    result.decompressionTimeMs = decompressTime.count();
    
    // Verify data integrity
    uint32_t decompressedCrc = crc32.calculate(decompressedData);
    result.validData = (originalCrc == decompressedCrc);
    
    return result;
}

// Function to run tests with real files
std::vector<BenchmarkResult> runFileTests(
    const std::vector<std::shared_ptr<compression::ICompressor>>& compressors,
    const std::vector<std::string>& algorithmNames,
    const std::vector<std::string>& filePaths) {
    
    std::vector<BenchmarkResult> results;
    
    for (const auto& filePath : filePaths) {
        try {
            // Extract filename for display
            std::filesystem::path path(filePath);
            std::string filename = path.filename().string();
            
            // Determine file type description
            std::string fileDescription;
            if (filename == "test.png") {
                fileDescription = "PNG Image";
            } else if (filename == "test.txt") {
                fileDescription = "Plain Text";
            } else if (filename == "compression_benchmark") {
                fileDescription = "Executable Binary";
            } else {
                fileDescription = "Unknown";
            }
            
            // Read the file
            std::vector<uint8_t> fileData = readFile(filePath);
            
            // Run benchmarks for this file
            for (size_t i = 0; i < compressors.size(); i++) {
                BenchmarkResult result = runBenchmark(compressors[i], fileData, algorithmNames[i], fileDescription, filePath);
                results.push_back(result);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing file " << filePath << ": " << e.what() << std::endl;
        }
    }
    
    return results;
}

// Function to print results in a nice table
void outputResults(const std::vector<BenchmarkResult>& results) {
    // Find the longest algorithm name for formatting
    size_t maxAlgoLength = 0;
    size_t maxDescLength = 0;
    
    for (const auto& result : results) {
        maxAlgoLength = std::max(maxAlgoLength, result.algorithm.length());
        maxDescLength = std::max(maxDescLength, result.dataDescription.length());
    }
    
    // Print header
    std::cout << "\n";
    std::cout << std::left << std::setw(maxAlgoLength) << "Algorithm"
              << " | " << std::setw(maxDescLength) << "Data Type"
              << " | " << std::setw(12) << "Orig Size"
              << " | " << std::setw(12) << "Comp Size"
              << " | " << std::setw(10) << "Ratio %"
              << " | " << std::setw(15) << "Comp Time (ms)"
              << " | " << std::setw(15) << "Decomp Time (ms)"
              << " | " << "Valid" << std::endl;
    
    std::cout << std::string(maxAlgoLength, '-') << "-+-" 
              << std::string(maxDescLength, '-') << "-+-"
              << std::string(12, '-') << "-+-"
              << std::string(12, '-') << "-+-"
              << std::string(10, '-') << "-+-"
              << std::string(15, '-') << "-+-"
              << std::string(15, '-') << "-+-"
              << std::string(5, '-') << std::endl;
    
    // Group results by data description
    std::string currentDesc = "";
    
    for (const auto& result : results) {
        // Print a separator between different data sets
        if (currentDesc != result.dataDescription) {
            if (!currentDesc.empty()) {
                std::cout << std::string(maxAlgoLength + maxDescLength + 80, '-') << std::endl;
            }
            currentDesc = result.dataDescription;
        }
        
        // Print result row
        std::cout << std::left << std::setw(maxAlgoLength) << result.algorithm
                  << " | " << std::setw(maxDescLength) << result.dataDescription
                  << " | " << std::right << std::setw(10) << result.originalSize << " B"
                  << " | " << std::setw(10) << result.compressedSize << " B"
                  << " | " << std::fixed << std::setprecision(2) << std::setw(8) << result.compressionRatio
                  << " | " << std::setw(13) << std::fixed << std::setprecision(4) << result.compressionTimeMs
                  << " | " << std::setw(13) << std::fixed << std::setprecision(4) << result.decompressionTimeMs
                  << " | " << (result.validData ? "YES" : "NO") << std::endl;
    }
}

// Function to save results to a Markdown file
void saveResultsToMarkdown(const std::vector<BenchmarkResult>& results, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }
    
    // Write markdown header
    file << "# Compression Benchmark Results\n\n";
    file << "This report compares the performance of various compression algorithms on different types of files.\n\n";
    
    // Create a table for each file type
    std::vector<std::string> fileTypes;
    for (const auto& result : results) {
        if (std::find(fileTypes.begin(), fileTypes.end(), result.dataDescription) == fileTypes.end()) {
            fileTypes.push_back(result.dataDescription);
        }
    }
    
    for (const auto& fileType : fileTypes) {
        file << "## " << fileType << "\n\n";
        
        // Find the sample result to get the file size
        auto sampleResult = std::find_if(results.begin(), results.end(),
            [&fileType](const BenchmarkResult& r) { return r.dataDescription == fileType; });
        
        if (sampleResult != results.end()) {
            file << "File size: " << sampleResult->originalSize << " bytes\n\n";
        }
        
        // Create the table header
        file << "| Algorithm | Compressed Size (B) | Ratio (%) | Compression Time (ms) | Decompression Time (ms) | Valid |\n";
        file << "| --------- | ------------------- | --------- | --------------------- | ----------------------- | ----- |\n";
        
        // Add rows for each algorithm
        for (const auto& result : results) {
            if (result.dataDescription == fileType) {
                file << "| " << result.algorithm
                     << " | " << result.compressedSize
                     << " | " << std::fixed << std::setprecision(2) << result.compressionRatio
                     << " | " << std::fixed << std::setprecision(4) << result.compressionTimeMs
                     << " | " << std::fixed << std::setprecision(4) << result.decompressionTimeMs
                     << " | " << (result.validData ? "✓" : "✗") << " |\n";
            }
        }
        
        file << "\n";
    }
    
    // Add summary section
    file << "## Summary\n\n";
    file << "### Best Compression Ratio by File Type\n\n";
    file << "| File Type | Best Algorithm | Compression Ratio (%) |\n";
    file << "| --------- | -------------- | --------------------- |\n";
    
    for (const auto& fileType : fileTypes) {
        // Find the algorithm with the lowest compression ratio (best compression)
        // but only consider valid results (where decompression worked correctly)
        std::optional<BenchmarkResult> bestResult;
        
        for (const auto& result : results) {
            if (result.dataDescription == fileType && result.validData) {
                if (!bestResult.has_value() || result.compressionRatio < bestResult->compressionRatio) {
                    bestResult = result;
                }
            }
        }
        
        if (bestResult.has_value()) {
            file << "| " << fileType
                 << " | " << bestResult->algorithm
                 << " | " << std::fixed << std::setprecision(2) << bestResult->compressionRatio
                 << " |\n";
        } else {
            file << "| " << fileType << " | N/A | N/A |\n";
        }
    }
    
    file << "\n### Best Compression Speed by File Type\n\n";
    file << "| File Type | Best Algorithm | Compression Time (ms) |\n";
    file << "| --------- | -------------- | --------------------- |\n";
    
    for (const auto& fileType : fileTypes) {
        // Find the algorithm with the fastest compression time
        // but only consider valid results (where decompression worked correctly)
        std::optional<BenchmarkResult> bestResult;
        
        for (const auto& result : results) {
            if (result.dataDescription == fileType && result.validData) {
                if (!bestResult.has_value() || result.compressionTimeMs < bestResult->compressionTimeMs) {
                    bestResult = result;
                }
            }
        }
        
        if (bestResult.has_value()) {
            file << "| " << fileType
                 << " | " << bestResult->algorithm
                 << " | " << std::fixed << std::setprecision(4) << bestResult->compressionTimeMs
                 << " |\n";
        } else {
            file << "| " << fileType << " | N/A | N/A |\n";
        }
    }
    
    file << "\n### Compression Algorithm Overall Performance\n\n";
    file << "| Algorithm | Avg. Compression Ratio (%) | Avg. Compression Time (ms) | Reliability |\n";
    file << "| --------- | -------------------------- | -------------------------- | ----------- |\n";
    
    std::vector<std::string> algorithms;
    for (const auto& result : results) {
        if (std::find(algorithms.begin(), algorithms.end(), result.algorithm) == algorithms.end()) {
            algorithms.push_back(result.algorithm);
        }
    }
    
    for (const auto& algorithm : algorithms) {
        double totalRatio = 0.0;
        double totalTime = 0.0;
        int validCount = 0;
        int totalCount = 0;
        
        for (const auto& result : results) {
            if (result.algorithm == algorithm) {
                totalCount++;
                if (result.validData) {
                    validCount++;
                    totalRatio += result.compressionRatio;
                    totalTime += result.compressionTimeMs;
                }
            }
        }
        
        double avgRatio = (validCount > 0) ? totalRatio / validCount : 0.0;
        double avgTime = (validCount > 0) ? totalTime / validCount : 0.0;
        double reliability = (totalCount > 0) ? 100.0 * validCount / totalCount : 0.0;
        
        file << "| " << algorithm
             << " | " << std::fixed << std::setprecision(2) << avgRatio
             << " | " << std::fixed << std::setprecision(4) << avgTime
             << " | " << std::fixed << std::setprecision(1) << reliability << "% |\n";
    }
    
    file << "\n## Conclusion\n\n";
    file << "Based on the benchmark results, here are the key findings:\n\n";
    
    // Determine best overall algorithm for compression ratio
    std::string bestCompressionAlgo = "N/A";
    double bestRatio = std::numeric_limits<double>::max();
    
    for (const auto& algorithm : algorithms) {
        double totalRatio = 0.0;
        int validCount = 0;
        
        for (const auto& result : results) {
            if (result.algorithm == algorithm && result.validData) {
                totalRatio += result.compressionRatio;
                validCount++;
            }
        }
        
        if (validCount > 0) {
            double avgRatio = totalRatio / validCount;
            if (avgRatio < bestRatio) {
                bestRatio = avgRatio;
                bestCompressionAlgo = algorithm;
            }
        }
    }
    
    if (bestCompressionAlgo != "N/A") {
        file << "- Best overall algorithm for compression ratio: **" << bestCompressionAlgo << "**\n";
    } else {
        file << "- No algorithm provided valid compression results\n";
    }
    
    // Determine best overall algorithm for speed
    std::string bestSpeedAlgo = "N/A";
    double bestSpeed = std::numeric_limits<double>::max();
    
    for (const auto& algorithm : algorithms) {
        double totalTime = 0.0;
        int validCount = 0;
        
        for (const auto& result : results) {
            if (result.algorithm == algorithm && result.validData) {
                totalTime += result.compressionTimeMs;
                validCount++;
            }
        }
        
        if (validCount > 0) {
            double avgTime = totalTime / validCount;
            if (avgTime < bestSpeed) {
                bestSpeed = avgTime;
                bestSpeedAlgo = algorithm;
            }
        }
    }
    
    if (bestSpeedAlgo != "N/A") {
        file << "- Best overall algorithm for speed: **" << bestSpeedAlgo << "**\n";
    } else {
        file << "- No algorithm provided valid speed results\n";
    }
    
    // Most reliable algorithm
    std::string mostReliableAlgo = "N/A";
    double bestReliability = 0.0;
    
    for (const auto& algorithm : algorithms) {
        int validCount = 0, totalCount = 0;
        
        for (const auto& result : results) {
            if (result.algorithm == algorithm) {
                totalCount++;
                if (result.validData) validCount++;
            }
        }
        
        if (totalCount > 0) {
            double reliability = static_cast<double>(validCount) / totalCount;
            if (reliability > bestReliability) {
                bestReliability = reliability;
                mostReliableAlgo = algorithm;
            }
        }
    }
    
    if (mostReliableAlgo != "N/A") {
        file << "- Most reliable algorithm: **" << mostReliableAlgo << "** (" 
             << std::fixed << std::setprecision(0) << (bestReliability * 100) << "% success rate)\n\n";
    } else {
        file << "- No algorithm provided reliability results\n\n";
    }
    
    file << "This benchmark was conducted on " << std::filesystem::absolute(std::filesystem::path(filename)).parent_path().string() << "\n";
    
    // Get current time
    std::time_t now = std::time(nullptr);
    file << "Generated on: " << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "\n";
    
    file.close();
    std::cout << "Results saved to " << filename << std::endl;
}

int main(int argc, char** argv) {
    // Create instances of all compressors
    std::vector<std::shared_ptr<compression::ICompressor>> compressors = {
        std::make_shared<compression::ArithmeticCompressor>(),
        std::make_shared<compression::HuffmanCompressor>(),
        std::make_shared<compression::RleCompressor>(),
        std::make_shared<compression::Lz77Compressor>(),
        std::make_shared<compression::NullCompressor>()
    };
    
    std::vector<std::string> algorithmNames = {
        "Arithmetic",
        "Huffman",
        "RLE",
        "LZ77",
        "Null (Identity)"
    };
    
    // Test with the three specified real files
    std::vector<std::string> filePaths = {
        "data/test.png",
        "data/test.txt",
        "data/compression_benchmark"
    };
    
    std::cout << "Running tests on specified files..." << std::endl;
    std::vector<BenchmarkResult> allResults = runFileTests(compressors, algorithmNames, filePaths);
    
    // Output results to console
    outputResults(allResults);
    
    // Save results to markdown file - both for CI compatibility and our detailed report
    saveResultsToMarkdown(allResults, "BENCHMARKS.md");
    saveResultsToMarkdown(allResults, "comprehensive_benchmark_results.md");
    
    std::cout << "Results saved to both BENCHMARKS.md (for CI) and comprehensive_benchmark_results.md (detailed report)" << std::endl;
    
    return 0;
} 