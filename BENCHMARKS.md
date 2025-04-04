# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 3.809 | 4.005 |
| RLE | 12609154 | 194.33 | 345.704 | 208.708 |
| Huffman | 3683390 | 56.77 | 5877.641 | 766.714 |
| LZ77 | 3880386 | 59.80 | 10274.157 | 231.243 |
| Deflate | 6488663 | 100.00 | 19356.713 | 238.507 |
