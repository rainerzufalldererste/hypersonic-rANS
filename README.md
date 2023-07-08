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
| Codec Type | Ratio | Encoder<br/>Clocks/Byte | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
| **rANS32x64 16w        10**               |  65.59 % |   12.83 clk/byte |   341.55 MiB/s |   1.43 clk/byte |  2,989.66 MiB/s |
| **rANS32x64 16w        11**               |  64.33 % |   12.34 clk/byte |   347.24 MiB/s |   1.44 clk/byte |  2,973.71 MiB/s |
| **rANS32x64 16w        12**               |  63.81 % |   12.51 clk/byte |   342.31 MiB/s |   1.44 clk/byte |  2,967.92 MiB/s |
| TurboANX 63                               |  63.4 %  |   -              |   981.79 MiB/s |  -              |  2,964.02 MiB/s |
| TurboANX 48                               |  63.3 %  |   -              |   969.72 MiB/s |  -              |  2,917.59 MiB/s |
| TurboANX 40                               |  63.2 %  |   -              |   964.45 MiB/s |  -              |  2,883.45 MiB/s |
| TurboANX 32                               |  66.4 %  |   -              |   951.53 MiB/s |  -              |  2,856.26 MiB/s |
| **rANS32x32 16w        11**               |  64.33 % |   12.86 clk/byte |   333.03 MiB/s |   1.50 clk/byte |  2,856.20 MiB/s |
| **rANS32x32 16w        10**               |  65.59 % |   12.80 clk/byte |   334.68 MiB/s |   1.51 clk/byte |  2,845.56 MiB/s |
| TurboANX 24                               |  63.0 %  |   -              |   936.12 MiB/s |  -              |  2,765.31 MiB/s |
| TurboANX 16                               |  62.8 %  |   -              |   902.32 MiB/s |  -              |  2,631.85 MiB/s |
| **rANS32x32 16w        12**               |  63.81 % |   12.83 clk/byte |   343.55 MiB/s |   1.54 clk/byte |  2,784.13 MiB/s |
| fsehuf                                    |  63.4 %  |   -              | 1,581.32 MiB/s |  -              |  2,515.23 MiB/s |
| htscodecs rans32avx2 0                    |  63.5 %  |   -              | 1,041.93 MiB/s |  -              |  2,374.04 MiB/s |
| TurboANX 8                                |  62.7 %  |   -              |   823.76 MiB/s |  -              |  2,347.10 MiB/s |
| **rANS32x32 32blk 16w  12**               |  63.81 % |   12.62 clk/byte |   339.50 MiB/s |   1.85 clk/byte |  2,312.10 MiB/s |
| **rANS32x32 32blk 16w  11**               |  64.33 % |   12.67 clk/byte |   338.00 MiB/s |   1.86 clk/byte |  2,299.31 MiB/s |
| **rANS32x32 32blk 16w  10**               |  65.59 % |   12.91 clk/byte |   331.80 MiB/s |   1.87 clk/byte |  2,289.10 MiB/s |
| htscodecs rans32avx512 0                  |  63.5 %  |   -              |   796.70 MiB/s |  -              |  2,221.93 MiB/s |
| **rANS32x32 32blk 8w   11**               |  64.33 % |   15.01 clk/byte |   285.45 MiB/s |   2.15 clk/byte |  1,988.10 MiB/s |
| **rANS32x32 32blk 8w   12**               |  63.82 % |   15.15 clk/byte |   282.80 MiB/s |   2.16 clk/byte |  1,984.68 MiB/s |
| **rANS32x32 32blk 8w   10**               |  65.60 % |   14.70 clk/byte |   291.41 MiB/s |   2.17 clk/byte |  1,977.26 MiB/s |
| htscodecs rans32sse 0                     |  63.5 %  |   -              |   732.08 MiB/s |  -              |  1,948.66 MiB/s |
| TurboANX 4                                |  63.0 %  |   -              |   706.92 MiB/s |  -              |  1,929.18 MiB/s |
| **rANS32x64 16w        13**               |  63.61 % |   12.32 clk/byte |   348.13 MiB/s |   2.29 clk/byte |  1,872.44 MiB/s |
| **rANS32x64 16w        14**               |  63.55 % |   12.36 clk/byte |   346.57 MiB/s |   2.28 clk/byte |  1,876.95 MiB/s |
| **rANS32x64 16w        15**               |  63.57 % |   12.30 clk/byte |   350.49 MiB/s |   2.34 clk/byte |  1,828.28 MiB/s |
| **rANS32x32 16w        13**               |  63.61 % |   12.55 clk/byte |   341.20 MiB/s |   2.38 clk/byte |  1,800.28 MiB/s |
| **rANS32x32 16w        14**               |  63.55 % |   12.54 clk/byte |   341.70 MiB/s |   2.39 clk/byte |  1,795.66 MiB/s |
| **rANS32x16 16w        10**               |  65.59 % |   13.26 clk/byte |   323.07 MiB/s |   2.54 clk/byte |  1,684.80 MiB/s |
| **rANS32x16 16w        12**               |  63.81 % |   13.21 clk/byte |   324.24 MiB/s |   2.55 clk/byte |  1,681.73 MiB/s |
| **rANS32x16 16w        11**               |  64.33 % |   13.25 clk/byte |   323.17 MiB/s |   2.55 clk/byte |  1,676.41 MiB/s |
| **rANS32x32 16w        15**               |  63.57 % |   12.94 clk/byte |   342.60 MiB/s |   2.56 clk/byte |  1,675.11 MiB/s |
| **rANS32x32 32blk 16w  14**               |  63.55 % |   13.02 clk/byte |   329.08 MiB/s |   2.66 clk/byte |  1,607.26 MiB/s |
| **rANS32x32 32blk 16w  13**               |  63.61 % |   12.56 clk/byte |   341.16 MiB/s |   2.71 clk/byte |  1,582.28 MiB/s |
| **rANS32x32 32blk 16w  15**               |  63.57 % |   13.21 clk/byte |   324.33 MiB/s |   2.76 clk/byte |  1,550.93 MiB/s |
| **rANS32x32 32blk 8w   13**               |  63.60 % |   15.07 clk/byte |   284.24 MiB/s |   2.98 clk/byte |  1,438.01 MiB/s |
| **rANS32x32 32blk 8w   14**               |  63.53 % |   15.06 clk/byte |   284.45 MiB/s |   3.00 clk/byte |  1,429.24 MiB/s |
| TurboANX 2                                |  64.0 %  |   -              |   656.86 MiB/s |  -              |  1,416.33 MiB/s |
| **rANS32x32 32blk 8w   15**               |  63.51 % |   15.11 clk/byte |   283.41 MiB/s |   3.10 clk/byte |  1,381.63 MiB/s |
| **rANS32x16 16w        13**               |  63.61 % |   13.14 clk/byte |   325.92 MiB/s |   3.60 clk/byte |  1,190.23 MiB/s |
| **rANS32x16 16w        14**               |  63.55 % |   13.37 clk/byte |   320.41 MiB/s |   3.64 clk/byte |  1,175.92 MiB/s |
| **rANS32x16 16w        15**               |  63.57 % |   13.28 clk/byte |   322.51 MiB/s |   4.21 clk/byte |  1,017.12 MiB/s |
| fse                                       |  63.2 %  |   -              |   736.10 MiB/s |  -              |    966.58 MiB/s |
| TurboANX 1                                |  66.4 %  |   -              |   522.13 MiB/s |  -              |    942.43 MiB/s |
| htscodecs rans32avx512 1                  |  51.6 %  |   -              |   168.22 MiB/s |  -              |    322.22 MiB/s |
| htscodecs rans32avx2 1                    |  51.6 %  |   -              |   177.36 MiB/s |  -              |    319.15 MiB/s |
| FastHF                                    |  63.6 %  |   -              |   189.84 MiB/s |  -              |    151.62 MiB/s |
| FastAC                                    |  63.2 %  |   -              |   223.06 MiB/s |  -              |     84.37 MiB/s |
| htscodecs arith_dyn 1                     |  47.8 %  |   -              |    89.60 MiB/s |  -              |     81.63 MiB/s |
| htscodecs arith_dyn 0                     |  62.0 %  |   -              |    88.09 MiB/s |  -              |     75.05 MiB/s |

### [x-ray](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (X-Ray Medical Image, Part of the Silesia Corpus)
| Codec Type | License | Ratio | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
| **rANS32x64 16w 11 (raw)**   | BSD-2  |  82.60 % |   311.60 MiB/s |   1.39 clk/byte |  3,079.98 MiB/s |
| **rANS32x64 16w 12**         | BSD-2  |  80.17 % |   193.60 MiB/s |   1.41 clk/byte |  3,048.15 MiB/s |
| **rANS32x64 16w 12 (raw)**   | BSD-2  |  82.57 % |   308.10 MiB/s |   1.41 clk/byte |  3,041.07 MiB/s |
| **rANS32x64 16w 10**         | BSD-2  |  80.81 % |   193.28 MiB/s |   1.41 clk/byte |  3,040.97 MiB/s |
| **rANS32x64 16w 10 (raw)**   | BSD-2  |  82.83 % |   305.96 MiB/s |   1.42 clk/byte |  3,027.01 MiB/s |
| **rANS32x64 16w 11**         | BSD-2  |  80.24 % |   186.41 MiB/s |   1.42 clk/byte |  3,015.25 MiB/s |
| TurboANX 63                  | -      |  79.6  % |   989.68 MiB/s |   -             |  2,966.83 MiB/s |
| TurboANX 48                  | -      |  79.6  % |   979.24 MiB/s |   -             |  2,923.90 MiB/s |
| TurboANX 40                  | -      |  79.7  % |   982.57 MiB/s |   -             |  2,904.99 MiB/s |
| **rANS32x32 16w 11 (raw)**   | BSD-2  |  82.60 % |   303.34 MiB/s |   1.48 clk/byte |  2,886.18 MiB/s |
| **rANS32x32 16w 10 (raw)**   | BSD-2  |  82.83 % |   301.23 MiB/s |   1.49 clk/byte |  2,881.42 MiB/s |
| **rANS32x32 16w 12 (raw)**   | BSD-2  |  82.57 % |   307.10 MiB/s |   1.49 clk/byte |  2,872.78 MiB/s |
| TurboANX 32                  | -      |  79.7  % |   973.82 MiB/s |   -             |  2,860.76 MiB/s |
| **rANS32x32 16w 10**         | BSD-2  |  80.81 % |   192.99 MiB/s |   1.51 clk/byte |  2,841.71 MiB/s |
| **rANS32x32 16w 11**         | BSD-2  |  80.24 % |   190.01 MiB/s |   1.51 clk/byte |  2,834.43 MiB/s |
| **rANS32x32 16w 12**         | BSD-2  |  80.53 % |   195.09 MiB/s |   1.54 clk/byte |  2,787.94 MiB/s |
| TurboANX 24                  | -      |  79.8  % |   962.68 MiB/s |   -             |  2,785.82 MiB/s |
| TurboANX 16                  | -      |  79.9  % |   937.33 MiB/s |   -             |  2,661.07 MiB/s |
| TurboANX 8                   | -      |  80.5  % |   864.63 MiB/s |   -             |  2,360.30 MiB/s |
| htscodecs rans32avx2 0       | BSD-3  |  80.6  % |   966.58 MiB/s |   -             |  2,244.87 MiB/s |
| htscodecs rans32avx512 0     | BSD-3  |  80.6  % |   739.14 MiB/s |   -             |  2,139.47 MiB/s |
| FSE Huff0                    | BSD-2  |  80.0  % | 1,395.71 MiB/s |   -             |  1,946.34 MiB/s |
| htscodecs rans32sse 0        | BSD-3  |  80.6  % |   723.48 MiB/s |   -             |  1,914.15 MiB/s |
| **rANS32x64 16w 13 (raw)**   | BSD-2  |  82.57 % |   305.45 MiB/s |   2.24 clk/byte |  1,910.60 MiB/s |
| **rANS32x64 16w 14 (raw)**   | BSD-2  |  82.58 % |   308.96 MiB/s |   2.25 clk/byte |  1,903.66 MiB/s |
| **rANS32x64 16w 13**         | BSD-2  |  79.98 % |   191.74 MiB/s |   2.26 clk/byte |  1,892.64 MiB/s |
| TurboANX 4                   | -      |  81.9  % |   677.08 MiB/s |   -             |  1,883.40 MiB/s |
| **rANS32x32 16w 13 (raw)**   | BSD-2  |  82.57 % |   305.00 MiB/s |   2.29 clk/byte |  1,870.26 MiB/s |
| **rANS32x64 16w 15 (raw)**   | BSD-2  |  82.63 % |   307.44 MiB/s |   2.30 clk/byte |  1,865.65 MiB/s |
| **rANS32x32 16w 14 (raw)**   | BSD-2  |  82.58 % |   306.18 MiB/s |   2.30 clk/byte |  1,865.18 MiB/s |
| **rANS32x64 16w 14**         | BSD-2  |  80.02 % |   192.71 MiB/s |   2.30 clk/byte |  1,861.42 MiB/s |
| **rANS32x32 16w 13**         | BSD-2  |  80.01 % |   196.93 MiB/s |   2.37 clk/byte |  1,808.33 MiB/s |
| **rANS32x64 16w 15**         | BSD-2  |  80.25 % |   193.85 MiB/s |   2.42 clk/byte |  1,773.42 MiB/s |
| **rANS32x32 16w 14**         | BSD-2  |  80.06 % |   198.86 MiB/s |   2.42 clk/byte |  1,767.12 MiB/s |
| **rANS32x32 16w 15 (raw)**   | BSD-2  |  82.63 % |   304.21 MiB/s |   2.44 clk/byte |  1,758.57 MiB/s |
| **rANS32x32 16w 15**         | BSD-2  |  80.06 % |   191.91 MiB/s |   2.70 clk/byte |  1,585.77 MiB/s |
| TurboANX 2                   | -      |  83.7  % |   600.46 MiB/s |   -             |  1,292.65 MiB/s |
| FSE                          | BSD-2  |  80.3  % |   696.88 MiB/s |   -             |    990.39 MiB/s |
| TurboANX 1                   | -      |  85.1  % |   387.40 MiB/s |   -             |    719.84 MiB/s |
| htscodecs rans32avx2 1       | BSD-3  |  74.4  % |   114.89 MiB/s |   -             |    229.78 MiB/s |
| htscodecs rans32avx512 1     | BSD-3  |  74.4  % |   104.87 MiB/s |   -             |    220.91 MiB/s |
| FastHF                       | Custom |  80.0  % |   183.35 MiB/s |   -             |    144.30 MiB/s |
| FastAC                       | Custom |  79.7  % |   244.35 MiB/s |   -             |     77.33 MiB/s |
| htscodecs arith_dyn 1        | BSD-3  |  67.6  % |    45.13 MiB/s |   -             |     45.67 MiB/s |
| htscodecs arith_dyn 0        | BSD-3  |  79.6  % |    47.12 MiB/s |   -             |     45.40 MiB/s |

### [mozilla](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (Tarred executables of Mozilla 1.0, Part of the Silesia Corpus)
| Codec Type | Ratio | Encoder<br/>Clocks/Byte | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
| **rANS32x64 16w                    11**   |  77.82 % |   13.84 clk/byte |   309.39 MiB/s |   1.44 clk/byte |  2,978.20 MiB/s |
| **rANS32x64 16w                    10**   |  77.92 % |   14.16 clk/byte |   302.46 MiB/s |   1.44 clk/byte |  2,968.99 MiB/s |
| TurboANX 63                               |  70.1  % |   -              |   965.97 MiB/s |   -             |  2,959.13 MiB/s |
| **rANS32x64 16w                    12**   |  77.79 % |   14.21 clk/byte |   301.44 MiB/s |   1.45 clk/byte |  2,946.52 MiB/s |
| TurboANX 48                               |  69.6  % |   -              |   954.87 MiB/s |   -             |  2,911.55 MiB/s |
| **rANS32x32 16w                    10**   |  77.92 % |   13.97 clk/byte |   306.54 MiB/s |   1.49 clk/byte |  2,878.05 MiB/s |
| TurboANX 40                               |  69.3  % |   -              |   941.29 MiB/s |   -             |  2,869.21 MiB/s |
| **rANS32x32 16w                    11**   |  77.82 % |   14.34 clk/byte |   298.79 MiB/s |   1.49 clk/byte |  2,867.33 MiB/s |
| TurboANX 32                               |  68.9  % |   -              |   927.04 MiB/s |   -             |  2,815.87 MiB/s |
| **rANS32x32 16w                    12**   |  77.79 % |   14.25 clk/byte |   300.51 MiB/s |   1.54 clk/byte |  2,782.35 MiB/s |
| TurboANX 24                               |  68.4  % |   -              |   900.92 MiB/s |   -             |  2,732.74 MiB/s |
| TurboANX 16                               |  67.9  % |   -              |   854.34 MiB/s |   -             |  2,582.05 MiB/s |
| htscodecs_rans32avx2 0                    |  69.5  % |   -              | 1,014.19 MiB/s |   -             |  2,250.58 MiB/s |
| TurboANX 8                                |  67.2  % |   -              |   748.14 MiB/s |   -             |  2,183.29 MiB/s |
| htscodecs_rans32avx512 0                  |  69.5  % |   -              |   760.33 MiB/s |   -             |  2,115.31 MiB/s |
| fsehuf                                    |  69.2  % |   -              | 1,491.60 MiB/s |   -             |  2,092.00 MiB/s |
| **rANS32x32 16w                    14**   |  77.79 % |   14.02 clk/byte |   305.49 MiB/s |   2.37 clk/byte |  1,804.10 MiB/s |
| **rANS32x64 16w                    14**   |  77.79 % |   14.09 clk/byte |   303.97 MiB/s |   2.26 clk/byte |  1,891.46 MiB/s |
| htscodecs_rans32sse 0                     |  69.5  % |   -              |   724.39 MiB/s |   -             |  1,884.40 MiB/s |
| **rANS32x64 16w                    13**   |  77.79 % |   13.89 clk/byte |   308.28 MiB/s |   2.27 clk/byte |  1,883.91 MiB/s |
| **rANS32x64 16w                    15**   |  77.85 % |   13.86 clk/byte |   309.13 MiB/s |   2.31 clk/byte |  1,855.74 MiB/s |
| **rANS32x32 16w                    13**   |  77.78 % |   14.13 clk/byte |   303.23 MiB/s |   2.37 clk/byte |  1,806.03 MiB/s |
| **rANS32x32 16w                    15**   |  77.84 % |   14.29 clk/byte |   299.78 MiB/s |   2.46 clk/byte |  1,743.60 MiB/s |
| TurboANX 4                                |  67.3  % |   -              |   603.91 MiB/s |   -             |  1,658.68 MiB/s |
| TurboANX 2                                |  68.5  % |   -              |   556.95 MiB/s |   -             |  1,106.06 MiB/s |
| fse                                       |  69.3  % |   -              |   713.08 MiB/s |   -             |    973.71 MiB/s |
| TurboANX 1                                |  71.6  % |   -              |   392.67 MiB/s |   -             |    677.10 MiB/s |
| htscodecs_rans32avx512 1                  |  55.7  % |   -              |    81.02 MiB/s |   -             |    168.42 MiB/s |
| htscodecs_rans32avx2 1                    |  55.7  % |   -              |    83.68 MiB/s |   -             |    167.19 MiB/s |
| FastHF                                    |  71.8  % |   -              |   174.86 MiB/s |   -             |    130.78 MiB/s |
| FastAC                                    |  70.7  % |   -              |   234.95 MiB/s |   -             |     81.01 MiB/s |
| htscodecs_arith_dyn 1                     |  52.1  % |   -              |    62.87 MiB/s |   -             |     62.98 MiB/s |
| htscodecs_arith_dyn 0                     |  66.4  % |   -              |    63.82 MiB/s |   -             |     59.92 MiB/s |

## Easy Multithreading
hypersonic-rANS includes a variant that's encodes blocks independently (at the expense of compression ratio) allowing for easy multithreading.

### [x-ray](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (X-Ray Medical Image, Part of the Silesia Corpus)
| Codec Type | Ratio | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
| rANS32x64 16w 10 mt |  80.23 % |   200.03 MiB/s |    0.24 clk/byte | 18,035.77 MiB/s |
| rANS32x32 16w 10 mt |  80.17 % |   194.73 MiB/s |    0.25 clk/byte | 17,834.38 MiB/s |
| rANS32x64 16w 11 mt |  80.08 % |   202.10 MiB/s |    0.26 clk/byte | 16,210.44 MiB/s |
| rANS32x32 16w 11 mt |  80.02 % |   191.90 MiB/s |    0.27 clk/byte | 15,630.58 MiB/s |
| rANS32x64 16w 12 mt |  80.05 % |   197.62 MiB/s |    0.34 clk/byte | 13,207.00 MiB/s |
| rANS32x32 16w 12 mt |  79.99 % |   197.21 MiB/s |    0.36 clk/byte | 12,358.57 MiB/s |
| rANS32x64 16w 13 mt |  80.04 % |   199.94 MiB/s |    0.37 clk/byte | 11,938.77 MiB/s |
| rANS32x32 16w 13 mt |  79.99 % |   195.00 MiB/s |    0.37 clk/byte | 11,497.36 MiB/s |
| rANS32x64 16w 14 mt |  80.05 % |   199.87 MiB/s |    0.42 clk/byte | 10,318.01 MiB/s |
| rANS32x32 16w 14 mt |  80.01 % |   190.94 MiB/s |    0.42 clk/byte | 10,134.59 MiB/s |
| rANS32x64 16w 15 mt |  80.09 % |   200.59 MiB/s |    0.59 clk/byte |  7,308.43 MiB/s |
| rANS32x32 16w 15 mt |  80.03 % |   192.28 MiB/s |    0.62 clk/byte |  7,024.69 MiB/s |

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
