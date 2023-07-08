  <a href="https://github.com/rainerzufalldererste/hypersonic-rANS"><img src="https://raw.githubusercontent.com/rainerzufalldererste/hypersonic-rANS/master/docs/logo.png" alt="hypersonic rANS logo" style="width: 533pt; max-width: 100%; margin: 50pt auto;"></a>
  <br>

#### This is my development repo for high-performance rANS codecs. I don't consider them finalized yet, so the repo is eventually going to be made more friendly towards integration with other applications, but for now, the main focus is RnD.

## Facts
- Written in C++.
- Contains many different variants with specific vectorized codepaths (mainly for AVX2 and AVX-512).
- Fastest decoders can reach a throughput of up to 3.3 GB/s.
- Currently mainly optimized for **decoding throughput**.
- Licensed under Two Clause BSD.

## Benchmarks
**See [Full Benchmark with Graphs](https://rainerzufalldererste.github.io/hypersonic-rANS/).**

  <a href="https://rainerzufalldererste.github.io/hypersonic-rANS/"><img src="https://raw.githubusercontent.com/rainerzufalldererste/hypersonic-rANS/master/docs/screenshot.png" alt="hypersonic rANS pareto graph screenshot" style="width: 100%;"></a>
  <br>

- Single-Threaded
- Running on an `AMD Ryzen 9 7950X`, `32 GB DDR5-6000 CL30` on `Windows 11` via `WSL2`
- Compiled with `clang++`
- Compared to `htscodecs`, `fse`, `fse Huffman`, `Fast HF`, `Fast AC`, `TurboANX` (Jun 2023 via `TurboBench`, MB/s converted to MiB/s) (`TurboANX` uses a native Windows build from [powturbo](https://github.com/powturbo))
- Notable decompressors selected (the benchmark section would be incredibly long otherwise...)
- hypersonic-rANS codecs **highlighted**
- Sorted by decode throughput.
- All encoders are currently scalar (and not particularly optimized)
- Every best performing decoder variant requires AVX2. (The AVX-512 variants for 32x64 can be faster in rare circumstances, but they weren't in this benchmark)

### [enwik8](http://mattmahoney.net/dc/textdata.html) (Wikipedia Extract, 100,000,000 Bytes)
| Codec Type | Open-<br/>Source | Ratio | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | :-: | --: | --: | --: | --: |
| **rANS32x64 16w 11 (raw)**                | ✔️ |  64.48 % |   336.81 MiB/s |   1.42 clk/byte |  3,018.02 MiB/s |
| **rANS32x64 16w 10 (raw)**                | ✔️ |  65.97 % |   335.28 MiB/s |   1.42 clk/byte |  3,013.45 MiB/s |
| **rANS32x64 16w 12 (raw)**                | ✔️ |  63.83 % |   347.90 MiB/s |   1.42 clk/byte |  3,009.18 MiB/s |
| TurboANX 63                               | ❌ |  63.4 %  |   981.79 MiB/s |  -              |  2,964.02 MiB/s |
| **rANS32x64 16w 10**                      | ✔️ |  65.56 % |   239.77 MiB/s |   1.46 clk/byte |  2,934.64 MiB/s |
| TurboANX 48                               | ❌ |  63.3 %  |   969.72 MiB/s |  -              |  2,917.59 MiB/s |
| **rANS32x64 16w 11**                      | ✔️ |  64.30 % |   225.35 MiB/s |   1.47 clk/byte |  2,907.73 MiB/s |
| TurboANX 40                               | ❌ |  63.2 %  |   964.45 MiB/s |  -              |  2,883.45 MiB/s |
| **rANS32x64 16w 12**                      | ✔️ |  63.73 % |   230.37 MiB/s |   1.50 clk/byte |  2,856.76 MiB/s |
| TurboANX 32                               | ❌ |  66.4 %  |   951.53 MiB/s |  -              |  2,856.26 MiB/s |
| **rANS32x32 16w 10 (raw)**                | ✔️ |  65.97 % |   328.77 MiB/s |   1.52 clk/byte |  2,822.60 MiB/s |
| **rANS32x32 16w 11 (raw)**                | ✔️ |  64.48 % |   332.10 MiB/s |   1.52 clk/byte |  2,817.60 MiB/s |
| **rANS32x32 16w 12 (raw)**                | ✔️ |  63.83 % |   341.70 MiB/s |   1.53 clk/byte |  2,800.63 MiB/s |
| TurboANX 24                               | ❌ |  63.0 %  |   936.12 MiB/s |  -              |  2,765.31 MiB/s |
| **rANS32x32 16w 10**                      | ✔️ |  65.56 % |   237.21 MiB/s |   1.55 clk/byte |  2,765.18 MiB/s |
| **rANS32x32 16w 11**                      | ✔️ |  64.30 % |   238.29 MiB/s |   1.57 clk/byte |  2,735.12 MiB/s |
| **rANS32x32 16w 12**                      | ✔️ |  63.71 % |   243.00 MiB/s |   1.62 clk/byte |  2,642.01 MiB/s |
| TurboANX 16                               | ❌ |  62.8 %  |   902.32 MiB/s |  -              |  2,631.85 MiB/s |
| FSE Huff0                                 | ✔️ |  63.4 %  | 1,581.32 MiB/s |  -              |  2,515.23 MiB/s |
| htscodecs rans32avx2 0                    | ✔️ |  63.5 %  | 1,041.93 MiB/s |  -              |  2,374.04 MiB/s |
| TurboANX 8                                | ❌ |  62.7 %  |   823.76 MiB/s |  -              |  2,347.10 MiB/s |
| htscodecs rans32avx512 0                  | ✔️ |  63.5 %  |   796.70 MiB/s |  -              |  2,221.93 MiB/s |
| htscodecs rans32sse 0                     | ✔️ |  63.5 %  |   732.08 MiB/s |  -              |  1,948.66 MiB/s |
| TurboANX 4                                | ❌ |  63.0 %  |   706.92 MiB/s |  -              |  1,929.18 MiB/s |
| **rANS32x64 16w 14 (raw)**                | ✔️ |  63.55 % |   350.13 MiB/s |   2.22 clk/byte |  1,926.82 MiB/s |
| **rANS32x64 16w 13 (raw)**                | ✔️ |  63.61 % |   345.16 MiB/s |   2.23 clk/byte |  1,924.81 MiB/s |
| **rANS32x64 16w 15 (raw)**                | ✔️ |  63.57 % |   340.96 MiB/s |   2.30 clk/byte |  1,861.57 MiB/s |
| **rANS32x64 16w 13**                      | ✔️ |  63.53 % |   232.05 MiB/s |   2.32 clk/byte |  1,846.34 MiB/s |
| **rANS32x64 16w 14**                      | ✔️ |  63.47 % |   235.14 MiB/s |   2.33 clk/byte |  1,837.19 MiB/s |
| **rANS32x32 16w 13 (raw)**                | ✔️ |  63.61 % |   344.26 MiB/s |   2.35 clk/byte |  1,818.86 MiB/s |
| **rANS32x32 16w 14 (raw)**                | ✔️ |  63.55 % |   324.44 MiB/s |   2.37 clk/byte |  1,810.24 MiB/s |
| **rANS32x32 16w 14**                      | ✔️ |  63.45 % |   252.28 MiB/s |   2.42 clk/byte |  1,772.88 MiB/s |
| **rANS32x32 16w 13**                      | ✔️ |  63.52 % |   249.07 MiB/s |   2.42 clk/byte |  1,772.30 MiB/s |
| **rANS32x64 16w 15**                      | ✔️ |  63.48 % |   235.02 MiB/s |   2.46 clk/byte |  1,744.39 MiB/s |
| **rANS32x32 16w 15 (raw)**                | ✔️ |  63.57 % |   336.51 MiB/s |   2.55 clk/byte |  1,679.08 MiB/s |
| **rANS32x32 16w 15**                      | ✔️ |  63.50 % |   250.86 MiB/s |   2.64 clk/byte |  1,622.75 MiB/s |
| TurboANX 2                                | ❌ |  64.0 %  |   656.86 MiB/s |  -              |  1,416.33 MiB/s |
| FSE                                       | ✔️ |  63.2 %  |   736.10 MiB/s |  -              |    966.58 MiB/s |
| TurboANX 1                                | ❌ |  66.4 %  |   522.13 MiB/s |  -              |    942.43 MiB/s |
| htscodecs rans32avx512 1                  | ✔️ |  51.6 %  |   168.22 MiB/s |  -              |    322.22 MiB/s |
| htscodecs rans32avx2 1                    | ✔️ |  51.6 %  |   177.36 MiB/s |  -              |    319.15 MiB/s |
| FastHF                                    | ✔️ |  63.6 %  |   189.84 MiB/s |  -              |    151.62 MiB/s |
| FastAC                                    | ✔️ |  63.2 %  |   223.06 MiB/s |  -              |     84.37 MiB/s |
| htscodecs arith_dyn 1                     | ✔️ |  47.8 %  |    89.60 MiB/s |  -              |     81.63 MiB/s |
| htscodecs arith_dyn 0                     | ✔️ |  62.0 %  |    88.09 MiB/s |  -              |     75.05 MiB/s |

### [x-ray](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (X-Ray Medical Image, Part of the Silesia Corpus)
| Codec Type | Open-<br/>Source | Ratio | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | :-: | --: | --: | --: | --: |
| **rANS32x64 16w 11 (raw)**   | ✔️ |  82.60 % |   311.60 MiB/s |   1.39 clk/byte |  3,079.98 MiB/s |
| **rANS32x64 16w 12**         | ✔️ |  80.17 % |   193.60 MiB/s |   1.41 clk/byte |  3,048.15 MiB/s |
| **rANS32x64 16w 12 (raw)**   | ✔️ |  82.57 % |   308.10 MiB/s |   1.41 clk/byte |  3,041.07 MiB/s |
| **rANS32x64 16w 10**         | ✔️ |  80.81 % |   193.28 MiB/s |   1.41 clk/byte |  3,040.97 MiB/s |
| **rANS32x64 16w 10 (raw)**   | ✔️ |  82.83 % |   305.96 MiB/s |   1.42 clk/byte |  3,027.01 MiB/s |
| **rANS32x64 16w 11**         | ✔️ |  80.24 % |   186.41 MiB/s |   1.42 clk/byte |  3,015.25 MiB/s |
| TurboANX 63                  | ❌ |  79.6  % |   989.68 MiB/s |   -             |  2,966.83 MiB/s |
| TurboANX 48                  | ❌ |  79.6  % |   979.24 MiB/s |   -             |  2,923.90 MiB/s |
| TurboANX 40                  | ❌ |  79.7  % |   982.57 MiB/s |   -             |  2,904.99 MiB/s |
| **rANS32x32 16w 11 (raw)**   | ✔️ |  82.60 % |   303.34 MiB/s |   1.48 clk/byte |  2,886.18 MiB/s |
| **rANS32x32 16w 10 (raw)**   | ✔️ |  82.83 % |   301.23 MiB/s |   1.49 clk/byte |  2,881.42 MiB/s |
| **rANS32x32 16w 12 (raw)**   | ✔️ |  82.57 % |   307.10 MiB/s |   1.49 clk/byte |  2,872.78 MiB/s |
| TurboANX 32                  | ❌ |  79.7  % |   973.82 MiB/s |   -             |  2,860.76 MiB/s |
| **rANS32x32 16w 10**         | ✔️ |  80.81 % |   192.99 MiB/s |   1.51 clk/byte |  2,841.71 MiB/s |
| **rANS32x32 16w 11**         | ✔️ |  80.24 % |   190.01 MiB/s |   1.51 clk/byte |  2,834.43 MiB/s |
| **rANS32x32 16w 12**         | ✔️ |  80.53 % |   195.09 MiB/s |   1.54 clk/byte |  2,787.94 MiB/s |
| TurboANX 24                  | ❌ |  79.8  % |   962.68 MiB/s |   -             |  2,785.82 MiB/s |
| TurboANX 16                  | ❌ |  79.9  % |   937.33 MiB/s |   -             |  2,661.07 MiB/s |
| TurboANX 8                   | ❌ |  80.5  % |   864.63 MiB/s |   -             |  2,360.30 MiB/s |
| htscodecs rans32avx2 0       | ✔️ |  80.6  % |   966.58 MiB/s |   -             |  2,244.87 MiB/s |
| htscodecs rans32avx512 0     | ✔️ |  80.6  % |   739.14 MiB/s |   -             |  2,139.47 MiB/s |
| FSE Huff0                    | ✔️ |  80.0  % | 1,395.71 MiB/s |   -             |  1,946.34 MiB/s |
| htscodecs rans32sse 0        | ✔️ |  80.6  % |   723.48 MiB/s |   -             |  1,914.15 MiB/s |
| **rANS32x64 16w 13 (raw)**   | ✔️ |  82.57 % |   305.45 MiB/s |   2.24 clk/byte |  1,910.60 MiB/s |
| **rANS32x64 16w 14 (raw)**   | ✔️ |  82.58 % |   308.96 MiB/s |   2.25 clk/byte |  1,903.66 MiB/s |
| **rANS32x64 16w 13**         | ✔️ |  79.98 % |   191.74 MiB/s |   2.26 clk/byte |  1,892.64 MiB/s |
| TurboANX 4                   | ❌ |  81.9  % |   677.08 MiB/s |   -             |  1,883.40 MiB/s |
| **rANS32x32 16w 13 (raw)**   | ✔️ |  82.57 % |   305.00 MiB/s |   2.29 clk/byte |  1,870.26 MiB/s |
| **rANS32x64 16w 15 (raw)**   | ✔️ |  82.63 % |   307.44 MiB/s |   2.30 clk/byte |  1,865.65 MiB/s |
| **rANS32x32 16w 14 (raw)**   | ✔️ |  82.58 % |   306.18 MiB/s |   2.30 clk/byte |  1,865.18 MiB/s |
| **rANS32x64 16w 14**         | ✔️ |  80.02 % |   192.71 MiB/s |   2.30 clk/byte |  1,861.42 MiB/s |
| **rANS32x32 16w 13**         | ✔️ |  80.01 % |   196.93 MiB/s |   2.37 clk/byte |  1,808.33 MiB/s |
| **rANS32x64 16w 15**         | ✔️ |  80.25 % |   193.85 MiB/s |   2.42 clk/byte |  1,773.42 MiB/s |
| **rANS32x32 16w 14**         | ✔️ |  80.06 % |   198.86 MiB/s |   2.42 clk/byte |  1,767.12 MiB/s |
| **rANS32x32 16w 15 (raw)**   | ✔️ |  82.63 % |   304.21 MiB/s |   2.44 clk/byte |  1,758.57 MiB/s |
| **rANS32x32 16w 15**         | ✔️ |  80.06 % |   191.91 MiB/s |   2.70 clk/byte |  1,585.77 MiB/s |
| TurboANX 2                   | ❌ |  83.7  % |   600.46 MiB/s |   -             |  1,292.65 MiB/s |
| FSE                          | ✔️ |  80.3  % |   696.88 MiB/s |   -             |    990.39 MiB/s |
| TurboANX 1                   | ❌ |  85.1  % |   387.40 MiB/s |   -             |    719.84 MiB/s |
| htscodecs rans32avx2 1       | ✔️ |  74.4  % |   114.89 MiB/s |   -             |    229.78 MiB/s |
| htscodecs rans32avx512 1     | ✔️ |  74.4  % |   104.87 MiB/s |   -             |    220.91 MiB/s |
| FastHF                       | ✔️ |  80.0  % |   183.35 MiB/s |   -             |    144.30 MiB/s |
| FastAC                       | ✔️ |  79.7  % |   244.35 MiB/s |   -             |     77.33 MiB/s |
| htscodecs arith_dyn 1        | ✔️ |  67.6  % |    45.13 MiB/s |   -             |     45.67 MiB/s |
| htscodecs arith_dyn 0        | ✔️ |  79.6  % |    47.12 MiB/s |   -             |     45.40 MiB/s |

### [mozilla](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (Tarred executables of Mozilla 1.0, Part of the Silesia Corpus)
| Codec Type | Open-<br/>Source | Ratio | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | :-: | --: | --: | --: | --: |
| **rANS32x64 16w 11 (raw)**                | ✔️ |  77.82 % |   309.39 MiB/s |   1.44 clk/byte |  2,978.20 MiB/s |
| TurboANX 63                               | ❌ |  70.1  % |   965.97 MiB/s |   -             |  2,959.13 MiB/s |
| **rANS32x64 16w 12 (raw)**                | ✔️ |  77.79 % |   308.29 MiB/s |   1.45 clk/byte |  2,946.52 MiB/s |
| **rANS32x64 16w 10**                      | ✔️ |  76.51 % |   206.82 MiB/s |   1.46 clk/byte |  2,927.30 MiB/s |
| **rANS32x64 16w 10 (raw)**                | ✔️ |  77.93 % |   302.67 MiB/s |   1.47 clk/byte |  2,916.71 MiB/s |
| TurboANX 48                               | ❌ |  69.6  % |   954.87 MiB/s |   -             |  2,911.55 MiB/s |
| **rANS32x64 16w 11**                      | ✔️ |  75.36 % |   201.83 MiB/s |   1.48 clk/byte |  2,894.53 MiB/s |
| TurboANX 40                               | ❌ |  69.3  % |   941.29 MiB/s |   -             |  2,869.21 MiB/s |
| **rANS32x32 16w 11 (raw)**                | ✔️ |  77.82 % |   298.79 MiB/s |   1.49 clk/byte |  2,867.33 MiB/s |
| **rANS32x32 16w 10 (raw)**                | ✔️ |  77.93 % |   300.28 MiB/s |   1.52 clk/byte |  2,826.16 MiB/s |
| TurboANX 32                               | ❌ |  68.9  % |   927.04 MiB/s |   -             |  2,815.87 MiB/s |
| **rANS32x32 16w 12 (raw)**                | ✔️ |  77.79 % |   305.16 MiB/s |   1.54 clk/byte |  2,782.36 MiB/s |
| **rANS32x32 16w 10**                      | ✔️ |  76.51 % |   205.76 MiB/s |   1.55 clk/byte |  2,757.67 MiB/s |
| **rANS32x32 16w 11**                      | ✔️ |  75.36 % |   205.29 MiB/s |   1.57 clk/byte |  2,733.71 MiB/s |
| TurboANX 24                               | ❌ |  68.4  % |   900.92 MiB/s |   -             |  2,732.74 MiB/s |
| **rANS32x64 16w 12**                      | ✔️ |  72.16 % |   198.68 MiB/s |   1.63 clk/byte |  2,631.17 MiB/s |
| TurboANX 16                               | ❌ |  67.9  % |   854.34 MiB/s |   -             |  2,582.05 MiB/s |
| **rANS32x32 16w 12**                      | ✔️ |  71.21 % |   202.45 MiB/s |   1.85 clk/byte |  2,319.35 MiB/s |
| htscodecs rans32avx2 0                    | ✔️ |  69.5  % | 1,014.19 MiB/s |   -             |  2,250.58 MiB/s |
| TurboANX 8                                | ❌ |  67.2  % |   748.14 MiB/s |   -             |  2,183.29 MiB/s |
| htscodecs rans32avx512 0                  | ✔️ |  69.5  % |   760.33 MiB/s |   -             |  2,115.31 MiB/s |
| FSE Huff0                                 | ✔️ |  69.2  % | 1,491.60 MiB/s |   -             |  2,092.00 MiB/s |
| **rANS32x64 16w 14 (raw)**                | ✔️ |  77.79 % |   307.05 MiB/s |   2.26 clk/byte |  1,891.46 MiB/s |
| htscodecs rans32sse 0                     | ✔️ |  69.5  % |   724.39 MiB/s |   -             |  1,884.40 MiB/s |
| **rANS32x64 16w 13 (raw)**                | ✔️ |  77.79 % |   308.28 MiB/s |   2.27 clk/byte |  1,883.91 MiB/s |
| **rANS32x64 16w 15 (raw)**                | ✔️ |  77.85 % |   309.13 MiB/s |   2.31 clk/byte |  1,855.74 MiB/s |
| **rANS32x32 16w 13 (raw)**                | ✔️ |  77.78 % |   306.95 MiB/s |   2.35 clk/byte |  1,824.85 MiB/s |
| **rANS32x32 16w 14 (raw)**                | ✔️ |  77.79 % |   302.09 MiB/s |   2.35 clk/byte |  1,818.82 MiB/s |
| **rANS32x64 16w 13**                      | ✔️ |  73.22 % |   199.90 MiB/s |   2.43 clk/byte |  1,763.01 MiB/s |
| **rANS32x32 16w 15 (raw)**                | ✔️ |  77.84 % |   301.06 MiB/s |   2.44 clk/byte |  1,758.41 MiB/s |
| **rANS32x32 16w 13**                      | ✔️ |  73.24 % |   204.45 MiB/s |   2.54 clk/byte |  1,688.64 MiB/s |
| **rANS32x64 16w 14**                      | ✔️ |  73.23 % |   199.48 MiB/s |   2.56 clk/byte |  1,672.95 MiB/s |
| TurboANX 4                                | ❌ |  67.3  % |   603.91 MiB/s |   -             |  1,658.68 MiB/s |
| **rANS32x32 16w 14**                      | ✔️ |  73.27 % |   204.91 MiB/s |   2.66 clk/byte |  1,611.11 MiB/s |
| **rANS32x32 16w 15**                      | ✔️ |  74.38 % |   204.20 MiB/s |   2.78 clk/byte |  1,543.54 MiB/s |
| **rANS32x64 16w 15**                      | ✔️ |  72.21 % |   198.42 MiB/s |   3.18 clk/byte |  1,345.59 MiB/s |
| TurboANX 2                                | ❌ |  68.5  % |   556.95 MiB/s |   -             |  1,106.06 MiB/s |
| FSE                                       | ✔️ |  69.3  % |   713.08 MiB/s |   -             |    973.71 MiB/s |
| TurboANX 1                                | ❌ |  71.6  % |   392.67 MiB/s |   -             |    677.10 MiB/s |
| htscodecs rans32avx512 1                  | ✔️ |  55.7  % |    81.02 MiB/s |   -             |    168.42 MiB/s |
| htscodecs rans32avx2 1                    | ✔️ |  55.7  % |    83.68 MiB/s |   -             |    167.19 MiB/s |
| FastHF                                    | ✔️ |  71.8  % |   174.86 MiB/s |   -             |    130.78 MiB/s |
| FastAC                                    | ✔️ |  70.7  % |   234.95 MiB/s |   -             |     81.01 MiB/s |
| htscodecs arith_dyn 1                     | ✔️ |  52.1  % |    62.87 MiB/s |   -             |     62.98 MiB/s |
| htscodecs arith_dyn 0                     | ✔️ |  66.4  % |    63.82 MiB/s |   -             |     59.92 MiB/s |

## Easy Multithreading
hypersonic-rANS includes a variant that's encodes blocks independently (at the expense of compression ratio) allowing for easy multithreading.

### [x-ray](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (X-Ray Medical Image, Part of the Silesia Corpus)
| Codec Type | Ratio | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: |
| rANS32x64 16w 10 mt |  80.23 % |   200.03 MiB/s |    0.24 clk/byte | **18,035.77 MiB/s** |
| rANS32x32 16w 10 mt |  80.17 % |   194.73 MiB/s |    0.25 clk/byte | **17,834.38 MiB/s** |
| rANS32x64 16w 11 mt |  80.08 % |   202.10 MiB/s |    0.26 clk/byte | **16,210.44 MiB/s** |
| rANS32x32 16w 11 mt |  80.02 % |   191.90 MiB/s |    0.27 clk/byte | **15,630.58 MiB/s** |
| rANS32x64 16w 12 mt |  80.05 % |   197.62 MiB/s |    0.34 clk/byte | **13,207.00 MiB/s** |
| rANS32x32 16w 12 mt |  79.99 % |   197.21 MiB/s |    0.36 clk/byte | **12,358.57 MiB/s** |
| rANS32x64 16w 13 mt |  80.04 % |   199.94 MiB/s |    0.37 clk/byte | **11,938.77 MiB/s** |
| rANS32x32 16w 13 mt |  79.99 % |   195.00 MiB/s |    0.37 clk/byte | **11,497.36 MiB/s** |
| rANS32x64 16w 14 mt |  80.05 % |   199.87 MiB/s |    0.42 clk/byte | **10,318.01 MiB/s** |
| rANS32x32 16w 14 mt |  80.01 % |   190.94 MiB/s |    0.42 clk/byte | **10,134.59 MiB/s** |
| rANS32x64 16w 15 mt |  80.09 % |   200.59 MiB/s |    0.59 clk/byte |  **7,308.43 MiB/s** |
| rANS32x32 16w 15 mt |  80.03 % |   192.28 MiB/s |    0.62 clk/byte |  **7,024.69 MiB/s** |

## Building
### On Linux/WSL
Clang:
```bash
premake/premake5 gmake2
config=releaseclang_x64 make
```

GCC:
```bash
premake/premake5 gmake2
config=release_x64 make
```

### Windows
```shell
premake/premake5.exe vs2022
msbuild /m /p:Configuration=Release /v:m
```

## License
Two Clause BSD
