# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 3.296 | 3.379 |
| RLE | 12609154 | 194.33 | 170.520 | 118.498 |
| Huffman | 3683390 | 56.77 | 7233.466 | 2092.647 |
| LZ77 | 3880386 | 59.80 | 9741.348 | 100.526 |
| Deflate | 6488663 | 100.00 | 17525.106 | 97.005 |
| BWT | 3258057 | 50.21 | 38447.123 | 6162.093 |
