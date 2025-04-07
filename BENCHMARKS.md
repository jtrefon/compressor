# Compression Benchmark Results

## Text File Tests

| Algorithm | Data Type | Original Size (bytes) | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-----------|------------------------|-------------------------|-----------|--------------------|----------------------|
| Null | test.txt | 6488663 | 6488663 | 100.00 | 5.237 | 4.908 |
| RLE | test.txt | 6488663 | 12609154 | 194.33 | 534.094 | 313.392 |
| Huffman | test.txt | 6488663 | 3683390 | 56.77 | 10607.906 | 1416.045 |
| Arithmetic | test.txt | 6488663 | 6488690 | 100.00 | 3060.130 | 46.490 |
| LZ77 | test.txt | 6488663 | 3880386 | 59.80 | 16367.834 | 363.026 |
| Deflate | test.txt | 6488663 | 6488663 | 100.00 | 29915.665 | 279.938 |

## Binary (Image) File Tests

| Algorithm | Data Type | Original Size (bytes) | Compressed Size (bytes) | Ratio (%) | Compress Time (ms) | Decompress Time (ms) |
|-----------|-----------|------------------------|-------------------------|-----------|--------------------|----------------------|
| Null | test.png | 3967970 | 3967970 | 100.00 | 0.784 | 0.830 |
| RLE | test.png | 3967970 | 7864748 | 198.21 | 318.981 | 190.597 |
| Huffman | test.png | 3967970 | 3968779 | 100.02 | 8639.892 | 0.002 |
| Arithmetic | test.png | 3967970 | 3967997 | 100.00 | 3742.613 | 27.481 |
| LZ77 | test.png | 3967970 | 3966921 | 99.97 | 15529.177 | 296.648 |
| Deflate | test.png | 3967970 | 3967970 | 100.00 | 15518.567 | 293.443 |

