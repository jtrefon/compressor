# Compression Benchmark Results

Benchmarked against `data/test.txt` (Size: 6488663 bytes)

## Default (multi-threaded)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488709 | 100.00 | 119.856 | 9.622 |
| RLE | 12609200 | 194.33 | 120.805 | 42.124 |
| Huffman | 3676028 | 56.65 | 2503.122 | 688.305 |
| LZ77 | 3888885 | 59.93 | 3325.915 | 36.959 |
| BWT | 3260426 | 50.25 | 4254.139 | 1710.900 |

## Single-thread (--no-threads)

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null | 6488663 | 100.00 | 7.237 | 40.349 |
| RLE | 12609154 | 194.33 | 214.948 | 154.491 |
| Huffman | 3683390 | 56.77 | 9735.439 | 2878.336 |
| LZ77 | 3880386 | 59.80 | 13418.045 | 149.783 |
| BWT | 3258056 | 50.21 | 19129.290 | 7787.315 |
