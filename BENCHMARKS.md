# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 3.594 | 4.382 |
| RLE | 12609154 | 194.33 | 155.278 | 98.631 |
| Huffman | 3683390 | 56.77 | 6950.433 | 2060.716 |
| LZ77 | 3880386 | 59.80 | 9461.179 | 109.380 |
| Deflate | 6488663 | 100.00 | 17357.154 | 83.011 |
| BWT | 3257391 | 50.20 | 21323.903 | inf |
