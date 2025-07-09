# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488729 | 100.00 | 20.513 | 3.576 |
| RLE | 12609220 | 194.33 | 21.600 | 5.750 |
| Huffman | 3669983 | 56.56 | 51.402 | 11.239 |
| LZ77 | 3898807 | 60.09 | 127.365 | 3.159 |
| BWT | 2345138 | 36.14 | 208.860 | 49.839 |
