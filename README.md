# rans_playground
Playground for rANS codecs

## [video-frame.raw](https://www.dropbox.com/s/yvsl1lg98c4maq1/video_frame.raw?dl=1) (heavily quantized video frame DCTs, 88,473,600 Bytes)

### MSVC
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average (StdDev.) | Maximum<br/>Throughput | Average (StdDev.) |
| -- | --: | --: | --: | --: | --: |
rANS32x32 multi block 15<br/>enc_basic                 |  12.58% |  7.031 clk/b |  7.063 clk/b<br/>(7.020 ~ 7.106) |   609.20 MiB/s |   606.42 MiB/s<br/>( 602.75 ~  610.15) |
rANS32x32 multi block 15<br/>dec_basic                 |         |  5.995 clk/b |  6.054 clk/b<br/>(6.025 ~ 6.083) |   714.45 MiB/s |   707.55 MiB/s<br/>( 704.19 ~  710.95) |
rANS32x32 multi block 15<br/>dec_avx2 (sym dpndt)      |         |  3.756 clk/b |  3.904 clk/b<br/>(3.360 ~ 4.449) |  1140.39 MiB/s |  1097.04 MiB/s<br/>( 962.87 ~ 1274.66) |
rANS32x32 multi block 15<br/>dec_avx2 (sym dpndt 2x)   |         |  3.586 clk/b |  3.730 clk/b<br/>(3.314 ~ 4.145) |  1194.42 MiB/s |  1148.45 MiB/s<br/>(1033.37 ~ 1292.39) |
rANS32x32 multi block 15<br/>dec_avx2 (sym indpt)      |         |  4.169 clk/b |  4.344 clk/b<br/>(3.800 ~ 4.888) |  1027.36 MiB/s |   986.12 MiB/s<br/>( 876.35 ~ 1127.32) |
rANS32x32 multi block 15<br/>dec_avx2 (sym indpt 2x)   |         |  3.971 clk/b |  4.005 clk/b<br/>(3.964 ~ 4.047) |  1078.68 MiB/s |  1069.46 MiB/s<br/>(1058.49 ~ 1080.66) |
rANS32x32 multi block 14<br/>enc_basic                 |  12.61% |  7.033 clk/b |  7.062 clk/b<br/>(7.043 ~ 7.082) |   609.04 MiB/s |   606.51 MiB/s<br/>( 604.84 ~  608.20) |
rANS32x32 multi block 14<br/>dec_basic                 |         |  5.950 clk/b |  6.034 clk/b<br/>(5.978 ~ 6.090) |   719.94 MiB/s |   709.88 MiB/s<br/>( 703.37 ~  716.50) |
rANS32x32 multi block 14<br/>dec_avx2 (sym dpndt)      |         |  3.683 clk/b |  3.695 clk/b<br/>(3.684 ~ 3.706) |  1163.09 MiB/s |  1159.24 MiB/s<br/>(1155.69 ~ 1162.81) |
rANS32x32 multi block 14<br/>dec_avx2 (sym dpndt 2x)   |         |  3.471 clk/b |  3.512 clk/b<br/>(3.448 ~ 3.575) |  1233.90 MiB/s |  1219.72 MiB/s<br/>(1198.05 ~ 1242.18) |
rANS32x32 multi block 14<br/>dec_avx2 (sym indpt)      |         |  3.949 clk/b |  3.964 clk/b<br/>(3.953 ~ 3.976) |  1084.66 MiB/s |  1080.54 MiB/s<br/>(1077.40 ~ 1083.69) |
rANS32x32 multi block 14<br/>dec_avx2 (sym indpt 2x)   |         |  3.783 clk/b |  3.813 clk/b<br/>(3.746 ~ 3.879) |  1132.15 MiB/s |  1123.46 MiB/s<br/>(1104.29 ~ 1143.30) |
rANS32x32 multi block 13<br/>enc_basic                 |  12.70% |  7.029 clk/b |  7.095 clk/b<br/>(7.017 ~ 7.172) |   609.39 MiB/s |   603.75 MiB/s<br/>( 597.24 ~  610.39) |
rANS32x32 multi block 13<br/>dec_basic                 |         |  5.924 clk/b |  5.998 clk/b<br/>(5.927 ~ 6.069) |   723.05 MiB/s |   714.13 MiB/s<br/>( 705.78 ~  722.67) |
rANS32x32 multi block 13<br/>dec_avx2 (sym dpndt)      |         |  3.689 clk/b |  3.819 clk/b<br/>(3.435 ~ 4.204) |  1161.23 MiB/s |  1121.54 MiB/s<br/>(1018.97 ~ 1247.08) |
rANS32x32 multi block 13<br/>dec_avx2 (sym dpndt 2x)   |         |  3.492 clk/b |  3.524 clk/b<br/>(3.460 ~ 3.588) |  1226.59 MiB/s |  1215.40 MiB/s<br/>(1193.67 ~ 1237.93) |
rANS32x32 multi block 13<br/>dec_avx2 (sym indpt)      |         |  3.757 clk/b |  3.916 clk/b<br/>(3.356 ~ 4.477) |  1140.16 MiB/s |  1093.70 MiB/s<br/>( 956.69 ~ 1276.51) |
rANS32x32 multi block 13<br/>dec_avx2 (sym indpt 2x)   |         |  3.619 clk/b |  3.636 clk/b<br/>(3.625 ~ 3.647) |  1183.47 MiB/s |  1178.12 MiB/s<br/>(1174.58 ~ 1181.69) |
rANS32x32 multi block 12<br/>enc_basic                 |  12.99% |  7.055 clk/b |  7.087 clk/b<br/>(7.067 ~ 7.108) |   607.15 MiB/s |   604.36 MiB/s<br/>( 602.58 ~  606.14) |
rANS32x32 multi block 12<br/>dec_basic                 |         |  5.969 clk/b |  6.035 clk/b<br/>(6.000 ~ 6.070) |   717.58 MiB/s |   709.76 MiB/s<br/>( 705.62 ~  713.94) |
rANS32x32 multi block 12<br/>dec_avx2 (sym dpndt)      |         |  3.683 clk/b |  3.748 clk/b<br/>(3.640 ~ 3.857) |  1162.93 MiB/s |  1142.78 MiB/s<br/>(1110.67 ~ 1176.80) |
rANS32x32 multi block 12<br/>dec_avx2 (sym dpndt 2x)   |         |  3.485 clk/b |  3.623 clk/b<br/>(3.214 ~ 4.032) |  1228.91 MiB/s |  1182.30 MiB/s<br/>(1062.43 ~ 1332.66) |
rANS32x32 multi block 12<br/>dec_avx2 (sym indpt)      |         |  3.663 clk/b |  3.821 clk/b<br/>(3.256 ~ 4.385) |  1169.22 MiB/s |  1121.05 MiB/s<br/>( 976.73 ~ 1315.43) |
rANS32x32 multi block 12<br/>dec_avx2 (sym indpt 2x)   |         |  3.553 clk/b |  3.593 clk/b<br/>(3.514 ~ 3.673) |  1205.48 MiB/s |  1192.04 MiB/s<br/>(1166.26 ~ 1218.98) |
rANS32x32 multi block 12<br/>dec_avx2 (sngl gathr)     |         |  2.618 clk/b |  2.634 clk/b<br/>(2.620 ~ 2.647) |  1636.36 MiB/s |  1626.22 MiB/s<br/>(1617.94 ~ 1634.59) |
rANS32x32 multi block 12<br/>dec_avx2 (sngl gathr 2x)  |         |  2.569 clk/b |  2.590 clk/b<br/>(2.579 ~ 2.600) |  1667.31 MiB/s |  1654.08 MiB/s<br/>(1647.36 ~ 1660.85) |
rANS32x32 multi block 11<br/>enc_basic                 |  13.73% |  7.110 clk/b |  7.133 clk/b<br/>(7.115 ~ 7.151) |   602.42 MiB/s |   600.50 MiB/s<br/>( 599.00 ~  602.01) |
rANS32x32 multi block 11<br/>dec_basic                 |         |  6.021 clk/b |  6.187 clk/b<br/>(5.723 ~ 6.651) |   711.36 MiB/s |   692.28 MiB/s<br/>( 644.00 ~  748.39) |
rANS32x32 multi block 11<br/>dec_avx2 (sym dpndt)      |         |  3.745 clk/b |  3.887 clk/b<br/>(3.381 ~ 4.392) |  1143.62 MiB/s |  1102.10 MiB/s<br/>( 975.23 ~ 1266.93) |
rANS32x32 multi block 11<br/>dec_avx2 (sym dpndt 2x)   |         |  3.507 clk/b |  3.591 clk/b<br/>(3.379 ~ 3.803) |  1221.21 MiB/s |  1192.70 MiB/s<br/>(1126.26 ~ 1267.47) |
rANS32x32 multi block 11<br/>dec_avx2 (sym indpt)      |         |  3.685 clk/b |  3.715 clk/b<br/>(3.697 ~ 3.733) |  1162.52 MiB/s |  1152.91 MiB/s<br/>(1147.36 ~ 1158.52) |
rANS32x32 multi block 11<br/>dec_avx2 (sym indpt 2x)   |         |  3.545 clk/b |  3.568 clk/b<br/>(3.553 ~ 3.583) |  1208.14 MiB/s |  1200.42 MiB/s<br/>(1195.33 ~ 1205.55) |
rANS32x32 multi block 11<br/>dec_avx2 (sngl gathr)     |         |  2.670 clk/b |  2.681 clk/b<br/>(2.674 ~ 2.689) |  1604.10 MiB/s |  1597.39 MiB/s<br/>(1593.11 ~ 1601.69) |
rANS32x32 multi block 11<br/>dec_avx2 (sngl gathr 2x)  |         |  2.568 clk/b |  2.607 clk/b<br/>(2.582 ~ 2.633) |  1667.95 MiB/s |  1642.82 MiB/s<br/>(1626.88 ~ 1659.08) |
rANS32x32 multi block 10<br/>enc_basic                 |  16.34% |  7.223 clk/b |  7.295 clk/b<br/>(7.230 ~ 7.359) |   593.02 MiB/s |   587.20 MiB/s<br/>( 582.02 ~  592.47) |
rANS32x32 multi block 10<br/>dec_basic                 |         |  6.119 clk/b |  6.177 clk/b<br/>(6.107 ~ 6.247) |   700.04 MiB/s |   693.43 MiB/s<br/>( 685.66 ~  701.37) |
rANS32x32 multi block 10<br/>dec_avx2 (sym dpndt)      |         |  3.738 clk/b |  3.842 clk/b<br/>(3.527 ~ 4.157) |  1145.90 MiB/s |  1114.89 MiB/s<br/>(1030.48 ~ 1214.36) |
rANS32x32 multi block 10<br/>dec_avx2 (sym dpndt 2x)   |         |  3.526 clk/b |  3.547 clk/b<br/>(3.531 ~ 3.562) |  1214.68 MiB/s |  1207.68 MiB/s<br/>(1202.35 ~ 1213.06) |
rANS32x32 multi block 10<br/>dec_avx2 (sym indpt)      |         |  3.716 clk/b |  3.744 clk/b<br/>(3.727 ~ 3.761) |  1152.62 MiB/s |  1144.11 MiB/s<br/>(1138.91 ~ 1149.36) |
rANS32x32 multi block 10<br/>dec_avx2 (sym indpt 2x)   |         |  3.576 clk/b |  3.602 clk/b<br/>(3.583 ~ 3.621) |  1197.83 MiB/s |  1189.28 MiB/s<br/>(1183.05 ~ 1195.58) |
rANS32x32 multi block 10<br/>dec_avx2 (sngl gathr)     |         |  2.690 clk/b |  2.953 clk/b<br/>(2.365 ~ 3.542) |  1592.17 MiB/s |  1450.28 MiB/s<br/>(1209.28 ~ 1811.24) |
rANS32x32 multi block 10<br/>dec_avx2 (sngl gathr 2x)  |         |  2.576 clk/b |  2.676 clk/b<br/>(2.397 ~ 2.954) |  1662.92 MiB/s |  1600.89 MiB/s<br/>(1449.93 ~ 1786.95) |

### Clang (via WSL2)
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average (StdDev.) | Maximum<br/>Throughput | Average (StdDev.) |
| -- | --: | --: | --: | --: | --: |
rANS32x32 multi block 15<br/>enc_basic                 |  12.58% |  6.909 clk/b |  6.959 clk/b<br/>(6.927 ~ 6.990) |   619.97 MiB/s |   615.55 MiB/s<br/>( 612.79 ~  618.35) |
rANS32x32 multi block 15<br/>dec_basic                 |         |  6.368 clk/b |  6.409 clk/b<br/>(6.380 ~ 6.439) |   672.68 MiB/s |   668.31 MiB/s<br/>( 665.23 ~  671.42) |
rANS32x32 multi block 15<br/>dec_avx2 (sym dpndt)      |         |  3.367 clk/b |  3.393 clk/b<br/>(3.371 ~ 3.415) |  1272.22 MiB/s |  1262.44 MiB/s<br/>(1254.17 ~ 1270.81) |
rANS32x32 multi block 15<br/>dec_avx2 (sym dpndt 2x)   |         |  3.196 clk/b |  3.224 clk/b<br/>(3.208 ~ 3.240) |  1340.24 MiB/s |  1328.71 MiB/s<br/>(1322.08 ~ 1335.40) |
rANS32x32 multi block 15<br/>dec_avx2 (sym indpt)      |         |  3.863 clk/b |  3.894 clk/b<br/>(3.879 ~ 3.909) |  1108.87 MiB/s |  1100.08 MiB/s<br/>(1095.86 ~ 1104.33) |
rANS32x32 multi block 15<br/>dec_avx2 (sym indpt 2x)   |         |  3.889 clk/b |  3.911 clk/b<br/>(3.898 ~ 3.925) |  1101.54 MiB/s |  1095.12 MiB/s<br/>(1091.33 ~ 1098.94) |
rANS32x32 multi block 14<br/>enc_basic                 |  12.61% |  7.057 clk/b |  7.101 clk/b<br/>(7.077 ~ 7.125) |   606.95 MiB/s |   603.20 MiB/s<br/>( 601.13 ~  605.28) |
rANS32x32 multi block 14<br/>dec_basic                 |         |  6.281 clk/b |  6.354 clk/b<br/>(6.311 ~ 6.396) |   681.96 MiB/s |   674.16 MiB/s<br/>( 669.66 ~  678.72) |
rANS32x32 multi block 14<br/>dec_avx2 (sym dpndt)      |         |  3.299 clk/b |  3.323 clk/b<br/>(3.305 ~ 3.340) |  1298.26 MiB/s |  1289.14 MiB/s<br/>(1282.31 ~ 1296.05) |
rANS32x32 multi block 14<br/>dec_avx2 (sym dpndt 2x)   |         |  3.136 clk/b |  3.163 clk/b<br/>(3.141 ~ 3.185) |  1365.72 MiB/s |  1354.00 MiB/s<br/>(1344.62 ~ 1363.52) |
rANS32x32 multi block 14<br/>dec_avx2 (sym indpt)      |         |  3.638 clk/b |  3.658 clk/b<br/>(3.643 ~ 3.672) |  1177.45 MiB/s |  1171.07 MiB/s<br/>(1166.45 ~ 1175.73) |
rANS32x32 multi block 14<br/>dec_avx2 (sym indpt 2x)   |         |  3.658 clk/b |  3.692 clk/b<br/>(3.669 ~ 3.714) |  1171.03 MiB/s |  1160.28 MiB/s<br/>(1153.21 ~ 1167.44) |
rANS32x32 multi block 13<br/>enc_basic                 |  12.70% |  6.968 clk/b |  7.007 clk/b<br/>(6.980 ~ 7.035) |   614.76 MiB/s |   611.26 MiB/s<br/>( 608.87 ~  613.68) |
rANS32x32 multi block 13<br/>dec_basic                 |         |  6.317 clk/b |  6.389 clk/b<br/>(6.337 ~ 6.440) |   678.11 MiB/s |   670.45 MiB/s<br/>( 665.07 ~  675.91) |
rANS32x32 multi block 13<br/>dec_avx2 (sym dpndt)      |         |  3.316 clk/b |  3.353 clk/b<br/>(3.333 ~ 3.372) |  1291.86 MiB/s |  1277.61 MiB/s<br/>(1270.23 ~ 1285.09) |
rANS32x32 multi block 13<br/>dec_avx2 (sym dpndt 2x)   |         |  3.140 clk/b |  3.167 clk/b<br/>(3.153 ~ 3.180) |  1363.88 MiB/s |  1352.64 MiB/s<br/>(1346.90 ~ 1358.43) |
rANS32x32 multi block 13<br/>dec_avx2 (sym indpt)      |         |  3.460 clk/b |  3.481 clk/b<br/>(3.467 ~ 3.495) |  1237.94 MiB/s |  1230.49 MiB/s<br/>(1225.44 ~ 1235.59) |
rANS32x32 multi block 13<br/>dec_avx2 (sym indpt 2x)   |         |  3.477 clk/b |  3.497 clk/b<br/>(3.486 ~ 3.508) |  1231.98 MiB/s |  1224.81 MiB/s<br/>(1220.87 ~ 1228.77) |
rANS32x32 multi block 12<br/>enc_basic                 |  12.99% |  7.120 clk/b |  7.177 clk/b<br/>(7.133 ~ 7.221) |   601.63 MiB/s |   596.82 MiB/s<br/>( 593.18 ~  600.50) |
rANS32x32 multi block 12<br/>dec_basic                 |         |  6.258 clk/b |  6.324 clk/b<br/>(6.282 ~ 6.366) |   684.47 MiB/s |   677.34 MiB/s<br/>( 672.87 ~  681.88) |
rANS32x32 multi block 12<br/>dec_avx2 (sym dpndt)      |         |  3.327 clk/b |  3.358 clk/b<br/>(3.342 ~ 3.374) |  1287.58 MiB/s |  1275.50 MiB/s<br/>(1269.36 ~ 1281.69) |
rANS32x32 multi block 12<br/>dec_avx2 (sym dpndt 2x)   |         |  3.133 clk/b |  3.161 clk/b<br/>(3.143 ~ 3.179) |  1367.14 MiB/s |  1355.06 MiB/s<br/>(1347.42 ~ 1362.78) |
rANS32x32 multi block 12<br/>dec_avx2 (sym indpt)      |         |  3.357 clk/b |  3.382 clk/b<br/>(3.366 ~ 3.398) |  1275.89 MiB/s |  1266.49 MiB/s<br/>(1260.61 ~ 1272.42) |
rANS32x32 multi block 12<br/>dec_avx2 (sym indpt 2x)   |         |  3.227 clk/b |  3.249 clk/b<br/>(3.232 ~ 3.266) |  1327.18 MiB/s |  1318.35 MiB/s<br/>(1311.67 ~ 1325.09) |
rANS32x32 multi block 12<br/>dec_avx2 (sngl gathr)     |         |  2.517 clk/b |  2.533 clk/b<br/>(2.520 ~ 2.545) |  1701.49 MiB/s |  1691.29 MiB/s<br/>(1683.09 ~ 1699.57) |
rANS32x32 multi block 12<br/>dec_avx2 (sngl gathr 2x)  |         |  2.461 clk/b |  2.477 clk/b<br/>(2.464 ~ 2.489) |  1740.33 MiB/s |  1729.48 MiB/s<br/>(1720.92 ~ 1738.13) |
rANS32x32 multi block 11<br/>enc_basic                 |  13.73% |  7.144 clk/b |  7.187 clk/b<br/>(7.166 ~ 7.207) |   599.58 MiB/s |   596.02 MiB/s<br/>( 594.33 ~  597.71) |
rANS32x32 multi block 11<br/>dec_basic                 |         |  6.314 clk/b |  6.369 clk/b<br/>(6.335 ~ 6.404) |   678.43 MiB/s |   672.49 MiB/s<br/>( 668.87 ~  676.16) |
rANS32x32 multi block 11<br/>dec_avx2 (sym dpndt)      |         |  3.315 clk/b |  3.358 clk/b<br/>(3.329 ~ 3.388) |  1292.24 MiB/s |  1275.44 MiB/s<br/>(1264.44 ~ 1286.62) |
rANS32x32 multi block 11<br/>dec_avx2 (sym dpndt 2x)   |         |  3.155 clk/b |  3.186 clk/b<br/>(3.160 ~ 3.211) |  1357.67 MiB/s |  1344.61 MiB/s<br/>(1333.94 ~ 1355.46) |
rANS32x32 multi block 11<br/>dec_avx2 (sym indpt)      |         |  3.360 clk/b |  3.386 clk/b<br/>(3.365 ~ 3.407) |  1274.74 MiB/s |  1265.03 MiB/s<br/>(1257.11 ~ 1273.06) |
rANS32x32 multi block 11<br/>dec_avx2 (sym indpt 2x)   |         |  3.214 clk/b |  3.239 clk/b<br/>(3.224 ~ 3.254) |  1332.64 MiB/s |  1322.39 MiB/s<br/>(1316.18 ~ 1328.66) |
rANS32x32 multi block 11<br/>dec_avx2 (sngl gathr)     |         |  2.491 clk/b |  2.525 clk/b<br/>(2.509 ~ 2.542) |  1719.58 MiB/s |  1696.05 MiB/s<br/>(1684.87 ~ 1707.37) |
rANS32x32 multi block 11<br/>dec_avx2 (sngl gathr 2x)  |         |  2.433 clk/b |  2.451 clk/b<br/>(2.436 ~ 2.465) |  1760.30 MiB/s |  1747.73 MiB/s<br/>(1737.30 ~ 1758.29) |
rANS32x32 multi block 10<br/>enc_basic                 |  16.34% |  7.219 clk/b |  7.292 clk/b<br/>(7.243 ~ 7.342) |   593.32 MiB/s |   587.37 MiB/s<br/>( 583.42 ~  591.37) |
rANS32x32 multi block 10<br/>dec_basic                 |         |  6.411 clk/b |  6.607 clk/b<br/>(6.440 ~ 6.774) |   668.10 MiB/s |   648.32 MiB/s<br/>( 632.32 ~  665.16) |
rANS32x32 multi block 10<br/>dec_avx2 (sym dpndt)      |         |  3.343 clk/b |  3.398 clk/b<br/>(3.364 ~ 3.433) |  1281.41 MiB/s |  1260.46 MiB/s<br/>(1247.86 ~ 1273.33) |
rANS32x32 multi block 10<br/>dec_avx2 (sym dpndt 2x)   |         |  3.131 clk/b |  3.178 clk/b<br/>(3.151 ~ 3.205) |  1368.06 MiB/s |  1347.76 MiB/s<br/>(1336.33 ~ 1359.39) |
rANS32x32 multi block 10<br/>dec_avx2 (sym indpt)      |         |  3.375 clk/b |  3.396 clk/b<br/>(3.384 ~ 3.408) |  1269.15 MiB/s |  1261.39 MiB/s<br/>(1256.89 ~ 1265.92) |
rANS32x32 multi block 10<br/>dec_avx2 (sym indpt 2x)   |         |  3.221 clk/b |  3.245 clk/b<br/>(3.229 ~ 3.262) |  1329.62 MiB/s |  1319.77 MiB/s<br/>(1313.17 ~ 1326.43) |
rANS32x32 multi block 10<br/>dec_avx2 (sngl gathr)     |         |  2.517 clk/b |  2.541 clk/b<br/>(2.528 ~ 2.553) |  1701.48 MiB/s |  1685.87 MiB/s<br/>(1677.76 ~ 1694.07) |
rANS32x32 multi block 10<br/>dec_avx2 (sngl gathr 2x)  |         |  2.439 clk/b |  2.472 clk/b<br/>(2.449 ~ 2.494) |  1755.90 MiB/s |  1732.94 MiB/s<br/>(1717.17 ~ 1749.00) |

## License
Two Clause BSD
