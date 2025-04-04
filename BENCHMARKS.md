# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 4.098 | 4.005 |
| RLE | 12609154 | 194.33 | 449.528 | 256.764 |
| Huffman | 3683390 | 56.77 | 7066.599 | 892.982 |
| LZ77 | 5390314 | 83.07 | 12300.744 | 256.660 |
| Deflate | 5390314 | 83.07 | 11837.539 | 239.609 |
