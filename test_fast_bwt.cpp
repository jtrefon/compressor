#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <chrono>

// Test the fast BWT algorithm performance
std::vector<uint8_t> fast_bwt_transform(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<uint32_t> suffix_array(n);
    std::iota(suffix_array.begin(), suffix_array.end(), 0);
    
    std::vector<uint32_t> rank(n);
    std::vector<uint32_t> new_rank(n);
    
    // Initial ranking based on single characters
    for (size_t i = 0; i < n; ++i) {
        rank[i] = data[i];
    }
    
    // Sort by 2k substrings, doubling k each iteration
    for (size_t k = 1; k < n; k <<= 1) {
        std::cout << "Iteration k=" << k << "...";
        auto iter_start = std::chrono::high_resolution_clock::now();
        
        // Sort suffix array using current ranks
        std::sort(suffix_array.begin(), suffix_array.end(), 
            [&rank, n, k](uint32_t a, uint32_t b) {
                uint32_t ra = rank[a];
                uint32_t rb = rank[b];
                uint32_t ra2 = (a + k < n) ? rank[a + k] : 0;
                uint32_t rb2 = (b + k < n) ? rank[b + k] : 0;
                
                if (ra != rb) return ra < rb;
                return ra2 < rb2;
            });
        
        auto iter_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> iter_duration = iter_end - iter_start;
        std::cout << " took " << iter_duration.count() << "ms" << std::endl;
        
        // Update rankings
        new_rank[suffix_array[0]] = 0;
        for (size_t i = 1; i < n; ++i) {
            uint32_t a = suffix_array[i-1];
            uint32_t b = suffix_array[i];
            
            uint32_t ra = rank[a];
            uint32_t rb = rank[b];
            uint32_t ra2 = (a + k < n) ? rank[a + k] : 0;
            uint32_t rb2 = (b + k < n) ? rank[b + k] : 0;
            
            if (ra == rb && ra2 == rb2) {
                new_rank[b] = new_rank[a];
            } else {
                new_rank[b] = new_rank[a] + 1;
            }
        }
        
        rank.swap(new_rank);
        
        // Early termination if all suffixes have unique ranks
        if (rank[suffix_array[n-1]] == n-1) break;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Total BWT transform time: " << duration.count() << "ms" << std::endl;

    // For now, just return a dummy result to test performance
    std::vector<uint8_t> result = data;
    return result;
}

int main() {
    std::ifstream file("../data/test.txt", std::ios::binary);
    std::vector<uint8_t> data(100000);
    file.read(reinterpret_cast<char*>(data.data()), 100000);
    size_t bytesRead = file.gcount();
    data.resize(bytesRead);
    
    std::cout << "Testing fast BWT on " << data.size() << " bytes" << std::endl;
    
    try {
        auto result = fast_bwt_transform(data);
        std::cout << "✅ Fast BWT completed" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
    }
    
    return 0;
}
