# Compression Benchmark Results

This report compares the performance of various compression algorithms on different types of files.

## PNG Image

File size: 3967970 bytes

| Algorithm | Compressed Size (B) | Ratio (%) | Compression Time (ms) | Decompression Time (ms) | Valid |
| --------- | ------------------- | --------- | --------------------- | ----------------------- | ----- |
| Arithmetic | 3967997 | 100.00 | 1952.5866 | 24.9825 | ✓ |
| Huffman | 3968779 | 100.02 | 7258.2826 | 0.0022 | ✗ |
| RLE | 7864748 | 198.21 | 273.6535 | 164.4672 | ✓ |
| LZ77 | 3966921 | 99.97 | 14105.7070 | 313.4386 | ✗ |
| Null (Identity) | 3967970 | 100.00 | 0.3969 | 0.5219 | ✓ |

## Plain Text

File size: 6488663 bytes

| Algorithm | Compressed Size (B) | Ratio (%) | Compression Time (ms) | Decompression Time (ms) | Valid |
| --------- | ------------------- | --------- | --------------------- | ----------------------- | ----- |
| Arithmetic | 0 | 0.00 | 0.0000 | 0.0000 | ✗ |
| Huffman | 3683390 | 56.77 | 8556.9490 | 1323.2195 | ✓ |
| RLE | 12609154 | 194.33 | 453.9883 | 264.1599 | ✓ |
| LZ77 | 3880386 | 59.80 | 14434.6939 | 304.7223 | ✗ |
| Null (Identity) | 6488663 | 100.00 | 1.0290 | 3.8854 | ✓ |

## Executable Binary

File size: 857216 bytes

| Algorithm | Compressed Size (B) | Ratio (%) | Compression Time (ms) | Decompression Time (ms) | Valid |
| --------- | ------------------- | --------- | --------------------- | ----------------------- | ----- |
| Arithmetic | 845191 | 98.60 | 71.0258 | 40.7189 | ✓ |
| Huffman | 618277 | 72.13 | 1276.0864 | 0.0019 | ✗ |
| RLE | 1405616 | 163.97 | 49.3325 | 33.1628 | ✓ |
| LZ77 | 255998 | 29.86 | 942.5789 | 44.5349 | ✗ |
| Null (Identity) | 857216 | 100.00 | 0.0937 | 0.0581 | ✓ |

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
| PNG Image | Null (Identity) | 0.3969 |
| Plain Text | Null (Identity) | 1.0290 |
| Executable Binary | Null (Identity) | 0.0937 |

### Compression Algorithm Overall Performance

| Algorithm | Avg. Compression Ratio (%) | Avg. Compression Time (ms) | Reliability |
| --------- | -------------------------- | -------------------------- | ----------- |
| Arithmetic | 99.30 | 1011.8062 | 66.7% |
| Huffman | 56.77 | 8556.9490 | 33.3% |
| RLE | 185.50 | 258.9914 | 100.0% |
| LZ77 | 0.00 | 0.0000 | 0.0% |
| Null (Identity) | 100.00 | 0.5065 | 100.0% |

## Conclusion

Based on the benchmark results, here are the key findings:

- Best overall algorithm for compression ratio: **Huffman**
- Best overall algorithm for speed: **Null (Identity)**
- Most reliable algorithm: **RLE** (100% success rate)

This benchmark was conducted on /Users/jacektrefon/Projects/vibing/compression
Generated on: 2025-04-07 23:14:41
