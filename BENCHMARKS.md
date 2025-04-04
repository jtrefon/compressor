# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 4.040 | 4.098 |
| RLE | 12609154 | 194.33 | 344.508 | 206.741 |
| Huffman | 3683390 | 56.77 | 5734.766 | 741.776 |
| LZ77 | 3880386 | 59.80 | 10017.157 | 229.582 |
| Deflate | 6488663 | 100.00 | 18361.415 | 182.800 |
