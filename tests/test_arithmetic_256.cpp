#include <compression/ArithmeticCompressor.hpp>
#include <vector>
#include <iostream>

int main(){
    // Create data with all 256 symbols, repeated twice to make the stream non-trivial
    std::vector<uint8_t> data;
    data.reserve(512);
    for (int r = 0; r < 2; ++r) {
        for (int i = 0; i < 256; ++i) data.push_back(static_cast<uint8_t>(i));
    }

    compression::ArithmeticCompressor ac;
    auto c = ac.compress(data);
    auto d = ac.decompress(c);

    if (d != data) {
        std::cerr << "Arithmetic 256-symbol roundtrip mismatch" << std::endl;
        return 1;
    }
    std::cout << "Arithmetic 256-symbol roundtrip: OK" << std::endl;
    return 0;
}
