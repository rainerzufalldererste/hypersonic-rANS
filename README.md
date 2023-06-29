  <a href="https://github.com/rainerzufalldererste/hypersonic-rANS"><img src="https://raw.githubusercontent.com/rainerzufalldererste/hypersonic-rANS/master/docs/logo.png" alt="hypersonic rANS logo" style="width: 533pt; max-width: 100%; margin: 50pt auto;"></a>
  <br>

#### This is my development repo for high-performance rANS codecs. I don't consider them finalized yet, so the repo is eventually going to be made more friendly towards integration with other applications, but for now, the main focus is RnD.

## Facts
- Written in C++.
- Contains many different variants with specific vectorized codepaths (mainly for AVX2 and AVX-512).
- Currently mainly optimized for **decoding throughput**.
- Licensed under Two Clause BSD.

## Benchmarks
**See [Full Benchmark with Graphs](https://rainerzufalldererste.github.io/hypersonic-rANS/).**

  <a href="https://rainerzufalldererste.github.io/hypersonic-rANS/"><img src="https://raw.githubusercontent.com/rainerzufalldererste/hypersonic-rANS/master/docs/screenshot.png" alt="hypersonic rANS pareto graph screenshot" style="width: 100%;"></a>
  <br>

- Single-Threaded
- Running on an `AMD Ryzen 9 7950X`, `32 GB DDR5-6000 CL30` on `Windows 11` via `WSL2`
- Compiled with `clang++`
- Compared to `htscodecs`, `fse`, `fse Huffman`, `Fast HF`, `Fast AC` (Jun 2023 via `TurboBench`, MB/s converted to MiB/s)
- Notable decompressors selected (the benchmark section would be incredibly long otherwise...)
- hypersonic-rANS codecs **highlighted**
- Sorted by decode throughput.

### [enwik8](http://mattmahoney.net/dc/textdata.html) (Wikipedia Extract, 100,000,000 Bytes)

| Codec Type | Ratio | Clocks/Byte | Throughput | Clocks/Byte | Throughput |
| -- | --: | --: | --: | --: | --: |
| **rANS32x64 16w        10** |  65.59 % |   12.54 clk/byte |   341.55 MiB/s |   1.53 clk/byte |  2791.45 MiB/s |
| **rANS32x64 16w        11** |  64.33 % |   12.36 clk/byte |   346.52 MiB/s |   1.54 clk/byte |  2785.87 MiB/s |
| **rANS32x64 16w        12** |  63.81 % |   12.54 clk/byte |   341.71 MiB/s |   1.55 clk/byte |  2755.96 MiB/s |
| **rANS32x32 16w        10** |  65.59 % |   13.08 clk/byte |   327.53 MiB/s |   1.58 clk/byte |  2716.91 MiB/s |
| **rANS32x32 16w        11** |  64.33 % |   12.88 clk/byte |   332.64 MiB/s |   1.58 clk/byte |  2710.09 MiB/s |
| **rANS32x32 16w        12** |  63.81 % |   12.47 clk/byte |   343.55 MiB/s |   1.59 clk/byte |  2686.87 MiB/s |
| fsehuf                      |  63.4 %  |   -              |  1581.32 MiB/s |  -              |  2515.23 MiB/s |
| rans32avx2 0                |  63.5 %  |   -              |  1041.93 MiB/s |  -              |  2374.04 MiB/s |
| **rANS32x32 32blk 16w  12** |  63.81 % |   12.62 clk/byte |   339.50 MiB/s |   1.85 clk/byte |  2312.10 MiB/s |
| **rANS32x32 32blk 16w  11** |  64.33 % |   12.67 clk/byte |   338.00 MiB/s |   1.86 clk/byte |  2299.31 MiB/s |
| **rANS32x32 32blk 16w  10** |  65.59 % |   12.91 clk/byte |   331.80 MiB/s |   1.87 clk/byte |  2289.10 MiB/s |
| rans32avx512 0              |  63.5 %  |   -              |   796.70 MiB/s |  -              |  2221.93 MiB/s |
| **rANS32x32 32blk 8w   11** |  64.33 % |   15.01 clk/byte |   285.45 MiB/s |   2.15 clk/byte |  1988.10 MiB/s |
| **rANS32x32 32blk 8w   12** |  63.82 % |   15.15 clk/byte |   282.80 MiB/s |   2.16 clk/byte |  1984.68 MiB/s |
| **rANS32x32 32blk 8w   10** |  65.60 % |   14.70 clk/byte |   291.41 MiB/s |   2.17 clk/byte |  1977.26 MiB/s |
| rans32sse 0                 |  63.5 %  |   -              |   732.08 MiB/s |  -              |  1948.66 MiB/s |
| **rANS32x32 16w        13** |  63.61 % |   12.93 clk/byte |   331.37 MiB/s |   2.40 clk/byte |  1781.65 MiB/s |
| **rANS32x32 16w        14** |  63.55 % |   12.76 clk/byte |   335.79 MiB/s |   2.42 clk/byte |  1770.66 MiB/s |
| **rANS32x64 16w        13** |  63.61 % |   12.30 clk/byte |   348.13 MiB/s |   2.47 clk/byte |  1731.22 MiB/s |
| **rANS32x64 16w        14** |  63.55 % |   12.59 clk/byte |   340.14 MiB/s |   2.48 clk/byte |  1726.67 MiB/s |
| **rANS32x16 16w        10** |  65.59 % |   13.26 clk/byte |   323.07 MiB/s |   2.54 clk/byte |  1684.80 MiB/s |
| **rANS32x16 16w        12** |  63.81 % |   13.21 clk/byte |   324.24 MiB/s |   2.55 clk/byte |  1681.73 MiB/s |
| **rANS32x16 16w        11** |  64.33 % |   13.25 clk/byte |   323.17 MiB/s |   2.55 clk/byte |  1676.41 MiB/s |
| **rANS32x64 16w        15** |  63.57 % |   12.22 clk/byte |   350.49 MiB/s |   2.56 clk/byte |  1674.05 MiB/s |
| **rANS32x32 16w        15** |  63.57 % |   12.50 clk/byte |   342.60 MiB/s |   2.56 clk/byte |  1670.92 MiB/s |
| **rANS32x32 32blk 16w  14** |  63.55 % |   13.02 clk/byte |   329.08 MiB/s |   2.66 clk/byte |  1607.26 MiB/s |
| **rANS32x32 32blk 16w  13** |  63.61 % |   12.56 clk/byte |   341.16 MiB/s |   2.71 clk/byte |  1582.28 MiB/s |
| **rANS32x32 32blk 16w  15** |  63.57 % |   13.21 clk/byte |   324.33 MiB/s |   2.76 clk/byte |  1550.93 MiB/s |
| **rANS32x32 32blk 8w   13** |  63.60 % |   15.07 clk/byte |   284.24 MiB/s |   2.98 clk/byte |  1438.01 MiB/s |
| **rANS32x32 32blk 8w   14** |  63.53 % |   15.06 clk/byte |   284.45 MiB/s |   3.00 clk/byte |  1429.24 MiB/s |
| **rANS32x32 32blk 8w   15** |  63.51 % |   15.11 clk/byte |   283.41 MiB/s |   3.10 clk/byte |  1381.63 MiB/s |
| **rANS32x16 16w        13** |  63.61 % |   13.14 clk/byte |   325.92 MiB/s |   3.60 clk/byte |  1190.23 MiB/s |
| **rANS32x16 16w        14** |  63.55 % |   13.37 clk/byte |   320.41 MiB/s |   3.64 clk/byte |  1175.92 MiB/s |
| **rANS32x16 16w        15** |  63.57 % |   13.28 clk/byte |   322.51 MiB/s |   4.21 clk/byte |  1017.12 MiB/s |
| fse                         |  63.2 %  |   -              |   736.10 MiB/s |  -              |   966.58 MiB/s |
| rans32avx512 1              |  51.6 %  |   -              |   168.22 MiB/s |  -              |   322.22 MiB/s |
| rans32avx2 1                |  51.6 %  |   -              |   177.36 MiB/s |  -              |   319.15 MiB/s |
| FastHF                      |  63.6 %  |   -              |   189.84 MiB/s |  -              |   151.62 MiB/s |
| FastAC                      |  63.2 %  |   -              |   223.06 MiB/s |  -              |    84.37 MiB/s |
| arith_dyn 1                 |  47.8 %  |   -              |    89.60 MiB/s |  -              |    81.63 MiB/s |
| arith_dyn 0                 |  62.0 %  |   -              |    88.09 MiB/s |  -              |    75.05 MiB/s |

## License
Two Clause BSD
