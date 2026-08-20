// app/benchmark.cpp
#include <chrono>
#include <filesystem> // Requires C++17
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <numeric> // std::accumulate (potentially needed later)
#include <sstream> // Include for stringstream
#include <string>
#include <vector>

// Include all compressor headers
#include <compression/ArithmeticCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/ExtremeCompressor.hpp>
#include <compression/FileFormat.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/ICompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <compression/SystemInfo.hpp>
#include <compression/ThreadPool.hpp>
#include <compression/UltraCompressor.hpp>
#include <compression/codec/CodecRegistry.hpp>
#include <compression/codec/ParallelCodecDecorator.hpp>

// Platform-specific defines for popen/pclose
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

// Factory helpers to create compressors by ID or name.
// CodecRegistry is the single source of truth (see codec/CodecRegistry.hpp).

std::unique_ptr<compression::ICompressor>
createCompressor(compression::format::AlgorithmID id) {
  return compression::codec::CodecRegistry::instance().create(id);
}

std::unique_ptr<compression::ICompressor>
createCompressor(const std::string &name) {
  if (name == "Arithmetic") {
    return compression::codec::CodecRegistry::instance().create("arithmetic");
  } else if (name == "Optimized") {
    return compression::codec::CodecRegistry::instance().create("optimized");
  }
  return compression::codec::CodecRegistry::instance().create(name);
}

// --- Helper Functions ---

// Reads a whole file into a byte vector
std::vector<uint8_t> readFile(const std::filesystem::path &filePath) {
  // Open file with shared read access to prevent locking issues
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file) {
    throw std::runtime_error(
        "Cannot open file (may be locked by another process): " +
        filePath.string());
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
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
  bool success = true;
};

// Runs compress/decompress and times them
BenchmarkResult runBenchmark(const std::string &name,
                             compression::format::AlgorithmID algoId,
                             const std::vector<uint8_t> &originalData,
                             std::size_t threads) {
  BenchmarkResult result;
  result.algorithmName = name;
  result.originalSize = originalData.size();

  if (originalData.empty()) {
    return result; // Avoid division by zero and unnecessary work
  }

  std::unique_ptr<compression::ICompressor> base = createCompressor(algoId);

  std::vector<uint8_t> compressedData;
  try {
    auto startCompress = std::chrono::high_resolution_clock::now();
    compression::ThreadPool pool(std::max<std::size_t>(threads, 1));
    compression::codec::ParallelCodecDecorator pc(std::move(base), algoId, &pool,
                                                  threads);
    compressedData = pc.compress(originalData);
    auto endCompress = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> compressDuration =
        endCompress - startCompress;
    result.compressionTimeMs = compressDuration.count();
    result.compressedSize = compressedData.size();
  } catch (const std::exception &e) {
    std::cerr << "ERROR: Compression failed for " << name << ": " << e.what()
              << std::endl;
    result.success = false;
    result.compressionTimeMs = std::numeric_limits<double>::infinity();
    result.decompressionTimeMs = std::numeric_limits<double>::infinity();
    result.compressedSize = 0;
  }

  if (result.success) {
    try {
      auto startDecompress = std::chrono::high_resolution_clock::now();
      compression::ThreadPool poolDec(std::max<std::size_t>(threads, 1));
      compression::codec::ParallelCodecDecorator pcDec(createCompressor(algoId),
                                                       algoId, &poolDec,
                                                       threads);
      std::vector<uint8_t> decompressedData = pcDec.decompress(compressedData);
      auto endDecompress = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> decompressDuration =
          endDecompress - startDecompress;
      result.decompressionTimeMs = decompressDuration.count();

      if (decompressedData != originalData) {
        throw std::runtime_error("Decompression mismatch");
      }
    } catch (const std::exception &e) {
      std::cerr << "ERROR: Decompression failed for " << name << ": "
                << e.what() << std::endl;
      result.success = false;
      result.decompressionTimeMs = std::numeric_limits<double>::infinity();
    }
  }

  // --- Calculate Ratio ---
  if (result.originalSize > 0) {
    result.ratio =
        static_cast<double>(result.compressedSize) / result.originalSize;
  }

  return result;
}

// Helper to read all files in a directory recursively into a single buffer
std::vector<uint8_t> readDirectoryRecursive(const std::filesystem::path &path) {
  std::vector<uint8_t> result;
  if (!std::filesystem::exists(path)) {
    return result;
  }

  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(path)) {
    if (entry.is_regular_file()) {
      try {
        std::vector<uint8_t> fileData = readFile(entry.path());
        result.insert(result.end(), fileData.begin(), fileData.end());
      } catch (const std::exception &e) {
        std::cerr << "Warning: Failed to read " << entry.path() << ": "
                  << e.what() << std::endl;
      }
    }
  }
  return result;
}

// --- Main Function ---

int main(int argc, char *argv[]) {
  // Check for other running benchmarks to prevent file locking
  std::string check_cmd = "pgrep -f 'compression_benchmark' | wc -l";
  FILE *pipe = popen(check_cmd.c_str(), "r");
  if (pipe) {
    char buffer[16];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      int count = std::atoi(buffer);
      if (count > 1) {
        std::cout
            << "⚠️  WARNING: Multiple compression_benchmark processes detected!"
            << std::endl;
        std::cout
            << "    This may cause file locking issues. Continuing anyway..."
            << std::endl;
      }
    }
    pclose(pipe);
  }

  // Parse command line arguments
  std::size_t threadCount = 1; // Default to 1 thread
  bool quick = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind("--threads=", 0) == 0) {
      threadCount = static_cast<std::size_t>(std::stoul(arg.substr(10)));
    } else if (arg == "--quick") {
      quick = true;
    }
  }

  // --- Get Data File Path using Compile Definition ---
#ifndef BENCHMARK_DATA_DIR
#error "BENCHMARK_DATA_DIR is not defined. Check app/CMakeLists.txt"
#endif

  std::filesystem::path dataDir = BENCHMARK_DATA_DIR;
  std::filesystem::path projectRoot =
      dataDir.parent_path(); // Assuming data/ is at root

  // Define all test files
  // We use a lambda or just manual handling for the directory case since it's
  // special
  struct BenchmarkCase {
    std::string desc;
    std::filesystem::path path;
    bool isDirectory;
  };

  std::vector<BenchmarkCase> testCases;
  if (quick) {
    testCases = {{"Text (6.2 MB)", dataDir / "test.txt", false},
                 {"Source Tree (C++ src)", projectRoot / "src", true}};
  } else {
    testCases = {{"Text (6.2 MB)", dataDir / "test.txt", false},
                 {"JPEG Image (2.3 MB)",
                  dataDir / "faizur-rehman-xqh-RlfJVx4-unsplash.jpg", false},
                 {"WAV Audio (9.4 MB)",
                  dataDir /
                      "835222__silverillusionist__ascendancy-music-sample.wav",
                  false},
                 {"Source Tree (C++ src)", projectRoot / "src", true},
                 {"Binary (Executable)",
                  projectRoot / "build/app/compression_benchmark", false}};
  }

  std::map<std::string, std::pair<size_t, size_t>> totalsByAlgorithm;

  std::vector<std::pair<std::string, compression::format::AlgorithmID>>
      algorithms_to_benchmark;
  if (quick) {
    algorithms_to_benchmark = {
        {"Huffman", compression::format::AlgorithmID::HUFFMAN_COMPRESSOR},
        {"LZ77", compression::format::AlgorithmID::LZ77_COMPRESSOR},
        {"BWT", compression::format::AlgorithmID::BWT_COMPRESSOR},
        {"Optimized", compression::format::AlgorithmID::OPTIMIZED_COMPRESSOR}};
  } else {
    algorithms_to_benchmark = {
        {"Huffman", compression::format::AlgorithmID::HUFFMAN_COMPRESSOR},
        {"LZ77", compression::format::AlgorithmID::LZ77_COMPRESSOR},
        {"BWT", compression::format::AlgorithmID::BWT_COMPRESSOR},
        {"Optimized", compression::format::AlgorithmID::OPTIMIZED_COMPRESSOR},
        {"Ultra", compression::format::AlgorithmID::ULTRA_COMPRESSOR},
        {"Extreme", compression::format::AlgorithmID::EXTREME_COMPRESSOR}};
  }

  // Benchmark each file
  for (const auto &testCase : testCases) {
    const std::string &fileDesc = testCase.desc;
    const std::filesystem::path &filePath = testCase.path;
    const bool isDirectory = testCase.isDirectory;

    // Special handling for binary path which might vary
    std::filesystem::path actualPath = filePath;
    if (!std::filesystem::exists(actualPath) &&
        fileDesc.find("Binary") != std::string::npos) {
      if (std::filesystem::exists(projectRoot /
                                  "build/app/Release/compression_benchmark")) {
        actualPath = projectRoot / "build/app/Release/compression_benchmark";
      } else if (std::filesystem::exists(
                     projectRoot / "build/app/Debug/compression_benchmark")) {
        actualPath = projectRoot / "build/app/Debug/compression_benchmark";
      }
    }

    if (!std::filesystem::exists(actualPath)) {
      std::cerr << "⚠️  Skipping " << fileDesc
                << " - file not found: " << actualPath << std::endl;
      continue;
    }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Benchmarking: " << fileDesc << std::endl;
    std::cout << "Path: " << actualPath.filename() << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    std::vector<uint8_t> originalData;
    try {
      if (isDirectory) {
        std::cout << "Reading directory recursively..." << std::endl;
        originalData = readDirectoryRecursive(actualPath);
      } else {
        originalData = readFile(actualPath);
      }
    } catch (const std::exception &e) {
      std::cerr << "Failed to read " << fileDesc << ": " << e.what()
                << std::endl;
      continue;
    }

    if (originalData.empty()) {
      std::cerr << "File is empty, skipping." << std::endl;
      continue;
    }

    std::cout << "Read " << originalData.size() << " bytes." << std::endl;

    // Run benchmarks for this file
    std::vector<BenchmarkResult> results;

    for (const auto &algo_pair : algorithms_to_benchmark) {
      // Single-threaded
      results.push_back(runBenchmark(algo_pair.first + " (1T)",
                                     algo_pair.second, originalData, 1));

      // Multi-threaded
      if (threadCount > 1) {
        results.push_back(runBenchmark(
            algo_pair.first + " (" + std::to_string(threadCount) + "T)",
            algo_pair.second, originalData, threadCount));
      }
    }

    std::cout << "\n--- Results for " << fileDesc << " ---\n" << std::endl;
    std::cout << std::fixed << std::setprecision(3);

    for (const auto &result : results) {
      double ratioPercent = result.ratio * 100.0;

      if (result.success) {
        auto &totals = totalsByAlgorithm[result.algorithmName];
        totals.first += result.originalSize;
        totals.second += result.compressedSize;
      }

      // Console output
      std::cout << "Algorithm:       " << result.algorithmName << std::endl;
      std::cout << "Original Size:   " << result.originalSize << " bytes"
                << std::endl;
      std::cout << "Compressed Size: " << result.compressedSize << " bytes"
                << std::endl;
      std::cout << "Ratio:           " << std::setprecision(2) << ratioPercent
                << "%" << std::endl;
      std::cout << "Compress Time:   " << std::setprecision(3)
                << result.compressionTimeMs << " ms" << std::endl;
      std::cout << "Decompress Time: " << result.decompressionTimeMs << " ms"
                << std::endl;
      if (!result.success) {
        std::cout << "Status:          FAILED" << std::endl;
      }
      std::cout << "-------------------------" << std::endl;
    }
  }

  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Aggregate Summary (successful runs only)" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  for (const auto &entry : totalsByAlgorithm) {
    const auto &algoName = entry.first;
    const size_t totalOriginal = entry.second.first;
    const size_t totalCompressed = entry.second.second;

    if (totalOriginal == 0) {
      continue;
    }

    const double aggregateRatio =
        static_cast<double>(totalCompressed) / static_cast<double>(totalOriginal);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << algoName << ": " << (aggregateRatio * 100.0)
              << "% (" << totalCompressed << "/" << totalOriginal << " bytes)"
              << std::endl;
  }

  return 0;
}