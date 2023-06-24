# rans_playground
Playground for rANS codecs

## [video-frame.raw](https://www.dropbox.com/s/yvsl1lg98c4maq1/video_frame.raw?dl=1) (heavily quantized video frame DCTs, 88,473,600 Bytes)

### MSVC
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 32blk 15<br/>enc basic                 |  12.58 % |  7.031 clk/b |  7.063 clk/b |   609.20 MiB/s |   606.42 MiB/s |
rANS32x32 32blk 15<br/>dec basic                 |          |  5.995 clk/b |  6.054 clk/b |   714.45 MiB/s |   707.55 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym dpndt)      |          |  3.756 clk/b |  3.904 clk/b |  1140.39 MiB/s |  1097.04 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym dpndt 2x)   |          |  3.586 clk/b |  3.730 clk/b |  1194.42 MiB/s |  1148.45 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym indpt)      |          |  4.169 clk/b |  4.344 clk/b |  1027.36 MiB/s |   986.12 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym indpt 2x)   |          |  3.971 clk/b |  4.005 clk/b |  1078.68 MiB/s |  1069.46 MiB/s |
rANS32x32 32blk 14<br/>enc basic                 |  12.61 % |  7.033 clk/b |  7.062 clk/b |   609.04 MiB/s |   606.51 MiB/s |
rANS32x32 32blk 14<br/>dec basic                 |          |  5.950 clk/b |  6.034 clk/b |   719.94 MiB/s |   709.88 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym dpndt)      |          |  3.683 clk/b |  3.695 clk/b |  1163.09 MiB/s |  1159.24 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym dpndt 2x)   |          |  3.471 clk/b |  3.512 clk/b |  1233.90 MiB/s |  1219.72 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym indpt)      |          |  3.949 clk/b |  3.964 clk/b |  1084.66 MiB/s |  1080.54 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym indpt 2x)   |          |  3.783 clk/b |  3.813 clk/b |  1132.15 MiB/s |  1123.46 MiB/s |
rANS32x32 32blk 13<br/>enc basic                 |  12.70 % |  7.029 clk/b |  7.095 clk/b |   609.39 MiB/s |   603.75 MiB/s |
rANS32x32 32blk 13<br/>dec basic                 |          |  5.924 clk/b |  5.998 clk/b |   723.05 MiB/s |   714.13 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym dpndt)      |          |  3.689 clk/b |  3.819 clk/b |  1161.23 MiB/s |  1121.54 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym dpndt 2x)   |          |  3.492 clk/b |  3.524 clk/b |  1226.59 MiB/s |  1215.40 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym indpt)      |          |  3.757 clk/b |  3.916 clk/b |  1140.16 MiB/s |  1093.70 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym indpt 2x)   |          |  3.619 clk/b |  3.636 clk/b |  1183.47 MiB/s |  1178.12 MiB/s |
rANS32x32 32blk 12<br/>enc basic                 |  12.99 % |  7.055 clk/b |  7.087 clk/b |   607.15 MiB/s |   604.36 MiB/s |
rANS32x32 32blk 12<br/>dec basic                 |          |  5.969 clk/b |  6.035 clk/b |   717.58 MiB/s |   709.76 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym dpndt)      |          |  3.683 clk/b |  3.748 clk/b |  1162.93 MiB/s |  1142.78 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym dpndt 2x)   |          |  3.485 clk/b |  3.623 clk/b |  1228.91 MiB/s |  1182.30 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym indpt)      |          |  3.663 clk/b |  3.821 clk/b |  1169.22 MiB/s |  1121.05 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym indpt 2x)   |          |  3.553 clk/b |  3.593 clk/b |  1205.48 MiB/s |  1192.04 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (1gathr)         |          |  2.618 clk/b |  2.634 clk/b |  1636.36 MiB/s |  1626.22 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (1gathr 2x)      |          |  2.569 clk/b |  2.590 clk/b |  1667.31 MiB/s |  1654.08 MiB/s |
rANS32x32 32blk 11<br/>enc basic                 |  13.73 % |  7.110 clk/b |  7.133 clk/b |   602.42 MiB/s |   600.50 MiB/s |
rANS32x32 32blk 11<br/>dec basic                 |          |  6.021 clk/b |  6.187 clk/b |   711.36 MiB/s |   692.28 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym dpndt)      |          |  3.745 clk/b |  3.887 clk/b |  1143.62 MiB/s |  1102.10 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym dpndt 2x)   |          |  3.507 clk/b |  3.591 clk/b |  1221.21 MiB/s |  1192.70 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym indpt)      |          |  3.685 clk/b |  3.715 clk/b |  1162.52 MiB/s |  1152.91 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym indpt 2x)   |          |  3.545 clk/b |  3.568 clk/b |  1208.14 MiB/s |  1200.42 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (1gathr)         |          |  2.670 clk/b |  2.681 clk/b |  1604.10 MiB/s |  1597.39 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (1gathr 2x)      |          |  2.568 clk/b |  2.607 clk/b |  1667.95 MiB/s |  1642.82 MiB/s |
rANS32x32 32blk 10<br/>enc basic                 |  16.34 % |  7.223 clk/b |  7.295 clk/b |   593.02 MiB/s |   587.20 MiB/s |
rANS32x32 32blk 10<br/>dec basic                 |          |  6.119 clk/b |  6.177 clk/b |   700.04 MiB/s |   693.43 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym dpndt)      |          |  3.738 clk/b |  3.842 clk/b |  1145.90 MiB/s |  1114.89 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym dpndt 2x)   |          |  3.526 clk/b |  3.547 clk/b |  1214.68 MiB/s |  1207.68 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym indpt)      |          |  3.716 clk/b |  3.744 clk/b |  1152.62 MiB/s |  1144.11 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym indpt 2x)   |          |  3.576 clk/b |  3.602 clk/b |  1197.83 MiB/s |  1189.28 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (1gathr)         |          |  2.690 clk/b |  2.953 clk/b |  1592.17 MiB/s |  1450.28 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (1gathr 2x)      |          |  2.576 clk/b |  2.676 clk/b |  1662.92 MiB/s |  1600.89 MiB/s |

### Clang (via WSL2)
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 32blk 15<br/>enc basic                 |  12.58 % |  6.909 clk/b |  6.959 clk/b |   619.97 MiB/s |   615.55 MiB/s |
rANS32x32 32blk 15<br/>dec basic                 |          |  6.368 clk/b |  6.409 clk/b |   672.68 MiB/s |   668.31 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym dpndt)      |          |  3.367 clk/b |  3.393 clk/b |  1272.22 MiB/s |  1262.44 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym dpndt 2x)   |          |  3.196 clk/b |  3.224 clk/b |  1340.24 MiB/s |  1328.71 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym indpt)      |          |  3.863 clk/b |  3.894 clk/b |  1108.87 MiB/s |  1100.08 MiB/s |
rANS32x32 32blk 15<br/>dec avx2 (sym indpt 2x)   |          |  3.889 clk/b |  3.911 clk/b |  1101.54 MiB/s |  1095.12 MiB/s |
rANS32x32 32blk 14<br/>enc basic                 |  12.61 % |  7.057 clk/b |  7.101 clk/b |   606.95 MiB/s |   603.20 MiB/s |
rANS32x32 32blk 14<br/>dec basic                 |          |  6.281 clk/b |  6.354 clk/b |   681.96 MiB/s |   674.16 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym dpndt)      |          |  3.299 clk/b |  3.323 clk/b |  1298.26 MiB/s |  1289.14 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym dpndt 2x)   |          |  3.136 clk/b |  3.163 clk/b |  1365.72 MiB/s |  1354.00 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym indpt)      |          |  3.638 clk/b |  3.658 clk/b |  1177.45 MiB/s |  1171.07 MiB/s |
rANS32x32 32blk 14<br/>dec avx2 (sym indpt 2x)   |          |  3.658 clk/b |  3.692 clk/b |  1171.03 MiB/s |  1160.28 MiB/s |
rANS32x32 32blk 13<br/>enc basic                 |  12.70 % |  6.968 clk/b |  7.007 clk/b |   614.76 MiB/s |   611.26 MiB/s |
rANS32x32 32blk 13<br/>dec basic                 |          |  6.317 clk/b |  6.389 clk/b |   678.11 MiB/s |   670.45 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym dpndt)      |          |  3.316 clk/b |  3.353 clk/b |  1291.86 MiB/s |  1277.61 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym dpndt 2x)   |          |  3.140 clk/b |  3.167 clk/b |  1363.88 MiB/s |  1352.64 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym indpt)      |          |  3.460 clk/b |  3.481 clk/b |  1237.94 MiB/s |  1230.49 MiB/s |
rANS32x32 32blk 13<br/>dec avx2 (sym indpt 2x)   |          |  3.477 clk/b |  3.497 clk/b |  1231.98 MiB/s |  1224.81 MiB/s |
rANS32x32 32blk 12<br/>enc basic                 |  12.99 % |  7.120 clk/b |  7.177 clk/b |   601.63 MiB/s |   596.82 MiB/s |
rANS32x32 32blk 12<br/>dec basic                 |          |  6.258 clk/b |  6.324 clk/b |   684.47 MiB/s |   677.34 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym dpndt)      |          |  3.327 clk/b |  3.358 clk/b |  1287.58 MiB/s |  1275.50 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym dpndt 2x)   |          |  3.133 clk/b |  3.161 clk/b |  1367.14 MiB/s |  1355.06 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym indpt)      |          |  3.357 clk/b |  3.382 clk/b |  1275.89 MiB/s |  1266.49 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (sym indpt 2x)   |          |  3.227 clk/b |  3.249 clk/b |  1327.18 MiB/s |  1318.35 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (1gathr)         |          |  2.517 clk/b |  2.533 clk/b |  1701.49 MiB/s |  1691.29 MiB/s |
rANS32x32 32blk 12<br/>dec avx2 (1gathr 2x)      |          |  2.461 clk/b |  2.477 clk/b |  1740.33 MiB/s |  1729.48 MiB/s |
rANS32x32 32blk 11<br/>enc basic                 |  13.73 % |  7.144 clk/b |  7.187 clk/b |   599.58 MiB/s |   596.02 MiB/s |
rANS32x32 32blk 11<br/>dec basic                 |          |  6.314 clk/b |  6.369 clk/b |   678.43 MiB/s |   672.49 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym dpndt)      |          |  3.315 clk/b |  3.358 clk/b |  1292.24 MiB/s |  1275.44 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym dpndt 2x)   |          |  3.155 clk/b |  3.186 clk/b |  1357.67 MiB/s |  1344.61 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym indpt)      |          |  3.360 clk/b |  3.386 clk/b |  1274.74 MiB/s |  1265.03 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (sym indpt 2x)   |          |  3.214 clk/b |  3.239 clk/b |  1332.64 MiB/s |  1322.39 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (1gathr)         |          |  2.491 clk/b |  2.525 clk/b |  1719.58 MiB/s |  1696.05 MiB/s |
rANS32x32 32blk 11<br/>dec avx2 (1gathr 2x)      |          |  2.433 clk/b |  2.451 clk/b |  1760.30 MiB/s |  1747.73 MiB/s |
rANS32x32 32blk 10<br/>enc basic                 |  16.34 % |  7.219 clk/b |  7.292 clk/b |   593.32 MiB/s |   587.37 MiB/s |
rANS32x32 32blk 10<br/>dec basic                 |          |  6.411 clk/b |  6.607 clk/b |   668.10 MiB/s |   648.32 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym dpndt)      |          |  3.343 clk/b |  3.398 clk/b |  1281.41 MiB/s |  1260.46 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym dpndt 2x)   |          |  3.131 clk/b |  3.178 clk/b |  1368.06 MiB/s |  1347.76 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym indpt)      |          |  3.375 clk/b |  3.396 clk/b |  1269.15 MiB/s |  1261.39 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (sym indpt 2x)   |          |  3.221 clk/b |  3.245 clk/b |  1329.62 MiB/s |  1319.77 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (1gathr)         |          |  2.517 clk/b |  2.541 clk/b |  1701.48 MiB/s |  1685.87 MiB/s |
rANS32x32 32blk 10<br/>dec avx2 (1gathr 2x)      |          |  2.439 clk/b |  2.472 clk/b |  1755.90 MiB/s |  1732.94 MiB/s |

## License
Two Clause BSD
