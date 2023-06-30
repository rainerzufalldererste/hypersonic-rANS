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
- Compared to `htscodecs`, `fse`, `fse Huffman`, `Fast HF`, `Fast AC`, `TurboANX` (Jun 2023 via `TurboBench`, MB/s converted to MiB/s) (`TurboANX` uses a native Windows build from @powturbo)
- Notable decompressors selected (the benchmark section would be incredibly long otherwise...)
- hypersonic-rANS codecs **highlighted**
- Sorted by decode throughput.
- All encoders are currently scalar (and not particularly optimized)
- Every best performing decoder variant requires AVX2. (The AVX-512 variants for 32x64 can be faster in rare circumstances, but they weren't in this benchmark)

### [enwik8](http://mattmahoney.net/dc/textdata.html) (Wikipedia Extract, 100,000,000 Bytes)

| Codec Type | Ratio | Encoder<br/>Clocks/Byte | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
| **rANS32x64 16w        10**               |  65.59 % |   12.83 clk/byte |   341.55 MiB/s |   1.43 clk/byte |  2989.66 MiB/s |
| **rANS32x64 16w        11**               |  64.33 % |   12.34 clk/byte |   347.24 MiB/s |   1.44 clk/byte |  2973.71 MiB/s |
| **rANS32x64 16w        12**               |  63.81 % |   12.51 clk/byte |   342.31 MiB/s |   1.44 clk/byte |  2967.92 MiB/s |
| TurboANX 63                               |  63.4 %  |   -              |   981.79 MiB/s |  -              |  2964.02 MiB/s |
| TurboANX 48                               |  63.3 %  |   -              |   969.72 MiB/s |  -              |  2917.59 MiB/s |
| TurboANX 40                               |  63.2 %  |   -              |   964.45 MiB/s |  -              |  2883.45 MiB/s |
| TurboANX 32                               |  66.4 %  |   -              |   951.53 MiB/s |  -              |  2856.26 MiB/s |
| **rANS32x32 16w        11**               |  64.33 % |   12.86 clk/byte |   333.03 MiB/s |   1.50 clk/byte |  2856.20 MiB/s |
| **rANS32x32 16w        10**               |  65.59 % |   12.80 clk/byte |   334.68 MiB/s |   1.51 clk/byte |  2845.56 MiB/s |
| TurboANX 24                               |  63.0 %  |   -              |   936.12 MiB/s |  -              |  2765.31 MiB/s |
| TurboANX 16                               |  62.8 %  |   -              |   902.32 MiB/s |  -              |  2631.85 MiB/s |
| **rANS32x32 16w        12**               |  63.81 % |   12.83 clk/byte |   343.55 MiB/s |   1.54 clk/byte |  2784.13 MiB/s |
| fsehuf                                    |  63.4 %  |   -              |  1581.32 MiB/s |  -              |  2515.23 MiB/s |
| rans32avx2 0                              |  63.5 %  |   -              |  1041.93 MiB/s |  -              |  2374.04 MiB/s |
| TurboANX 8                                |  62.7 %  |   -              |   823.76 MiB/s |  -              |  2347.10 MiB/s |
| **rANS32x32 32blk 16w  12**               |  63.81 % |   12.62 clk/byte |   339.50 MiB/s |   1.85 clk/byte |  2312.10 MiB/s |
| **rANS32x32 32blk 16w  11**               |  64.33 % |   12.67 clk/byte |   338.00 MiB/s |   1.86 clk/byte |  2299.31 MiB/s |
| **rANS32x32 32blk 16w  10**               |  65.59 % |   12.91 clk/byte |   331.80 MiB/s |   1.87 clk/byte |  2289.10 MiB/s |
| rans32avx512 0                            |  63.5 %  |   -              |   796.70 MiB/s |  -              |  2221.93 MiB/s |
| **rANS32x32 32blk 8w   11**               |  64.33 % |   15.01 clk/byte |   285.45 MiB/s |   2.15 clk/byte |  1988.10 MiB/s |
| **rANS32x32 32blk 8w   12**               |  63.82 % |   15.15 clk/byte |   282.80 MiB/s |   2.16 clk/byte |  1984.68 MiB/s |
| **rANS32x32 32blk 8w   10**               |  65.60 % |   14.70 clk/byte |   291.41 MiB/s |   2.17 clk/byte |  1977.26 MiB/s |
| rans32sse 0                               |  63.5 %  |   -              |   732.08 MiB/s |  -              |  1948.66 MiB/s |
| TurboANX 4                                |  63.0 %  |   -              |   706.92 MiB/s |  -              |  1929.18 MiB/s |
| **rANS32x64 16w        13**               |  63.61 % |   12.32 clk/byte |   348.13 MiB/s |   2.29 clk/byte |  1872.44 MiB/s |
| **rANS32x64 16w        14**               |  63.55 % |   12.36 clk/byte |   346.57 MiB/s |   2.28 clk/byte |  1876.95 MiB/s |
| **rANS32x64 16w        15**               |  63.57 % |   12.30 clk/byte |   350.49 MiB/s |   2.34 clk/byte |  1828.28 MiB/s |
| **rANS32x32 16w        13**               |  63.61 % |   12.55 clk/byte |   341.20 MiB/s |   2.38 clk/byte |  1800.28 MiB/s |
| **rANS32x32 16w        14**               |  63.55 % |   12.54 clk/byte |   341.70 MiB/s |   2.39 clk/byte |  1795.66 MiB/s |
| **rANS32x16 16w        10**               |  65.59 % |   13.26 clk/byte |   323.07 MiB/s |   2.54 clk/byte |  1684.80 MiB/s |
| **rANS32x16 16w        12**               |  63.81 % |   13.21 clk/byte |   324.24 MiB/s |   2.55 clk/byte |  1681.73 MiB/s |
| **rANS32x16 16w        11**               |  64.33 % |   13.25 clk/byte |   323.17 MiB/s |   2.55 clk/byte |  1676.41 MiB/s |
| **rANS32x32 16w        15**               |  63.57 % |   12.94 clk/byte |   342.60 MiB/s |   2.56 clk/byte |  1675.11 MiB/s |
| **rANS32x32 32blk 16w  14**               |  63.55 % |   13.02 clk/byte |   329.08 MiB/s |   2.66 clk/byte |  1607.26 MiB/s |
| **rANS32x32 32blk 16w  13**               |  63.61 % |   12.56 clk/byte |   341.16 MiB/s |   2.71 clk/byte |  1582.28 MiB/s |
| **rANS32x32 32blk 16w  15**               |  63.57 % |   13.21 clk/byte |   324.33 MiB/s |   2.76 clk/byte |  1550.93 MiB/s |
| **rANS32x32 32blk 8w   13**               |  63.60 % |   15.07 clk/byte |   284.24 MiB/s |   2.98 clk/byte |  1438.01 MiB/s |
| **rANS32x32 32blk 8w   14**               |  63.53 % |   15.06 clk/byte |   284.45 MiB/s |   3.00 clk/byte |  1429.24 MiB/s |
| TurboANX 2                                |  64.0 %  |   -              |   656.86 MiB/s |  -              |  1416.33 MiB/s |
| **rANS32x32 32blk 8w   15**               |  63.51 % |   15.11 clk/byte |   283.41 MiB/s |   3.10 clk/byte |  1381.63 MiB/s |
| **rANS32x16 16w        13**               |  63.61 % |   13.14 clk/byte |   325.92 MiB/s |   3.60 clk/byte |  1190.23 MiB/s |
| **rANS32x16 16w        14**               |  63.55 % |   13.37 clk/byte |   320.41 MiB/s |   3.64 clk/byte |  1175.92 MiB/s |
| **rANS32x16 16w        15**               |  63.57 % |   13.28 clk/byte |   322.51 MiB/s |   4.21 clk/byte |  1017.12 MiB/s |
| fse                                       |  63.2 %  |   -              |   736.10 MiB/s |  -              |   966.58 MiB/s |
| TurboANX 1                                |  66.4 %  |   -              |   522.13 MiB/s |  -              |   942.43 MiB/s |
| rans32avx512 1                            |  51.6 %  |   -              |   168.22 MiB/s |  -              |   322.22 MiB/s |
| rans32avx2 1                              |  51.6 %  |   -              |   177.36 MiB/s |  -              |   319.15 MiB/s |
| FastHF                                    |  63.6 %  |   -              |   189.84 MiB/s |  -              |   151.62 MiB/s |
| FastAC                                    |  63.2 %  |   -              |   223.06 MiB/s |  -              |    84.37 MiB/s |
| arith_dyn 1                               |  47.8 %  |   -              |    89.60 MiB/s |  -              |    81.63 MiB/s |
| arith_dyn 0                               |  62.0 %  |   -              |    88.09 MiB/s |  -              |    75.05 MiB/s |

The following benchmarks demonstrate, apart from incredibly high decompression speeds, how terrible the histogram generation currently is:

### [x-ray](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (X-Ray Medical Image, Part of the Silesia Corpus)
| Codec Type | Ratio | Encoder<br/>Clocks/Byte | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
| **rANS32x64 16w                    11**   |  82.60 % |   13.75 clk/byte |   311.60 MiB/s |   1.39 clk/byte |  3079.98 MiB/s |
| **rANS32x64 16w                    10**   |  82.66 % |   14.03 clk/byte |   305.22 MiB/s |   1.42 clk/byte |  3026.65 MiB/s |
| TurboANX 63                               |  79.6  % |   -              |   989.68 MiB/s |   -             |  2966.83 MiB/s |
| TurboANX 48                               |  79.6  % |   -              |   979.24 MiB/s |   -             |  2923.90 MiB/s |
| TurboANX 40                               |  79.7  % |   -              |   982.57 MiB/s |   -             |  2904.99 MiB/s |
| **rANS32x64 16w                    12**   |  82.57 % |   13.99 clk/byte |   306.19 MiB/s |   1.48 clk/byte |  2900.31 MiB/s |
| TurboANX 32                               |  79.7  % |   -              |   973.82 MiB/s |   -             |  2860.76 MiB/s |
| **rANS32x32 16w                    11**   |  82.60 % |   14.31 clk/byte |   299.31 MiB/s |   1.50 clk/byte |  2851.47 MiB/s |
| **rANS32x32 16w                    10**   |  82.66 % |   13.82 clk/byte |   309.99 MiB/s |   1.52 clk/byte |  2822.97 MiB/s |
| TurboANX 24                               |  79.8  % |   -              |   962.68 MiB/s |   -             |  2785.82 MiB/s |
| **rANS32x32 16w                    12**   |  82.57 % |   13.95 clk/byte |   306.97 MiB/s |   1.59 clk/byte |  2693.99 MiB/s |
| TurboANX 16                               |  79.9  % |   -              |   937.33 MiB/s |   -             |  2661.07 MiB/s |
| TurboANX 8                                |  80.5  % |   -              |   864.63 MiB/s |   -             |  2360.30 MiB/s |
| rans32avx2 0                              |  80.6  % |   -              |   966.58 MiB/s |   -             |  2244.87 MiB/s |
| rans32avx512 0                            |  80.6  % |   -              |   739.14 MiB/s |   -             |  2139.47 MiB/s |
| fsehuf                                    |  80.0  % |   -              |  1395.71 MiB/s |   -             |  1946.34 MiB/s |
| rans32sse 0                               |  80.6  % |   -              |   723.48 MiB/s |   -             |  1914.15 MiB/s |
| **rANS32x64 16w                    13**   |  82.57 % |   13.94 clk/byte |   307.28 MiB/s |   2.25 clk/byte |  1903.01 MiB/s |
| TurboANX 4                                |  81.9  % |   -              |   677.08 MiB/s |   -             |  1883.40 MiB/s |
| **rANS32x64 16w                    14**   |  82.58 % |   14.09 clk/byte |   304.01 MiB/s |   2.29 clk/byte |  1870.17 MiB/s |
| **rANS32x32 16w                    13**   |  82.57 % |   13.97 clk/byte |   306.60 MiB/s |   2.31 clk/byte |  1855.99 MiB/s |
| **rANS32x64 16w                    15**   |  82.63 % |   13.88 clk/byte |   308.52 MiB/s |   2.39 clk/byte |  1793.13 MiB/s |
| **rANS32x32 16w                    14**   |  82.58 % |   13.91 clk/byte |   307.92 MiB/s |   2.45 clk/byte |  1749.16 MiB/s |
| **rANS32x32 16w                    15**   |  82.63 % |   14.20 clk/byte |   301.70 MiB/s |   2.59 clk/byte |  1654.49 MiB/s |
| TurboANX 2                                |  83.7  % |   -              |   600.46 MiB/s |   -             |  1292.65 MiB/s |
| fse                                       |  80.3  % |   -              |   696.88 MiB/s |   -             |   990.39 MiB/s |
| TurboANX 1                                |  85.1  % |   -              |   387.40 MiB/s |   -             |   719.84 MiB/s |
| rans32avx2 1                              |  74.4  % |   -              |   114.89 MiB/s |   -             |   229.78 MiB/s |
| rans32avx512 1                            |  74.4  % |   -              |   104.87 MiB/s |   -             |   220.91 MiB/s |
| FastHF                                    |  80.0  % |   -              |   183.35 MiB/s |   -             |   144.30 MiB/s |
| FastAC                                    |  79.7  % |   -              |   244.35 MiB/s |   -             |    77.33 MiB/s |
| arith_dyn 1                               |  67.6  % |   -              |    45.13 MiB/s |   -             |    45.67 MiB/s |
| arith_dyn 0                               |  79.6  % |   -              |    47.12 MiB/s |   -             |    45.40 MiB/s |

[mozilla](https://sun.aei.polsl.pl//~sdeor/index.php?page=silesia) (Tarred executables of Mozilla 1.0, Part of the Silesia Corpus)
| Codec Type | Ratio | Encoder<br/>Clocks/Byte | Encoder<br/>Throughput | Decoder<br/>Clocks/Byte | Decoder<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
| **rANS32x64 16w                    11**   |  77.82 % |   13.84 clk/byte |   309.39 MiB/s |   1.44 clk/byte |  2978.20 MiB/s |
| **rANS32x64 16w                    10**   |  77.92 % |   14.16 clk/byte |   302.46 MiB/s |   1.44 clk/byte |  2968.99 MiB/s |
| TurboANX 63                               |  70.1  % |   -              |   965.97 MiB/s |   -             |  2959.13 MiB/s |
| **rANS32x64 16w                    12**   |  77.79 % |   14.21 clk/byte |   301.44 MiB/s |   1.45 clk/byte |  2946.52 MiB/s |
| TurboANX 48                               |  69.6  % |   -              |   954.87 MiB/s |   -             |  2911.55 MiB/s |
| **rANS32x32 16w                    10**   |  77.92 % |   13.97 clk/byte |   306.54 MiB/s |   1.49 clk/byte |  2878.05 MiB/s |
| TurboANX 40                               |  69.3  % |   -              |   941.29 MiB/s |   -             |  2869.21 MiB/s |
| **rANS32x32 16w                    11**   |  77.82 % |   14.34 clk/byte |   298.79 MiB/s |   1.49 clk/byte |  2867.33 MiB/s |
| TurboANX 32                               |  68.9  % |   -              |   927.04 MiB/s |   -             |  2815.87 MiB/s |
| **rANS32x32 16w                    12**   |  77.79 % |   14.25 clk/byte |   300.51 MiB/s |   1.54 clk/byte |  2782.35 MiB/s |
| TurboANX 24                               |  68.4  % |   -              |   900.92 MiB/s |   -             |  2732.74 MiB/s |
| TurboANX 16                               |  67.9  % |   -              |   854.34 MiB/s |   -             |  2582.05 MiB/s |
| rans32avx2 0                              |  69.5  % |   -              |  1014.19 MiB/s |   -             |  2250.58 MiB/s |
| TurboANX 8                                |  67.2  % |   -              |   748.14 MiB/s |   -             |  2183.29 MiB/s |
| rans32avx512 0                            |  69.5  % |   -              |   760.33 MiB/s |   -             |  2115.31 MiB/s |
| fsehuf                                    |  69.2  % |   -              |  1491.60 MiB/s |   -             |  2092.00 MiB/s |
| **rANS32x32 16w                    14**   |  77.79 % |   14.02 clk/byte |   305.49 MiB/s |   2.37 clk/byte |  1804.10 MiB/s |
| **rANS32x64 16w                    14**   |  77.79 % |   14.09 clk/byte |   303.97 MiB/s |   2.26 clk/byte |  1891.46 MiB/s |
| rans32sse 0                               |  69.5  % |   -              |   724.39 MiB/s |   -             |  1884.40 MiB/s |
| **rANS32x64 16w                    13**   |  77.79 % |   13.89 clk/byte |   308.28 MiB/s |   2.27 clk/byte |  1883.91 MiB/s |
| **rANS32x64 16w                    15**   |  77.85 % |   13.86 clk/byte |   309.13 MiB/s |   2.31 clk/byte |  1855.74 MiB/s |
| **rANS32x32 16w                    13**   |  77.78 % |   14.13 clk/byte |   303.23 MiB/s |   2.37 clk/byte |  1806.03 MiB/s |
| **rANS32x32 16w                    15**   |  77.84 % |   14.29 clk/byte |   299.78 MiB/s |   2.46 clk/byte |  1743.60 MiB/s |
| TurboANX 4                                |  67.3  % |   -              |   603.91 MiB/s |   -             |  1658.68 MiB/s |
| TurboANX 2                                |  68.5  % |   -              |   556.95 MiB/s |   -             |  1106.06 MiB/s |
| fse                                       |  69.3  % |   -              |   713.08 MiB/s |   -             |   973.71 MiB/s |
| TurboANX 1                                |  71.6  % |   -              |   392.67 MiB/s |   -             |   677.10 MiB/s |
| rans32avx512 1                            |  55.7  % |   -              |    81.02 MiB/s |   -             |   168.42 MiB/s |
| rans32avx2 1                              |  55.7  % |   -              |    83.68 MiB/s |   -             |   167.19 MiB/s |
| FastHF                                    |  71.8  % |   -              |   174.86 MiB/s |   -             |   130.78 MiB/s |
| FastAC                                    |  70.7  % |   -              |   234.95 MiB/s |   -             |    81.01 MiB/s |
| arith_dyn 1                               |  52.1  % |   -              |    62.87 MiB/s |   -             |    62.98 MiB/s |
| arith_dyn 0                               |  66.4  % |   -              |    63.82 MiB/s |   -             |    59.92 MiB/s |

## License
Two Clause BSD
