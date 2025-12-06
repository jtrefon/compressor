# Compression Benchmark Results

Performance comparison across different file types.


## Text (6.2 MB)

File: `test.txt`  
Size: 6488663 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
<<<<<<< HEAD
| Null (1T) | 6488663 | 100.00 | 0.465 | 0.494 |
| RLE (1T) | 12609154 | 194.33 | 10.140 | 14.814 |
| Huffman (1T) | 3683391 | 56.77 | 194.846 | 52.358 |
| LZ77 (1T) | 3544128 | 54.62 | 435.094 | 9.535 |
| BWT (1T) | 1866158 | 28.76 | 2434.741 | 357.632 |
| Optimized (1T) | 1866159 | 28.76 | 4664.465 | 354.060 |

## JPEG Image (2.3 MB)

File: `faizur-rehman-xqh-RlfJVx4-unsplash.jpg`  
Size: 2372325 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null (1T) | 2372325 | 100.00 | 0.083 | 0.207 |
| RLE (1T) | 4717886 | 198.87 | 2.010 | 4.052 |
| Huffman (1T) | 2371022 | 99.95 | 198.806 | 79.394 |
| LZ77 (1T) | 2666803 | 112.41 | 172.990 | 2.242 |
| BWT (1T) | 2381402 | 100.38 | 509.227 | 150.034 |
| Optimized (1T) | 2372326 | 100.00 | 3283.666 | 0.112 |

## WAV Audio (9.4 MB)

File: `835222__silverillusionist__ascendancy-music-sample.wav`  
Size: 9878444 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null (1T) | 9878444 | 100.00 | 0.783 | 1.186 |
| RLE (1T) | 19679564 | 199.22 | 10.635 | 17.426 |
| Huffman (1T) | 9255312 | 93.69 | 671.568 | 235.365 |
| LZ77 (1T) | 11112881 | 112.50 | 757.514 | 9.034 |
| BWT (1T) | 9727879 | 98.48 | 3174.127 | 1107.771 |
| Optimized (1T) | 9319607 | 94.34 | 18567.681 | 519.679 |

## Source Tree (C++ src)

File: `src`  
Size: 139526 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null (1T) | 139526 | 100.00 | 0.003 | 0.003 |
| RLE (1T) | 234972 | 168.41 | 0.170 | 0.282 |
| Huffman (1T) | 87128 | 62.45 | 4.218 | 1.050 |
| LZ77 (1T) | 42093 | 30.17 | 6.776 | 0.159 |
| BWT (1T) | 27633 | 19.80 | 24.119 | 4.277 |
| Optimized (1T) | 27634 | 19.81 | 63.822 | 4.520 |

## Binary (Executable)

File: `compression_benchmark`  
Size: 102104 bytes

| Algorithm | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-------------------------|-----------|--------------------|----------------------|
| Null (1T) | 102104 | 100.00 | 0.002 | 0.002 |
| RLE (1T) | 97578 | 95.57 | 0.081 | 0.144 |
| Huffman (1T) | 53002 | 51.91 | 3.132 | 0.845 |
| LZ77 (1T) | 31700 | 31.05 | 4.098 | 0.086 |
| BWT (1T) | 25360 | 24.84 | 45.265 | 2.733 |
| Optimized (1T) | 25361 | 24.84 | 71.288 | 2.848 |

## Summary

**Observations:**

- **Text files**: Highly compressible with Huffman, LZ77, and BWT algorithms
- **JPEG images**: Already compressed, minimal improvement possible
- **WAV audio**: Moderate compressibility, larger file size tests throughput

**Hardware**: 10 threads available
=======
| Null (1T) | 6488663 | 100.00 | 0.441 | 0.514 |
| Null (10T) | 6488729 | 100.00 | 14.475 | 0.566 |
| RLE (1T) | 12609154 | 194.33 | 8.936 | 12.843 |
| RLE (10T) | 12609220 | 194.33 | 16.130 | 3.513 |
| Huffman (1T) | 3683391 | 56.77 | 297.235 | 58.790 |
| Huffman (10T) | 3669993 | 56.56 | 51.502 | 12.050 |
| LZ77 (1T) | 3544128 | 54.62 | 488.027 | 10.774 |
| LZ77 (10T) | 3568437 | 54.99 | 123.353 | 2.375 |
| BWT (1T) | 1743174 | 26.86 | 2525.441 | 416.343 |
| BWT (10T) | 1918238 | 29.56 | 246.717 | 47.005 |
| Arithmetic (1T) | 2919673 | 45.00 | 81.501 | 1299.880 |
| Arithmetic (10T) | 2919673 | 45.00 | 81.831 | 1342.193 |

