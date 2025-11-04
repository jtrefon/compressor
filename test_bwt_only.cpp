#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <fstream>
#include <cstdint>

// Copy of current BWT forward (prefix-doubling over circular indices)
static std::vector<uint8_t> bwt_transform_circular(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    if (n == 0) return {};

    // T = S + S, sort by finite substrings length n via doubling
    std::vector<uint8_t> T; T.reserve(2*n);
    T.insert(T.end(), data.begin(), data.end());
    T.insert(T.end(), data.begin(), data.end());
    const size_t m = 2*n;

    std::vector<uint32_t> sa(m); std::iota(sa.begin(), sa.end(), 0);
    std::vector<uint32_t> rank(m), new_rank(m);
    for (size_t i=0;i<m;++i) rank[i] = static_cast<uint32_t>(T[i]) + 1u;

    for (size_t k=1; k<n; k<<=1){
        std::sort(sa.begin(), sa.end(), [&](uint32_t a, uint32_t b){
            uint32_t ra=rank[a], rb=rank[b]; if (ra!=rb) return ra<rb;
            uint32_t ra2 = (a+k<m) ? rank[a+k] : 0u;
            uint32_t rb2 = (b+k<m) ? rank[b+k] : 0u;
            return ra2 < rb2;
        });
        new_rank[sa[0]] = 0;
        for (size_t i=1;i<m;++i){
            uint32_t a=sa[i-1], b=sa[i];
            uint32_t ra=rank[a], rb=rank[b];
            uint32_t ra2=(a+k<m)?rank[a+k]:0u, rb2=(b+k<m)?rank[b+k]:0u;
            new_rank[b] = (ra==rb && ra2==rb2) ? new_rank[a] : (new_rank[a]+1);
        }
        rank.swap(new_rank);
        if (rank[sa[m-1]] == m-1) break;
    }

    // Order rotations 0..n-1 by rank (tie-breaker via rank[i+n])
    std::vector<uint32_t> rot_idx(n); std::iota(rot_idx.begin(), rot_idx.end(), 0);
    std::sort(rot_idx.begin(), rot_idx.end(), [&](uint32_t a, uint32_t b){
        if (rank[a] != rank[b]) return rank[a] < rank[b];
        return rank[a+n] < rank[b+n];
    });

    std::vector<uint8_t> last_column(n); uint32_t primary_index=0;
    for (size_t i=0;i<n;++i){
        uint32_t start = rot_idx[i];
        last_column[i] = data[(start + n - 1) % n];
        if (start == 0) primary_index = static_cast<uint32_t>(i);
    }

    std::vector<uint8_t> out; out.reserve(4+n);
    out.push_back(static_cast<uint8_t>((primary_index>>24)&0xFF));
    out.push_back(static_cast<uint8_t>((primary_index>>16)&0xFF));
    out.push_back(static_cast<uint8_t>((primary_index>>8)&0xFF));
    out.push_back(static_cast<uint8_t>(primary_index&0xFF));
    out.insert(out.end(), last_column.begin(), last_column.end());
    return out;
}

// Copy of current inverse BWT (LF-mapping, step-first then emit L[row])
static std::vector<uint8_t> inverse_bwt_circular(const std::vector<uint8_t>& data) {
    if (data.size() < 5) return {};
    size_t pos = 0;
    auto read_u32_be = [&](const std::vector<uint8_t>& src, size_t& p){
        if (p + 4 > src.size()) throw std::out_of_range("read_u32_be: insufficient bytes");
        uint32_t b0 = src[p++], b1 = src[p++], b2 = src[p++], b3 = src[p++];
        return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    };
    uint32_t primary_index = read_u32_be(data, pos);
    const uint8_t* L = data.data() + 4;
    const size_t n = data.size() - 4;
    if (primary_index >= n) throw std::runtime_error("Invalid primary index");

    // Build first column (F) by sorting L
    std::vector<uint8_t> F(L, L + n);
    std::sort(F.begin(), F.end());

    // Count positions of each char in F to compute starts
    std::vector<uint32_t> freq(256, 0);
    for (uint8_t c : F) freq[c]++;
    std::vector<uint32_t> start(256);
    uint32_t s = 0; for (int c = 0; c < 256; ++c){ start[c] = s; s += freq[c]; }

    // Build LF mapping: for each position i in L, LF[i] = start[L[i]] + rank_of_that_occurrence
    std::vector<uint32_t> occ(256, 0), LF(n);
    for (size_t i = 0; i < n; ++i) { uint8_t c = L[i]; LF[i] = start[c] + occ[c]++; }

    std::vector<uint8_t> out(n);
    uint32_t row = primary_index;
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) { out[static_cast<size_t>(i)] = L[row]; row = LF[row]; }
    return out;
}

static std::vector<uint8_t> read_prefix(const std::string& path, size_t max_bytes){
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open file");
    std::vector<uint8_t> buf(max_bytes);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(max_bytes));
    buf.resize(static_cast<size_t>(f.gcount()));
    return buf;
}

static void run_case(size_t n){
    auto data = read_prefix("../data/test.txt", n);
    std::cout << "\n[BWT-ONLY] testing size=" << data.size() << std::endl;
    auto t = bwt_transform_circular(data);
    auto r = inverse_bwt_circular(t);
    if (r == data) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "MISMATCH" << std::endl;
        // show first mismatch
        size_t i=0; for (; i<r.size() && i<data.size(); ++i){ if (r[i]!=data[i]) break; }
        std::cout << "first mismatch at " << i << " exp=" << (int)data[i] << " got=" << (int)r[i] << std::endl;
    }
}

int main(){
    try{
        run_case(1000);
        run_case(10000);
        run_case(50000);
        run_case(100000);
        run_case(200000);
        run_case(1000000);
        run_case(6000000);
    } catch (const std::exception& e){
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
