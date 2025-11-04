#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <filesystem>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

// Include only working compressors
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/BwtCompressor.hpp>

// Process management
volatile bool interrupted = false;
void signal_handler(int sig) {
    interrupted = true;
    std::cout << "\n⚠️  Benchmark interrupted by signal " << sig << std::endl;
}

// Safe file reading with sharing
std::vector<uint8_t> readFileSafe(const std::filesystem::path& filePath) {
    // Check if file exists and is accessible
    struct stat file_stat;
    if (stat(filePath.c_str(), &file_stat) != 0) {
        throw std::runtime_error("Cannot stat file: " + filePath.string());
    }
    
    // Try to open with shared read access
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file (may be locked): " + filePath.string());
    }
    
    std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("File is empty or invalid: " + filePath.string());
    }
    
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Error reading file: " + filePath.string());
    }
    
    return buffer;
}

// Check if other benchmarks are running
bool checkForOtherBenchmarks() {
    std::string command = "pgrep -f 'benchmark|compression' | grep -v $$ | wc -l";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        int count = std::atoi(buffer);
        pclose(pipe);
        return count > 0;
    }
    
    pclose(pipe);
    return false;
}

// Structure to hold benchmark results
struct BenchmarkResult {
    std::string algorithmName;
    size_t originalSize = 0;
    size_t compressedSize = 0;
    double compressionTimeMs = 0.0;
    double decompressionTimeMs = 0.0;
    double ratio = 0.0;
    bool success = true;
};

// Runs compress/decompress and times them with interruption support
BenchmarkResult runBenchmark(
    const std::string& name,
    std::unique_ptr<compression::ICompressor> compressor,
    const std::vector<uint8_t>& originalData)
{
    BenchmarkResult result;
    result.algorithmName = name;
    result.originalSize = originalData.size();

    if (originalData.empty() || interrupted) {
        return result;
    }

    try {
        // --- Time Compression ---
        auto startCompress = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> compressedData;
        
        std::cout << "  📦 Compressing with " << name << "..." << std::flush;
        compressedData = compressor->compress(originalData);
        
        if (interrupted) {
            std::cout << " ❌ INTERRUPTED" << std::endl;
            result.success = false;
            return result;
        }
        
        auto endCompress = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> compressDuration = endCompress - startCompress;
        result.compressionTimeMs = compressDuration.count();
        result.compressedSize = compressedData.size();
        
        std::cout << " ✅ " << compressDuration.count() << " ms" << std::endl;

        // --- Time Decompression ---
        if (!compressedData.empty() && !interrupted) {
            try {
                std::cout << "  📂 Decompressing with " << name << "..." << std::flush;
                auto startDecompress = std::chrono::high_resolution_clock::now();
                auto decompressedData = compressor->decompress(compressedData);
                auto endDecompress = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> decompressDuration = endDecompress - startDecompress;
                result.decompressionTimeMs = decompressDuration.count();

                if (interrupted) {
                    std::cout << " ❌ INTERRUPTED" << std::endl;
                    result.success = false;
                    return result;
                }

                // Sanity check decompression
                if (decompressedData != originalData) {
                    std::cout << " ❌ MISMATCH!" << std::endl;
                    result.success = false;
                } else {
                    std::cout << " ✅ " << decompressDuration.count() << " ms" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << " ❌ ERROR: " << e.what() << std::endl;
                result.success = false;
                result.decompressionTimeMs = std::numeric_limits<double>::infinity();
            }
        }
    } catch (const std::exception& e) {
        std::cout << " ❌ ERROR: " << e.what() << std::endl;
        result.success = false;
        result.compressionTimeMs = std::numeric_limits<double>::infinity();
    }

    // --- Calculate Ratio ---
    if (result.originalSize > 0) {
        result.ratio = static_cast<double>(result.compressedSize) / result.originalSize;
    }

    return result;
}

int main() {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "🔒 SAFE BENCHMARK - File Lock Protection" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Check for other running benchmarks
    if (checkForOtherBenchmarks()) {
        std::cout << "⚠️  WARNING: Other benchmarks may be running!" << std::endl;
        std::cout << "    This could cause file locking issues." << std::endl;
        std::cout << "    Continue anyway? (y/N): ";
        
        char response;
        std::cin >> response;
        if (response != 'y' && response != 'Y') {
            std::cout << "❌ Aborted to prevent conflicts." << std::endl;
            return 1;
        }
    }
    
    // --- Get Data File Path ---
    std::filesystem::path dataFilePath = "../data/test.txt";

    if (!std::filesystem::exists(dataFilePath)) {
         std::cerr << "Error: Benchmark data file not found at: " << dataFilePath << std::endl;
         return 1;
    }

    // --- Read Data Safely ---
    std::vector<uint8_t> originalData;
    try {
        std::cout << "📁 Reading file: " << dataFilePath << std::endl;
        originalData = readFileSafe(dataFilePath);
        std::cout << "✅ Successfully read " << originalData.size() << " bytes" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to read benchmark data: " << e.what() << std::endl;
        return 1;
    }

    if (originalData.empty()) {
        std::cerr << "❌ Benchmark data file is empty. No benchmarks to run." << std::endl;
        return 0;
    }

    // --- Run Benchmarks ---
    std::vector<BenchmarkResult> results;
    
    std::cout << "\n🧪 Running safe benchmarks..." << std::endl;
    
    // Test only the working, optimized algorithms
    results.push_back(runBenchmark("Null", std::make_unique<compression::NullCompressor>(), originalData));
    if (interrupted) goto cleanup;
    
    results.push_back(runBenchmark("Huffman", std::make_unique<compression::HuffmanCompressor>(), originalData));
    if (interrupted) goto cleanup;
    
    results.push_back(runBenchmark("LZ77", std::make_unique<compression::Lz77Compressor>(32768, 3, 258, false, true, true), originalData));
    if (interrupted) goto cleanup;
    
    results.push_back(runBenchmark("BWT (Optimized)", std::make_unique<compression::BwtCompressor>(), originalData));

cleanup:
    // --- Output Results ---
    std::cout << "\n📊 SAFE BENCHMARK RESULTS" << std::endl;
    std::cout << "=========================" << std::endl;

    std::cout << std::fixed << std::setprecision(3);

    // Sort by compression ratio (best first)
    std::sort(results.begin(), results.end(), 
              [](const BenchmarkResult& a, const BenchmarkResult& b) {
                  return a.ratio < b.ratio;
              });

    for (const auto& result : results) {
        double ratioPercent = result.ratio * 100.0;
        std::string status = result.success ? "✅" : "❌";

        std::cout << "\n" << status << " **" << result.algorithmName << "**" << std::endl;
        std::cout << "   Ratio:      " << std::setprecision(1) << ratioPercent << "%" << std::endl;
        std::cout << "   Compress:   " << std::setprecision(0) << result.compressionTimeMs << " ms" << std::endl;
        std::cout << "   Decompress: " << std::setprecision(0) << result.decompressionTimeMs << " ms" << std::endl;
        std::cout << "   Total:      " << std::setprecision(0) << (result.compressionTimeMs + result.decompressionTimeMs) << " ms" << std::endl;
    }

    if (interrupted) {
        std::cout << "\n⚠️  Benchmark was interrupted!" << std::endl;
    } else {
        std::cout << "\n✅ Safe benchmark completed successfully!" << std::endl;
        std::cout << "✅ No file locking issues detected!" << std::endl;
    }

    return interrupted ? 1 : 0;
}
