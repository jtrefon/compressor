# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 4.603 | 5.451 |
| RLE | 12609154 | 194.33 | 414.233 | 239.658 |
| Huffman | 3683390 | 56.77 | 8679.869 | 1298.883 |
| LZ77 | 3880386 | 59.80 | 15102.900 | 320.356 |
| Deflate | 6488663 | 100.00 | 26525.272 | 248.277 |
| BWT | 3265706 | 50.33 | 18573.781 | 7097.959 |
