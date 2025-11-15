#include <compression/HuffmanCompressor.hpp>
#include <iostream>
#include <vector>
#include <cstdint>
#include <fstream>

static std::vector<uint8_t> read_prefix(const std::string& path, size_t n){
    std::ifstream f(path, std::ios::binary); if(!f) throw std::runtime_error("open");
    std::vector<uint8_t> buf(n); f.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)n);
    buf.resize((size_t)f.gcount()); return buf;
}

static void analyze(const std::vector<uint8_t>& buf){
    if (buf.size() < 2) { std::cout<<"too small"<<std::endl; return; }
    size_t off=0;
    uint16_t count = (uint16_t)buf[off] | ((uint16_t)buf[off+1] << 8); off += 2;
    std::cout << "count="<<count<<"\n";
    for (size_t i=0; i<count; ++i){
        if (off >= buf.size()) { std::cout<<"eof in symbol at i="<<i<<" off="<<off<<"\n"; return; }
        uint8_t sym = buf[off++];
        uint64_t freq=0; uint8_t shift=0; uint8_t b=0;
        size_t bytes=0;
        do {
            if (off >= buf.size()) { std::cout<<"eof in varint at i="<<i<<"\n"; return; }
            b = buf[off++];
            freq |= (uint64_t)(b & 0x7F) << shift;
            shift += 7; bytes++;
            if (shift > 63 && (b & 0x80)) { std::cout<<"varint overflow at i="<<i<<"\n"; return; }
        } while (b & 0x80);
        if (i<5) std::cout<<"  entry"<<i<<": sym="<<(int)sym<<" freq="<<freq<<" bytes="<<bytes<<"\n";
    }
    if (off >= buf.size()) { std::cout<<"no lastByteBits, off="<<off<<" size="<<buf.size()<<"\n"; return; }
    uint8_t lastBits = buf[off++];
    size_t dataByteCount = buf.size() - off;
    std::cout<<"lastBits="<<(int)lastBits<<" payloadBytes="<<dataByteCount<<"\n";
}

#include <compression/Lz77Compressor.hpp>

int main(){
    try{
        auto data = read_prefix("../data/test.txt", 100000);
        compression::Lz77Compressor lz(65536,3,258,false,true,true);
        auto lz_c = lz.compress(data);
        compression::HuffmanCompressor h;
        auto hf_c = h.compress(lz_c);
        std::cout<<"hf_c.size="<<hf_c.size()<<" lz_c.size="<<lz_c.size()<<"\n";
        analyze(hf_c);
        auto hf_d = h.decompress(hf_c);
        std::cout<<"hf_d.size="<<hf_d.size()<<" (expect "<<lz_c.size()<<")\n";
    } catch (const std::exception& e){ std::cerr<<"error: "<<e.what()<<std::endl; return 1; }
    return 0;
}
