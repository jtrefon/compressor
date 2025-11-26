# Compression Benchmark Results

Performance comparison across different file types.


## Text (6.2 MB)

File: `test.txt`  
Size: 6488663 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null (1T) | 6488663 | 100.00 | 0.608 | 0.717 |
| Null (10T) | 6488729 | 100.00 | 17.936 | 1.099 |
| RLE (1T) | 12609154 | 194.33 | 9.333 | 14.124 |
| RLE (10T) | 12609220 | 194.33 | 15.801 | 2.908 |
| Huffman (1T) | 3683391 | 56.77 | 220.942 | 56.900 |
| Huffman (10T) | 3669993 | 56.56 | 50.390 | 11.185 |
| LZ77 (1T) | 3544128 | 54.62 | 503.734 | 10.943 |
| LZ77 (10T) | 3568437 | 54.99 | 131.010 | 2.627 |
| BWT (1T) | 1866158 | 28.76 | 3392.461 | 512.844 |
| BWT (10T) | 1982387 | 30.55 | 353.452 | 100.711 |

## JPEG Image (2.3 MB)

File: `faizur-rehman-xqh-RlfJVx4-unsplash.jpg`  
Size: 2372325 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null (1T) | 2372325 | 100.00 | 0.058 | 0.064 |
| Null (10T) | 2372391 | 100.00 | 6.077 | 0.362 |
| RLE (1T) | 4717886 | 198.87 | 2.436 | 4.602 |
| RLE (10T) | 4717952 | 198.87 | 6.217 | 1.542 |
| Huffman (1T) | 2371022 | 99.95 | 265.511 | 83.588 |
| Huffman (10T) | 2370562 | 99.93 | 47.094 | 20.494 |
| LZ77 (1T) | 2666803 | 112.41 | 508.573 | 3.630 |
| LZ77 (10T) | 2666907 | 112.42 | 173.926 | 1.132 |
| BWT (1T) | 2381402 | 100.38 | 871.651 | 261.076 |
| BWT (10T) | 2383102 | 100.45 | 138.280 | 29.727 |

## WAV Audio (9.4 MB)

File: `835222__silverillusionist__ascendancy-music-sample.wav`  
Size: 9878444 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null (1T) | 9878444 | 100.00 | 0.458 | 1.974 |
| Null (10T) | 9878510 | 100.00 | 29.320 | 1.965 |
| RLE (1T) | 19679564 | 199.22 | 22.235 | 25.003 |
| RLE (10T) | 19679632 | 199.22 | 41.318 | 9.723 |
| Huffman (1T) | 9255312 | 93.69 | 858.650 | 317.316 |
| Huffman (10T) | 9180813 | 92.94 | 302.646 | 63.243 |
| LZ77 (1T) | 11112881 | 112.50 | 2095.775 | 9.096 |
| LZ77 (10T) | 11112987 | 112.50 | 512.841 | 2.870 |
| BWT (1T) | 9727879 | 98.48 | 3789.652 | 1417.477 |
| BWT (10T) | 9748001 | 98.68 | 445.290 | 180.895 |

## Summary

**Observations:**

- **Text files**: Highly compressible with Huffman, LZ77, and BWT algorithms
- **JPEG images**: Already compressed, minimal improvement possible
- **WAV audio**: Moderate compressibility, larger file size tests throughput

**Hardware**: 10 threads available
