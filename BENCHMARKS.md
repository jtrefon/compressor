# Compression Benchmark Results

This report compares the performance of various compression algorithms on different types of files.

## PNG Image

File size: 3967970 bytes

| Algorithm | Compressed Size (B) | Ratio (%) | Compression Time (ms) | Decompression Time (ms) | Valid |
| --------- | ------------------- | --------- | --------------------- | ----------------------- | ----- |
| Arithmetic | 3967997 | 100.00 | 1904.7610 | 25.6634 | ✓ |
| Huffman | 3968779 | 100.02 | 7894.6658 | 0.0017 | ✗ |
| RLE | 7864748 | 198.21 | 395.4957 | 162.2463 | ✓ |
| LZ77 | 3966921 | 99.97 | 15458.5618 | 254.1383 | ✗ |
| Null (Identity) | 3967970 | 100.00 | 0.6028 | 0.7859 | ✓ |

## Plain Text

File size: 6488663 bytes

| Algorithm | Compressed Size (B) | Ratio (%) | Compression Time (ms) | Decompression Time (ms) | Valid |
| --------- | ------------------- | --------- | --------------------- | ----------------------- | ----- |
| Arithmetic | 0 | 0.00 | 0.0000 | 0.0000 | ✗ |
| Huffman | 3683390 | 56.77 | 8686.6915 | 1072.6107 | ✓ |
| RLE | 12609154 | 194.33 | 410.9831 | 246.9615 | ✓ |
| LZ77 | 3880386 | 59.80 | 16617.8513 | 624.9184 | ✗ |
| Null (Identity) | 6488663 | 100.00 | 1.5626 | 4.6105 | ✓ |

## Executable Binary

File size: 857216 bytes

| Algorithm | Compressed Size (B) | Ratio (%) | Compression Time (ms) | Decompression Time (ms) | Valid |
| --------- | ------------------- | --------- | --------------------- | ----------------------- | ----- |
| Arithmetic | 845191 | 98.60 | 78.0560 | 41.2973 | ✓ |
| Huffman | 618277 | 72.13 | 1752.3422 | 0.0018 | ✗ |
| RLE | 1405616 | 163.97 | 54.7713 | 38.7807 | ✓ |
| LZ77 | 255998 | 29.86 | 1131.7221 | 40.8711 | ✗ |
| Null (Identity) | 857216 | 100.00 | 0.0890 | 0.0526 | ✓ |

## Summary

### Best Compression Ratio by File Type

| File Type | Best Algorithm | Compression Ratio (%) |
| --------- | -------------- | --------------------- |
| PNG Image | Null (Identity) | 100.00 |
| Plain Text | Huffman | 56.77 |
| Executable Binary | Arithmetic | 98.60 |

### Best Compression Speed by File Type

| File Type | Best Algorithm | Compression Time (ms) |
| --------- | -------------- | --------------------- |
| PNG Image | Null (Identity) | 0.6028 |
| Plain Text | Null (Identity) | 1.5626 |
| Executable Binary | Null (Identity) | 0.0890 |

### Compression Algorithm Overall Performance

| Algorithm | Avg. Compression Ratio (%) | Avg. Compression Time (ms) | Reliability |
| --------- | -------------------------- | -------------------------- | ----------- |
| Arithmetic | 99.30 | 991.4085 | 66.7% |
| Huffman | 56.77 | 8686.6915 | 33.3% |
| RLE | 185.50 | 287.0834 | 100.0% |
| LZ77 | 0.00 | 0.0000 | 0.0% |
| Null (Identity) | 100.00 | 0.7515 | 100.0% |

## Conclusion

Based on the benchmark results, here are the key findings:

- Best overall algorithm for compression ratio: **Huffman**
- Best overall algorithm for speed: **Null (Identity)**
- Most reliable algorithm: **RLE** (100% success rate)

This benchmark was conducted on /Users/jacektrefon/Projects/vibing/compression
Generated on: 2025-04-07 23:22:10
