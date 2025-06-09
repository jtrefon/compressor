# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 3.646 | 3.170 |
| RLE | 12609154 | 194.33 | 175.708 | 113.299 |
| Huffman | 3683390 | 56.77 | 7322.329 | 2093.496 |
| LZ77 | 3880386 | 59.80 | 10166.130 | 127.257 |
| Deflate | 6488663 | 100.00 | 18469.067 | 98.895 |
| BWT | 3258056 | 50.21 | 40326.751 | 6370.723 |
