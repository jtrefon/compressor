#include <compression/UltraCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <numeric>

// Copy raw BWT forward/inverse identical to UltraCompressor helpers
static std::vector<uint8_t> raw_bwt_forward(const std::vector<uint8_t>& data){
    const size_t n = data.size(); if (n==0) return {};
    std::vector<uint8_t> T; T.reserve(2*n);
    T.insert(T.end(), data.begin(), data.end());
    T.insert(T.end(), data.begin(), data.end());
    const size_t m = 2*n;
    std::vector<uint32_t> sa(m); std::iota(sa.begin(), sa.end(), 0);
    std::vector<uint32_t> rank(m), new_rank(m);
    for(size_t i=0;i<m;++i) rank[i] = static_cast<uint32_t>(T[i]) + 1u;
    for(size_t k=1;k<n;k<<=1){
        std::sort(sa.begin(), sa.end(), [&](uint32_t a, uint32_t b){
            uint32_t ra=rank[a], rb=rank[b]; if(ra!=rb) return ra<rb;
            uint32_t ra2=(a+k<m)?rank[a+k]:0u, rb2=(b+k<m)?rank[b+k]:0u; return ra2<rb2;});
        new_rank[sa[0]] = 0;
        for(size_t i=1;i<m;++i){ uint32_t a=sa[i-1], b=sa[i]; uint32_t ra=rank[a], rb=rank[b];
            uint32_t ra2=(a+k<m)?rank[a+k]:0u, rb2=(b+k<m)?rank[b+k]:0u;
            new_rank[b] = (ra==rb && ra2==rb2) ? new_rank[a] : (new_rank[a]+1); }
        rank.swap(new_rank);
        if (rank[sa[m-1]] == m-1) break;
    }
    std::vector<uint32_t> rot_idx(n); std::iota(rot_idx.begin(), rot_idx.end(), 0);
    std::sort(rot_idx.begin(), rot_idx.end(), [&](uint32_t a, uint32_t b){ if(rank[a]!=rank[b]) return rank[a]<rank[b]; return rank[a+n]<rank[b+n];});
    std::vector<uint8_t> L(n); uint32_t primary=0;
    for(size_t i=0;i<n;++i){ uint32_t s=rot_idx[i]; L[i]=data[(s+n-1)%n]; if(s==0) primary=static_cast<uint32_t>(i);}    
    std::vector<uint8_t> out; out.reserve(4+n);
    out.push_back(static_cast<uint8_t>((primary>>24)&0xFF));
    out.push_back(static_cast<uint8_t>((primary>>16)&0xFF));
    out.push_back(static_cast<uint8_t>((primary>>8)&0xFF));
    out.push_back(static_cast<uint8_t>(primary&0xFF));
    out.insert(out.end(), L.begin(), L.end());
    return out;
}

static std::vector<uint8_t> raw_bwt_inverse(const std::vector<uint8_t>& data){
    if (data.size()<5) return {};
    size_t pos=0; auto read_u32=[&](const std::vector<uint8_t>& src, size_t& p){ if(p+4>src.size()) throw std::out_of_range("hdr"); uint32_t b0=src[p++],b1=src[p++],b2=src[p++],b3=src[p++]; return (b0<<24)|(b1<<16)|(b2<<8)|b3; };
    uint32_t primary = read_u32(data,pos);
    const uint8_t* L = data.data()+4; size_t n = data.size()-4;
    std::vector<uint8_t> F(L,L+n); std::sort(F.begin(), F.end());
    std::vector<uint32_t> freq(256,0); for(uint8_t c:F) freq[c]++;
    std::vector<uint32_t> start(256); uint32_t s=0; for(int c=0;c<256;++c){ start[c]=s; s+=freq[c]; }
    std::vector<uint32_t> occ(256,0), LF(n); for(size_t i=0;i<n;++i){ uint8_t c=L[i]; LF[i]=start[c]+occ[c]++; }
    std::vector<uint8_t> out(n); uint32_t row=primary; for(int i=(int)n-1;i>=0;--i){ out[(size_t)i]=L[row]; row=LF[row]; }
    return out;
}

static std::vector<uint8_t> read_prefix(const std::string& path, size_t n){
    std::ifstream f(path, std::ios::binary); if(!f) throw std::runtime_error("cannot open file");
    std::vector<uint8_t> buf(n); f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(n));
    buf.resize(static_cast<size_t>(f.gcount())); return buf;
}

static void print_first_mismatch(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b){
    size_t len = std::min(a.size(), b.size());
    for(size_t i=0;i<len;++i){ if(a[i]!=b[i]){ std::cout<<" first mismatch at "<<i<<" exp="<<(int)a[i]<<" got="<<(int)b[i]<<"\n"; return; } }
    if (a.size()!=b.size()) std::cout<<" size mismatch: a="<<a.size()<<" b="<<b.size()<<"\n";
}

static void run_case(size_t n){
    auto data = read_prefix("../data/test.txt", n);
    std::cout << "\n[ULTRA-STAGES] testing size=" << data.size() << std::endl;

    auto bwt = raw_bwt_forward(data);
    auto rec = raw_bwt_inverse(bwt);
    std::cout << "  BWT-only: " << (rec==data?"OK":"FAIL"); if(rec!=data){ print_first_mismatch(data, rec); return; } std::cout<<"\n";

    compression::Lz77Compressor lz(65536,3,258,false,true,true);
    compression::HuffmanCompressor huf;

    auto lz_c = lz.compress(bwt);
    auto lz_d = lz.decompress(lz_c);
    std::cout << "  LZ77 round-trip on BWT: " << (lz_d==bwt?"OK":"FAIL"); if(lz_d!=bwt){ print_first_mismatch(bwt, lz_d); return; } std::cout<<"\n";

    auto hf_c = huf.compress(lz_c);
    auto hf_d = huf.decompress(hf_c);
    std::cout << "  Huffman round-trip on LZ output: " << (hf_d==lz_c?"OK":"FAIL");
    if(hf_d!=lz_c){
        std::cout << " (hf_c.size=" << hf_c.size() << ")";
        // quick header parse
        if (hf_c.size() >= 3) {
            size_t off=0; uint16_t cnt = (uint16_t)hf_c[off] | ((uint16_t)hf_c[off+1] << 8); off+=2;
            std::cout << " count=" << cnt;
            // skip entries roughly
            for (size_t i=0; i<cnt && off < hf_c.size(); ++i){
                off += 1; // sym
                // varint
                while (off < hf_c.size()) { uint8_t b = hf_c[off++]; if ((b & 0x80) == 0) break; }
            }
            if (off < hf_c.size()) {
                uint8_t lastBits = hf_c[off++];
                size_t payload = hf_c.size() - off;
                std::cout << " lastBits=" << (int)lastBits << " payload=" << payload;
            }
        }
        print_first_mismatch(lz_c, hf_d);
        return;
    }
    std::cout<<"\n";

    // Full chain
    auto full_c = huf.compress(lz_c);
    auto full_d_lz = lz.decompress(huf.decompress(full_c));
    auto final = raw_bwt_inverse(full_d_lz);
    bool ok = (final == data);
    std::cout << "  Full chain: " << (ok?"OK":"FAIL"); if(!ok){ print_first_mismatch(data, final);} std::cout<<"\n";
}

int main(){
    try{
        run_case(1000);
        run_case(10000);
        run_case(100000);
        run_case(1000000);
    } catch (const std::exception& e){ std::cerr << "error: " << e.what() << std::endl; return 1; }
    return 0;
}
