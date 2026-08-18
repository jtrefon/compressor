# ROADMAP

A concise, sequential roadmap to guide your enhancements and make it easy to pick up where you left off.

**Status legend:** ✅ done · 🔶 partial / experimental · ⬜ not started

---

### 1. Advanced Preprocessing Transforms

* **Burrows–Wheeler Transform (BWT):** ✅ `BwtCompressor` (v2 format: escape →
  BWT → MTF → optional LZ77 → adaptive order-1 arithmetic coding; EOF-sentinel
  based, backward compatible with v1).
* **MTF + RLE:** ✅ MTF is part of the BWT pipeline. RLE exists as a standalone
  strategy (`RleCompressor`) and as an internal stage in `OptimizedCompressor`.

### 2. High-Efficiency Entropy Coding

* **Arithmetic/Range Coding:** ✅ `ArithmeticCompressor` — adaptive order-1
  context model with a Witten–Neal–Cleary 32-bit range coder; used by BWT and
  available as a standalone strategy.
* **Asymmetric Numeral Systems (ANS):** ⬜ offers arithmetic-level compression
  at near-Huffman speeds, ideal for high-throughput use cases. Would replace
  arithmetic coding in the BWT pipeline for speed.

### 3. Statistical Modeling (PPM / Context Mixing)

* **Prediction by Partial Matching (PPM):** ⬜ builds n-order Markov models for
  text and binary data to predict symbols, yielding top text-compression ratios.
* **Context Mixing (PAQ family):** ⬜ combines multiple models with logistic
  mixing to capture diverse patterns, leading benchmarks in lossless
  compression.

### 4. Enhanced Dictionary Methods

* **LZMA (Lempel–Ziv–Markov chain):** ⬜ utilize large (up to 4 GiB)
  dictionaries and range encoding for structured-data redundancy. Note: current
  `Lz77Compressor` supports a 64 KiB window; a multi-MiB window needs a
  different match-finder (hash chains scale poorly past ~1 MiB).
* **BCJ Filters:** ⬜ preprocess machine code by converting relative jumps/
  branches to absolute addresses, boosting LZMA's efficiency on executables.

### 5. Learned Compression Paradigms

* **Neural Autoencoders:** ⬜ train domain-specific models (e.g., image/audio)
  to encode data into compact latent representations.
* **Hybrid Pipeline with ANS:** ⬜ entropy-encode neural outputs using ANS for
  a practical, high-ratio codec.

---

## Current state (v1.6.0+)

Implemented strategies (all exercised by the CLI and/or benchmark):

`Null` · `RLE` · `Huffman` · `LZ77` · `BWT` (v2) · `Arithmetic` ·
`Ultra` · `Extreme` · `Optimized` (default) · `Parallel` (chunked wrapper)

Reference points:
- Benchmark: `./build/app/compression_benchmark --quick` (text: BWT/Optimized ~26%).
- Roadmap §1 and §2 (arithmetic) are complete; §2 (ANS), §3, §4, §5 are open.

[1]: https://www.geeksforgeeks.org/dsa/burrows-wheeler-data-transform-algorithm/?utm_source=chatgpt.com "Burrows - Wheeler Data Transform Algorithm - GeeksforGeeks"
[2]: https://stackoverflow.com/questions/14026952/efficient-to-apply-run-length-transform-after-move-to-front-transform-and-bwt?utm_source=chatgpt.com "Efficient to apply Run-Length Transform after Move to Front ..."
[3]: https://www.reddit.com/r/algorithms/comments/sab3a8/better_encoding_than_huffman_coding/?utm_source=chatgpt.com "Better encoding than Huffman coding? : r/algorithms - Reddit"
[4]: https://kedartatwawadi.github.io/post--ANS/?utm_source=chatgpt.com "Understanding the ANS Compressor - Kedar Tatwawadi"
[5]: https://compressions.sourceforge.net/PPM.html?utm_source=chatgpt.com "PPM - Prediction by Partial Matching."
[6]: https://en.wikipedia.org/wiki/Context_mixing?utm_source=chatgpt.com "Context mixing - Wikipedia"
[7]: https://en.wikipedia.org/wiki/LZMA?utm_source=chatgpt.com "LZMA (algorithm) - Wikipedia"
[8]: https://en.wikipedia.org/wiki/BCJ_%28algorithm%29?utm_source=chatgpt.com "BCJ (algorithm) - Wikipedia"
[9]: https://medium.com/%40bredelet/understanding-ans-coding-through-examples-d1bebfc7e076?utm_source=chatgpt.com "Understanding ANS coding through examples | by Denis Bredelet"
[10]: https://bjlkeng.github.io/posts/lossless-compression-with-asymmetric-numeral-systems/?utm_source=chatgpt.com "Lossless Compression with Asymmetric Numeral Systems"
