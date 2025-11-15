#include <compression/UltraCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

static std::vector<uint8_t> read_prefix(const std::string& path, size_t n){
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open file");
    std::vector<uint8_t> buf(n);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(n));
    buf.resize(static_cast<size_t>(f.gcount()));
    return buf;
}

static void run_case(size_t n){
    auto data = read_prefix("../data/test.txt", n);
    std::cout << "\n[ULTRA] testing size=" << data.size() << std::endl;
    compression::UltraCompressor ultra;
    auto c = ultra.compress(data);
    auto d = ultra.decompress(c);
    if (d == data){
        std::cout << "OK (c=" << c.size() << ")" << std::endl;
    } else {
        std::cout << "MISMATCH" << std::endl;
        size_t i=0; for(; i<d.size() && i<data.size(); ++i){ if (d[i]!=data[i]) break;}
        std::cout << "first mismatch at " << i << " exp=" << (int)data[i] << " got=" << (int)d[i] << std::endl;
    }
}

int main(){
    try{
        run_case(1000);
        run_case(10000);
        run_case(100000);
        run_case(1000000);
        run_case(6000000);
    } catch (const std::exception& e){
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
