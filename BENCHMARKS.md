# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 4.191 | 4.355 |
| RLE | 12609154 | 194.33 | 447.762 | 267.331 |
| Huffman | 3683390 | 56.77 | 7691.463 | 1013.340 |
| LZ77 | 5024444 | 77.43 | 23243.973 | 270.112 |
| Deflate | 5024444 | 77.43 | 23083.178 | 271.225 |
