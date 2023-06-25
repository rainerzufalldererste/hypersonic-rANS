# rans_playground
Playground for rANS codecs

## [video-frame.raw](https://www.dropbox.com/s/yvsl1lg98c4maq1/video_frame.raw?dl=1) (heavily quantized video frame DCTs, 88,473,600 Bytes)

### MSVC
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 16w                    15 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.59 % |    6.02 clk/byte |    6.04 clk/byte |   711.68 MiB/s |   708.78 MiB/s |
  dec scalar                        |          |    5.00 clk/byte |    5.05 clk/byte |   857.41 MiB/s |   848.00 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.74 clk/byte |    2.90 clk/byte |  1562.62 MiB/s |  1478.41 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.75 clk/byte |    2.77 clk/byte |  1555.90 MiB/s |  1544.89 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.90 clk/byte |    3.35 clk/byte |  1477.40 MiB/s |  1279.28 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.91 clk/byte |    3.08 clk/byte |  1471.63 MiB/s |  1390.63 MiB/s |
rANS32x32 16w                    14 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.62 % |    6.00 clk/byte |    6.01 clk/byte |   713.71 MiB/s |   712.19 MiB/s |
  dec scalar                        |          |    4.94 clk/byte |    5.03 clk/byte |   867.88 MiB/s |   851.26 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.66 clk/byte |    2.70 clk/byte |  1608.63 MiB/s |  1584.32 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.70 clk/byte |    2.71 clk/byte |  1585.90 MiB/s |  1580.90 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.82 clk/byte |    2.85 clk/byte |  1516.98 MiB/s |  1504.45 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.86 clk/byte |    2.87 clk/byte |  1500.11 MiB/s |  1492.37 MiB/s |
rANS32x32 16w                    13 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.71 % |    6.06 clk/byte |    6.09 clk/byte |   706.52 MiB/s |   703.29 MiB/s |
  dec scalar                        |          |    4.95 clk/byte |    4.99 clk/byte |   865.87 MiB/s |   857.83 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.66 clk/byte |    2.67 clk/byte |  1610.93 MiB/s |  1601.97 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.65 clk/byte |    2.66 clk/byte |  1618.78 MiB/s |  1608.79 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.81 clk/byte |    2.86 clk/byte |  1526.90 MiB/s |  1500.27 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.80 clk/byte |    2.91 clk/byte |  1531.92 MiB/s |  1471.59 MiB/s |
rANS32x32 16w                    12 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.99 % |    6.11 clk/byte |    6.14 clk/byte |   701.21 MiB/s |   697.96 MiB/s |
  dec scalar                        |          |    5.00 clk/byte |    5.10 clk/byte |   857.20 MiB/s |   840.55 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.65 clk/byte |    2.67 clk/byte |  1617.08 MiB/s |  1604.64 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.63 clk/byte |    2.65 clk/byte |  1630.96 MiB/s |  1619.16 MiB/s |
  dec avx2 (xmm shfl, sngl gthr)    |          |    1.65 clk/byte |    1.67 clk/byte |  2591.22 MiB/s |  2572.25 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.81 clk/byte |    2.82 clk/byte |  1523.76 MiB/s |  1519.52 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.76 clk/byte |    2.78 clk/byte |  1553.05 MiB/s |  1541.40 MiB/s |
  dec avx2 (ymm perm, sngl gthr)    |          |    1.79 clk/byte |    1.81 clk/byte |  2398.34 MiB/s |  2372.90 MiB/s |
rANS32x32 16w                    11 | -        | -                | -                | -              | -              |
  enc scalar                        |  13.73 % |    6.22 clk/byte |    6.23 clk/byte |   689.12 MiB/s |   687.10 MiB/s |
  dec scalar                        |          |    5.14 clk/byte |    5.21 clk/byte |   833.91 MiB/s |   822.10 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.65 clk/byte |    2.67 clk/byte |  1615.87 MiB/s |  1603.74 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.63 clk/byte |    2.65 clk/byte |  1627.51 MiB/s |  1617.55 MiB/s |
  dec avx2 (xmm shfl, sngl gthr)    |          |    1.65 clk/byte |    1.66 clk/byte |  2597.10 MiB/s |  2578.64 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.81 clk/byte |    2.84 clk/byte |  1522.60 MiB/s |  1509.20 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.77 clk/byte |    2.93 clk/byte |  1546.87 MiB/s |  1460.50 MiB/s |
  dec avx2 (ymm perm, sngl gthr)    |          |    1.77 clk/byte |    1.91 clk/byte |  2425.24 MiB/s |  2247.00 MiB/s |
rANS32x32 16w                    10 | -        | -                | -                | -              | -              |
  enc scalar                        |  16.34 % |    6.45 clk/byte |    6.50 clk/byte |   664.30 MiB/s |   659.09 MiB/s |
  dec scalar                        |          |    5.50 clk/byte |    5.61 clk/byte |   778.14 MiB/s |   763.15 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.65 clk/byte |    2.67 clk/byte |  1617.72 MiB/s |  1606.08 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.62 clk/byte |    2.64 clk/byte |  1633.63 MiB/s |  1620.95 MiB/s |
  dec avx2 (xmm shfl, sngl gthr)    |          |    1.65 clk/byte |    1.66 clk/byte |  2603.23 MiB/s |  2583.94 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.80 clk/byte |    2.82 clk/byte |  1527.48 MiB/s |  1519.57 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.76 clk/byte |    2.78 clk/byte |  1550.46 MiB/s |  1540.54 MiB/s |
  dec avx2 (ymm perm, sngl gthr)    |          |    1.76 clk/byte |    1.77 clk/byte |  2428.48 MiB/s |  2415.10 MiB/s |
rANS32x32 32blk 16w              15 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.59 % |    6.09 clk/byte |    6.12 clk/byte |   703.70 MiB/s |   699.65 MiB/s |
  dec scalar                        |          |    5.11 clk/byte |    5.16 clk/byte |   838.22 MiB/s |   830.73 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.25 clk/byte |    3.27 clk/byte |  1319.57 MiB/s |  1310.73 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.09 clk/byte |    3.11 clk/byte |  1385.30 MiB/s |  1377.92 MiB/s |
  dec avx2 (sym indpt)              |          |    3.60 clk/byte |    4.03 clk/byte |  1191.36 MiB/s |  1064.05 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.15 clk/byte |    3.57 clk/byte |  1360.35 MiB/s |  1200.21 MiB/s |
rANS32x32 32blk 16w              14 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.62 % |    6.10 clk/byte |    6.21 clk/byte |   701.79 MiB/s |   689.93 MiB/s |
  dec scalar                        |          |    5.04 clk/byte |    5.10 clk/byte |   849.45 MiB/s |   840.09 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.32 clk/byte |    3.34 clk/byte |  1291.96 MiB/s |  1283.41 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.04 clk/byte |    3.06 clk/byte |  1407.92 MiB/s |  1398.87 MiB/s |
  dec avx2 (sym indpt)              |          |    3.54 clk/byte |    3.64 clk/byte |  1209.34 MiB/s |  1177.71 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.11 clk/byte |    3.12 clk/byte |  1376.56 MiB/s |  1372.34 MiB/s |
rANS32x32 32blk 16w              13 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.71 % |    6.13 clk/byte |    6.15 clk/byte |   698.88 MiB/s |   696.46 MiB/s |
  dec scalar                        |          |    5.10 clk/byte |    5.21 clk/byte |   840.53 MiB/s |   822.17 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.29 clk/byte |    3.35 clk/byte |  1303.08 MiB/s |  1278.99 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.03 clk/byte |    3.05 clk/byte |  1413.32 MiB/s |  1403.43 MiB/s |
  dec avx2 (sym indpt)              |          |    3.46 clk/byte |    3.48 clk/byte |  1237.03 MiB/s |  1229.79 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.09 clk/byte |    3.18 clk/byte |  1387.87 MiB/s |  1348.45 MiB/s |
rANS32x32 32blk 16w              12 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.99 % |    6.16 clk/byte |    6.35 clk/byte |   695.90 MiB/s |   674.49 MiB/s |
  dec scalar                        |          |    5.06 clk/byte |    5.42 clk/byte |   845.79 MiB/s |   790.17 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.27 clk/byte |    3.86 clk/byte |  1310.82 MiB/s |  1109.54 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.05 clk/byte |    3.06 clk/byte |  1404.04 MiB/s |  1400.00 MiB/s |
  dec avx2 (sym indpt)              |          |    3.40 clk/byte |    3.41 clk/byte |  1258.61 MiB/s |  1255.15 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.04 clk/byte |    3.06 clk/byte |  1407.28 MiB/s |  1397.94 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.27 clk/byte |    2.40 clk/byte |  1886.56 MiB/s |  1781.66 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.03 clk/byte |    2.04 clk/byte |  2108.38 MiB/s |  2100.30 MiB/s |
rANS32x32 32blk 16w              11 | -        | -                | -                | -              | -              |
  enc scalar                        |  13.73 % |    6.28 clk/byte |    6.32 clk/byte |   682.25 MiB/s |   677.77 MiB/s |
  dec scalar                        |          |    5.21 clk/byte |    5.30 clk/byte |   822.89 MiB/s |   808.17 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.24 clk/byte |    3.26 clk/byte |  1323.12 MiB/s |  1313.29 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.02 clk/byte |    3.04 clk/byte |  1420.62 MiB/s |  1410.46 MiB/s |
  dec avx2 (sym indpt)              |          |    3.41 clk/byte |    3.43 clk/byte |  1257.49 MiB/s |  1250.61 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.05 clk/byte |    3.07 clk/byte |  1402.78 MiB/s |  1395.31 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.31 clk/byte |    2.69 clk/byte |  1851.15 MiB/s |  1594.12 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.02 clk/byte |    2.31 clk/byte |  2121.31 MiB/s |  1853.50 MiB/s |
rANS32x32 32blk 16w              10 | -        | -                | -                | -              | -              |
  enc scalar                        |  16.34 % |    6.58 clk/byte |    6.82 clk/byte |   651.32 MiB/s |   628.22 MiB/s |
  dec scalar                        |          |    5.58 clk/byte |    5.97 clk/byte |   767.64 MiB/s |   717.61 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.24 clk/byte |    3.27 clk/byte |  1320.66 MiB/s |  1309.66 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.02 clk/byte |    3.05 clk/byte |  1416.70 MiB/s |  1405.96 MiB/s |
  dec avx2 (sym indpt)              |          |    3.41 clk/byte |    3.43 clk/byte |  1257.00 MiB/s |  1248.06 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.02 clk/byte |    3.07 clk/byte |  1419.82 MiB/s |  1395.36 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.30 clk/byte |    2.33 clk/byte |  1861.08 MiB/s |  1838.07 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.01 clk/byte |    2.04 clk/byte |  2127.73 MiB/s |  2099.53 MiB/s |
rANS32x32 32blk 8w               15 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.58 % |    7.06 clk/byte |    7.13 clk/byte |   606.61 MiB/s |   600.76 MiB/s |
  dec scalar                        |          |    6.01 clk/byte |    6.07 clk/byte |   712.15 MiB/s |   705.78 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.64 clk/byte |    3.66 clk/byte |  1175.48 MiB/s |  1170.75 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.47 clk/byte |    3.48 clk/byte |  1233.81 MiB/s |  1230.82 MiB/s |
  dec avx2 (sym indpt)              |          |    4.01 clk/byte |    4.03 clk/byte |  1069.00 MiB/s |  1063.94 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.63 clk/byte |    3.76 clk/byte |  1180.09 MiB/s |  1140.56 MiB/s |
rANS32x32 32blk 8w               14 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.61 % |    7.05 clk/byte |    7.20 clk/byte |   607.98 MiB/s |   594.60 MiB/s |
  dec scalar                        |          |    5.98 clk/byte |    6.25 clk/byte |   716.39 MiB/s |   685.11 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.60 clk/byte |    3.63 clk/byte |  1189.33 MiB/s |  1179.74 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.37 clk/byte |    3.39 clk/byte |  1272.45 MiB/s |  1262.80 MiB/s |
  dec avx2 (sym indpt)              |          |    3.84 clk/byte |    3.85 clk/byte |  1114.82 MiB/s |  1111.62 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.51 clk/byte |    3.52 clk/byte |  1220.89 MiB/s |  1216.41 MiB/s |
rANS32x32 32blk 8w               13 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.70 % |    7.08 clk/byte |    7.10 clk/byte |   605.38 MiB/s |   603.32 MiB/s |
  dec scalar                        |          |    5.94 clk/byte |    6.02 clk/byte |   721.44 MiB/s |   711.24 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.64 clk/byte |    3.65 clk/byte |  1177.55 MiB/s |  1172.11 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.38 clk/byte |    3.41 clk/byte |  1265.81 MiB/s |  1255.01 MiB/s |
  dec avx2 (sym indpt)              |          |    3.67 clk/byte |    3.69 clk/byte |  1166.83 MiB/s |  1161.43 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.40 clk/byte |    3.41 clk/byte |  1260.74 MiB/s |  1254.60 MiB/s |
rANS32x32 32blk 8w               12 | -        | -                | -                | -              | -              |
  enc scalar                        |  12.99 % |    7.12 clk/byte |    7.15 clk/byte |   601.79 MiB/s |   599.05 MiB/s |
  dec scalar                        |          |    6.02 clk/byte |    6.16 clk/byte |   711.37 MiB/s |   695.77 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.61 clk/byte |    4.25 clk/byte |  1186.10 MiB/s |  1007.48 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.38 clk/byte |    3.95 clk/byte |  1268.59 MiB/s |  1084.26 MiB/s |
  dec avx2 (sym indpt)              |          |    3.58 clk/byte |    3.60 clk/byte |  1194.84 MiB/s |  1191.42 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.37 clk/byte |    3.37 clk/byte |  1272.43 MiB/s |  1269.55 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.51 clk/byte |    2.54 clk/byte |  1706.84 MiB/s |  1687.99 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.39 clk/byte |    2.43 clk/byte |  1795.35 MiB/s |  1762.49 MiB/s |
rANS32x32 32blk 8w               11 | -        | -                | -                | -              | -              |
  enc scalar                        |  13.73 % |    7.12 clk/byte |    7.16 clk/byte |   601.67 MiB/s |   598.33 MiB/s |
  dec scalar                        |          |    6.02 clk/byte |    6.12 clk/byte |   711.42 MiB/s |   699.94 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.65 clk/byte |    3.67 clk/byte |  1172.92 MiB/s |  1167.62 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.38 clk/byte |    3.40 clk/byte |  1266.85 MiB/s |  1260.40 MiB/s |
  dec avx2 (sym indpt)              |          |    3.65 clk/byte |    3.67 clk/byte |  1172.67 MiB/s |  1167.36 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.38 clk/byte |    3.47 clk/byte |  1266.33 MiB/s |  1234.27 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.58 clk/byte |    2.60 clk/byte |  1663.14 MiB/s |  1647.86 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.41 clk/byte |    2.42 clk/byte |  1776.01 MiB/s |  1768.25 MiB/s |
rANS32x32 32blk 8w               10 | -        | -                | -                | -              | -              |
  enc scalar                        |  16.34 % |    7.17 clk/byte |    7.21 clk/byte |   597.55 MiB/s |   593.69 MiB/s |
  dec scalar                        |          |    6.09 clk/byte |    6.17 clk/byte |   703.74 MiB/s |   693.71 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.62 clk/byte |    3.64 clk/byte |  1182.70 MiB/s |  1176.82 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.38 clk/byte |    3.39 clk/byte |  1268.30 MiB/s |  1262.56 MiB/s |
  dec avx2 (sym indpt)              |          |    3.61 clk/byte |    3.63 clk/byte |  1187.47 MiB/s |  1178.85 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.38 clk/byte |    3.39 clk/byte |  1266.38 MiB/s |  1262.78 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.54 clk/byte |    2.59 clk/byte |  1687.24 MiB/s |  1653.33 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.40 clk/byte |    2.41 clk/byte |  1783.11 MiB/s |  1775.06 MiB/s |

### Clang (via WSL2)
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 16w                    15 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.59 % |    6.28 clk/byte |    6.29 clk/byte  |   682.55 MiB/s |   680.80 MiB/s |
  dec scalar                        |          |    5.21 clk/byte |    5.32 clk/byte  |   821.48 MiB/s |   804.64 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.43 clk/byte |    2.46 clk/byte  |  1759.64 MiB/s |  1739.65 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.80 clk/byte |    2.81 clk/byte  |  1528.61 MiB/s |  1521.78 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.59 clk/byte |    2.62 clk/byte  |  1653.06 MiB/s |  1637.83 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    3.01 clk/byte |    3.02 clk/byte  |  1423.46 MiB/s |  1418.15 MiB/s |
rANS32x32 16w                    14 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.62 % |    6.28 clk/byte |    6.31 clk/byte  |   681.83 MiB/s |   678.78 MiB/s |
  dec scalar                        |          |    5.31 clk/byte |    5.33 clk/byte  |   806.73 MiB/s |   804.27 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.40 clk/byte |    2.41 clk/byte  |  1786.11 MiB/s |  1774.01 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.70 clk/byte |    2.71 clk/byte  |  1588.62 MiB/s |  1578.66 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.56 clk/byte |    2.57 clk/byte  |  1676.19 MiB/s |  1666.93 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.88 clk/byte |    2.90 clk/byte  |  1486.06 MiB/s |  1477.57 MiB/s |
rANS32x32 16w                    13 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.71 % |    6.29 clk/byte |    6.32 clk/byte  |   681.06 MiB/s |   678.25 MiB/s |
  dec scalar                        |          |    5.27 clk/byte |    5.30 clk/byte  |   812.93 MiB/s |   808.74 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.37 clk/byte |    2.38 clk/byte  |  1804.99 MiB/s |  1797.87 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.57 clk/byte |    2.58 clk/byte  |  1669.50 MiB/s |  1660.65 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.54 clk/byte |    2.56 clk/byte  |  1689.08 MiB/s |  1674.06 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.77 clk/byte |    2.79 clk/byte  |  1544.27 MiB/s |  1535.11 MiB/s |
rANS32x32 16w                    12 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.99 % |    6.36 clk/byte |    6.37 clk/byte  |   673.91 MiB/s |   672.47 MiB/s |
  dec scalar                        |          |    5.30 clk/byte |    5.37 clk/byte  |   808.46 MiB/s |   797.51 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.37 clk/byte |    2.38 clk/byte  |  1807.73 MiB/s |  1797.18 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.43 clk/byte |    2.46 clk/byte  |  1761.24 MiB/s |  1743.51 MiB/s |
  dec avx2 (xmm shfl, sngl gthr)    |          |    1.57 clk/byte |    1.59 clk/byte  |  2732.98 MiB/s |  2698.21 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.56 clk/byte |    2.57 clk/byte  |  1674.59 MiB/s |  1665.05 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.68 clk/byte |    2.70 clk/byte  |  1600.40 MiB/s |  1585.18 MiB/s |
  dec avx2 (ymm perm, sngl gthr)    |          |    1.71 clk/byte |    1.73 clk/byte  |  2498.73 MiB/s |  2471.75 MiB/s |
rANS32x32 16w                    11 | -        | -                | -                 | -              | -              |
  enc scalar                        |  13.73 % |    6.45 clk/byte |    6.47 clk/byte  |   664.59 MiB/s |   662.12 MiB/s |
  dec scalar                        |          |    5.40 clk/byte |    5.43 clk/byte  |   793.91 MiB/s |   789.05 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.38 clk/byte |    2.41 clk/byte  |  1797.36 MiB/s |  1780.45 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.44 clk/byte |    2.46 clk/byte  |  1754.25 MiB/s |  1744.37 MiB/s |
  dec avx2 (xmm shfl, sngl gthr)    |          |    1.57 clk/byte |    1.58 clk/byte  |  2730.31 MiB/s |  2705.41 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.56 clk/byte |    2.58 clk/byte  |  1673.70 MiB/s |  1662.29 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.66 clk/byte |    2.67 clk/byte  |  1612.21 MiB/s |  1602.44 MiB/s |
  dec avx2 (ymm perm, sngl gthr)    |          |    1.72 clk/byte |    1.73 clk/byte  |  2491.58 MiB/s |  2471.98 MiB/s |
rANS32x32 16w                    10 | -        | -                | -                 | -              | -              |
  enc scalar                        |  16.34 % |    6.78 clk/byte |    6.82 clk/byte  |   632.15 MiB/s |   628.14 MiB/s |
  dec scalar                        |          |    5.81 clk/byte |    5.88 clk/byte  |   737.34 MiB/s |   728.37 MiB/s |
  dec avx2 (xmm shfl, sym dpndt)    |          |    2.38 clk/byte |    2.39 clk/byte  |  1800.70 MiB/s |  1790.78 MiB/s |
  dec avx2 (xmm shfl, sym indpt)    |          |    2.43 clk/byte |    2.46 clk/byte  |  1759.59 MiB/s |  1744.06 MiB/s |
  dec avx2 (xmm shfl, sngl gthr)    |          |    1.56 clk/byte |    1.58 clk/byte  |  2737.97 MiB/s |  2715.31 MiB/s |
  dec avx2 (ymm perm, sym dpndt)    |          |    2.53 clk/byte |    2.57 clk/byte  |  1690.76 MiB/s |  1667.33 MiB/s |
  dec avx2 (ymm perm, sym indpt)    |          |    2.65 clk/byte |    2.67 clk/byte  |  1618.62 MiB/s |  1601.42 MiB/s |
  dec avx2 (ymm perm, sngl gthr)    |          |    1.70 clk/byte |    1.73 clk/byte  |  2517.48 MiB/s |  2481.76 MiB/s |
rANS32x32 32blk 16w              15 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.59 % |    6.10 clk/byte |    6.14 clk/byte  |   702.34 MiB/s |   697.27 MiB/s |
  dec scalar                        |          |    5.25 clk/byte |    5.28 clk/byte  |   815.87 MiB/s |   811.29 MiB/s |
  dec avx2 (sym dpndt)              |          |    2.94 clk/byte |    2.96 clk/byte  |  1456.62 MiB/s |  1447.36 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.64 clk/byte |    2.67 clk/byte  |  1624.87 MiB/s |  1607.02 MiB/s |
  dec avx2 (sym indpt)              |          |    3.30 clk/byte |    3.32 clk/byte  |  1299.77 MiB/s |  1289.25 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.00 clk/byte |    3.03 clk/byte  |  1429.80 MiB/s |  1415.78 MiB/s |
rANS32x32 32blk 16w              14 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.62 % |    6.23 clk/byte |    6.29 clk/byte  |   687.06 MiB/s |   681.27 MiB/s |
  dec scalar                        |          |    5.32 clk/byte |    5.36 clk/byte  |   805.28 MiB/s |   799.82 MiB/s |
  dec avx2 (sym dpndt)              |          |    2.93 clk/byte |    2.94 clk/byte  |  1459.82 MiB/s |  1454.66 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.62 clk/byte |    2.65 clk/byte  |  1635.37 MiB/s |  1618.07 MiB/s |
  dec avx2 (sym indpt)              |          |    3.21 clk/byte |    3.22 clk/byte  |  1333.80 MiB/s |  1328.29 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.90 clk/byte |    2.92 clk/byte  |  1478.46 MiB/s |  1468.47 MiB/s |
rANS32x32 32blk 16w              13 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.71 % |    6.21 clk/byte |    6.25 clk/byte  |   689.95 MiB/s |   684.93 MiB/s |
  dec scalar                        |          |    5.26 clk/byte |    5.29 clk/byte  |   814.78 MiB/s |   809.73 MiB/s |
  dec avx2 (sym dpndt)              |          |    2.93 clk/byte |    2.97 clk/byte  |  1461.07 MiB/s |  1444.33 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.64 clk/byte |    2.65 clk/byte  |  1623.37 MiB/s |  1615.86 MiB/s |
  dec avx2 (sym indpt)              |          |    3.10 clk/byte |    3.12 clk/byte  |  1382.52 MiB/s |  1374.45 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.78 clk/byte |    2.81 clk/byte  |  1538.81 MiB/s |  1526.52 MiB/s |
rANS32x32 32blk 16w              12 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.99 % |    6.24 clk/byte |    6.30 clk/byte  |   686.60 MiB/s |   680.26 MiB/s |
  dec scalar                        |          |    5.28 clk/byte |    5.31 clk/byte  |   810.74 MiB/s |   806.16 MiB/s |
  dec avx2 (sym dpndt)              |          |    2.94 clk/byte |    2.95 clk/byte  |  1457.50 MiB/s |  1450.50 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.63 clk/byte |    2.65 clk/byte  |  1626.48 MiB/s |  1616.02 MiB/s |
  dec avx2 (sym indpt)              |          |    3.07 clk/byte |    3.09 clk/byte  |  1396.02 MiB/s |  1388.33 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.66 clk/byte |    2.68 clk/byte  |  1608.71 MiB/s |  1595.64 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.20 clk/byte |    2.23 clk/byte  |  1943.09 MiB/s |  1924.81 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    1.79 clk/byte |    1.81 clk/byte  |  2391.11 MiB/s |  2366.07 MiB/s |
rANS32x32 32blk 16w              11 | -        | -                | -                 | -              | -              |
  enc scalar                        |  13.73 % |    6.37 clk/byte |    6.43 clk/byte  |   672.69 MiB/s |   666.61 MiB/s |
  dec scalar                        |          |    5.38 clk/byte |    5.44 clk/byte  |   796.16 MiB/s |   787.62 MiB/s |
  dec avx2 (sym dpndt)              |          |    2.95 clk/byte |    2.99 clk/byte  |  1449.62 MiB/s |  1434.11 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.64 clk/byte |    2.68 clk/byte  |  1619.91 MiB/s |  1600.32 MiB/s |
  dec avx2 (sym indpt)              |          |    3.08 clk/byte |    3.10 clk/byte  |  1389.34 MiB/s |  1381.45 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.68 clk/byte |    2.69 clk/byte  |  1599.23 MiB/s |  1592.94 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.19 clk/byte |    2.20 clk/byte  |  1954.62 MiB/s |  1947.53 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    1.85 clk/byte |    1.86 clk/byte  |  2319.74 MiB/s |  2296.92 MiB/s |
rANS32x32 32blk 16w              10 | -        | -                | -                 | -              | -              |
  enc scalar                        |  16.34 % |    6.71 clk/byte |    6.75 clk/byte  |   638.00 MiB/s |   634.74 MiB/s |
  dec scalar                        |          |    5.79 clk/byte |    5.85 clk/byte  |   739.32 MiB/s |   732.72 MiB/s |
  dec avx2 (sym dpndt)              |          |    2.97 clk/byte |    3.01 clk/byte  |  1441.73 MiB/s |  1424.86 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.65 clk/byte |    2.67 clk/byte  |  1614.63 MiB/s |  1601.86 MiB/s |
  dec avx2 (sym indpt)              |          |    3.12 clk/byte |    3.14 clk/byte  |  1373.01 MiB/s |  1364.69 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.67 clk/byte |    2.69 clk/byte  |  1604.31 MiB/s |  1589.51 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.20 clk/byte |    2.21 clk/byte  |  1948.04 MiB/s |  1936.20 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    1.84 clk/byte |    1.86 clk/byte  |  2324.27 MiB/s |  2300.85 MiB/s |
rANS32x32 32blk 8w               15 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.58 % |    7.06 clk/byte |    7.10 clk/byte  |   606.91 MiB/s |   603.46 MiB/s |
  dec scalar                        |          |    6.49 clk/byte |    6.52 clk/byte  |   659.68 MiB/s |   656.67 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.36 clk/byte |    3.38 clk/byte  |  1273.90 MiB/s |  1267.59 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    3.03 clk/byte |    3.05 clk/byte  |  1414.08 MiB/s |  1402.93 MiB/s |
  dec avx2 (sym indpt)              |          |    3.85 clk/byte |    3.88 clk/byte  |  1113.44 MiB/s |  1102.92 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.53 clk/byte |    3.55 clk/byte  |  1214.31 MiB/s |  1207.01 MiB/s |
rANS32x32 32blk 8w               14 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.61 % |    7.07 clk/byte |    7.09 clk/byte  |   606.03 MiB/s |   604.43 MiB/s |
  dec scalar                        |          |    6.38 clk/byte |    6.44 clk/byte  |   671.67 MiB/s |   665.51 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.25 clk/byte |    3.26 clk/byte  |  1317.88 MiB/s |  1312.18 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.96 clk/byte |    2.97 clk/byte  |  1448.41 MiB/s |  1441.11 MiB/s |
  dec avx2 (sym indpt)              |          |    3.57 clk/byte |    3.59 clk/byte  |  1198.39 MiB/s |  1192.50 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.34 clk/byte |    3.36 clk/byte  |  1283.61 MiB/s |  1275.38 MiB/s |
rANS32x32 32blk 8w               13 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.70 % |    7.09 clk/byte |    7.13 clk/byte  |   603.86 MiB/s |   600.59 MiB/s |
  dec scalar                        |          |    6.36 clk/byte |    6.39 clk/byte  |   673.21 MiB/s |   670.15 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.26 clk/byte |    3.29 clk/byte  |  1314.82 MiB/s |  1302.09 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.97 clk/byte |    2.99 clk/byte  |  1442.91 MiB/s |  1434.87 MiB/s |
  dec avx2 (sym indpt)              |          |    3.40 clk/byte |    3.42 clk/byte  |  1258.40 MiB/s |  1253.10 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    3.13 clk/byte |    3.16 clk/byte  |  1369.02 MiB/s |  1357.36 MiB/s |
rANS32x32 32blk 8w               12 | -        | -                | -                 | -              | -              |
  enc scalar                        |  12.99 % |    7.10 clk/byte |    7.15 clk/byte  |   603.56 MiB/s |   599.31 MiB/s |
  dec scalar                        |          |    6.37 clk/byte |    6.40 clk/byte  |   672.58 MiB/s |   668.79 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.24 clk/byte |    3.27 clk/byte  |  1324.01 MiB/s |  1310.90 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.94 clk/byte |    2.96 clk/byte  |  1456.96 MiB/s |  1447.56 MiB/s |
  dec avx2 (sym indpt)              |          |    3.26 clk/byte |    3.29 clk/byte  |  1312.03 MiB/s |  1301.61 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.89 clk/byte |    2.91 clk/byte  |  1483.60 MiB/s |  1472.79 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.42 clk/byte |    2.45 clk/byte  |  1768.85 MiB/s |  1749.19 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.07 clk/byte |    2.09 clk/byte  |  2066.72 MiB/s |  2051.62 MiB/s |
rANS32x32 32blk 8w               11 | -        | -                | -                 | -              | -              |
  enc scalar                        |  13.73 % |    7.13 clk/byte |    7.17 clk/byte  |   600.82 MiB/s |   597.13 MiB/s |
  dec scalar                        |          |    6.33 clk/byte |    6.36 clk/byte  |   677.06 MiB/s |   673.07 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.28 clk/byte |    3.30 clk/byte  |  1305.47 MiB/s |  1296.14 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.97 clk/byte |    2.99 clk/byte  |  1439.92 MiB/s |  1434.07 MiB/s |
  dec avx2 (sym indpt)              |          |    3.29 clk/byte |    3.33 clk/byte  |  1303.53 MiB/s |  1285.27 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.90 clk/byte |    2.93 clk/byte  |  1475.39 MiB/s |  1462.28 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.38 clk/byte |    2.40 clk/byte  |  1796.74 MiB/s |  1785.82 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.12 clk/byte |    2.13 clk/byte  |  2016.90 MiB/s |  2011.26 MiB/s |
rANS32x32 32blk 8w               10 | -        | -                | -                 | -              | -              |
  enc scalar                        |  16.34 % |    7.10 clk/byte |    7.17 clk/byte  |   603.58 MiB/s |   597.39 MiB/s |
  dec scalar                        |          |    6.44 clk/byte |    6.51 clk/byte  |   665.02 MiB/s |   658.15 MiB/s |
  dec avx2 (sym dpndt)              |          |    3.28 clk/byte |    3.31 clk/byte  |  1305.38 MiB/s |  1295.84 MiB/s |
  dec avx2 (sym dpndt 2x)           |          |    2.99 clk/byte |    3.00 clk/byte  |  1434.06 MiB/s |  1426.52 MiB/s |
  dec avx2 (sym indpt)              |          |    3.26 clk/byte |    3.29 clk/byte  |  1315.74 MiB/s |  1302.51 MiB/s |
  dec avx2 (sym indpt 2x)           |          |    2.89 clk/byte |    2.91 clk/byte  |  1480.90 MiB/s |  1471.40 MiB/s |
  dec avx2 (sngl gthr)              |          |    2.39 clk/byte |    2.40 clk/byte  |  1792.14 MiB/s |  1785.99 MiB/s |
  dec avx2 (sngl gthr 2x)           |          |    2.11 clk/byte |    2.13 clk/byte  |  2026.25 MiB/s |  2014.03 MiB/s |

## License
Two Clause BSD
