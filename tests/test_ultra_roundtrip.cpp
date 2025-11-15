#include <compression/UltraCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

static std::vector<uint8_t> read_prefix(const std::string& path, size_t n){
    std::ifstream f(path, std::ios::binary); if(!f) throw std::runtime_error("cannot open file");
    std::vector<uint8_t> buf(n); f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(n));
    buf.resize(static_cast<size_t>(f.gcount())); return buf;
}

static bool run_case(size_t n){
    auto data = read_prefix("../data/test.txt", n);
    compression::UltraCompressor comp;
    auto c = comp.compress(data);
    auto d = comp.decompress(c);
    if (d != data){
        std::cerr << "Ultra mismatch at size=" << data.size() << std::endl;
        return false;
    }
    return true;
}

int main(){
    try{
        if(!run_case(1000)) return 1;
        if(!run_case(10000)) return 1;
        if(!run_case(100000)) return 1;
        if(!run_case(1000000)) return 1;
    } catch (const std::exception& e){ std::cerr << e.what() << std::endl; return 1; }
    std::cout << "Ultra roundtrip tests: OK" << std::endl;
    return 0;
}
