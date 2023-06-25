# rans_playground
Playground for rANS codecs

## [video-frame.raw](https://www.dropbox.com/s/yvsl1lg98c4maq1/video_frame.raw?dl=1) (heavily quantized video frame DCTs, 88,473,600 Bytes)

### MSVC
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 lut 16w     15<br/>encode_scalar                |  12.59 % |    6.05 clk/byte |    6.07 clk/byte |   707.75 MiB/s |   705.67 MiB/s |
rANS32x32 lut 16w     15<br/>decode_scalar                |          |    4.97 clk/byte |    5.09 clk/byte |   861.18 MiB/s |   841.90 MiB/s |
rANS32x32 lut 16w     15<br/>decode_avx2 (sym dpndt)      |          |    2.74 clk/byte |    2.76 clk/byte |  1565.84 MiB/s |  1551.88 MiB/s |
rANS32x32 lut 16w     15<br/>decode_avx2 (sym indpt)      |          |    2.75 clk/byte |    2.77 clk/byte |  1559.11 MiB/s |  1546.84 MiB/s |
rANS32x32 lut 16w     14<br/>encode_scalar                |  12.62 % |    6.03 clk/byte |    6.07 clk/byte |   710.64 MiB/s |   706.06 MiB/s |
rANS32x32 lut 16w     14<br/>decode_scalar                |          |    4.93 clk/byte |    5.08 clk/byte |   869.49 MiB/s |   843.81 MiB/s |
rANS32x32 lut 16w     14<br/>decode_avx2 (sym dpndt)      |          |    2.67 clk/byte |    2.69 clk/byte |  1601.95 MiB/s |  1593.80 MiB/s |
rANS32x32 lut 16w     14<br/>decode_avx2 (sym indpt)      |          |    2.70 clk/byte |    2.71 clk/byte |  1586.88 MiB/s |  1578.35 MiB/s |
rANS32x32 lut 16w     13<br/>encode_scalar                |  12.71 % |    6.01 clk/byte |    6.05 clk/byte |   712.13 MiB/s |   707.53 MiB/s |
rANS32x32 lut 16w     13<br/>decode_scalar                |          |    4.95 clk/byte |    5.22 clk/byte |   865.65 MiB/s |   820.62 MiB/s |
rANS32x32 lut 16w     13<br/>decode_avx2 (sym dpndt)      |          |    2.66 clk/byte |    2.83 clk/byte |  1608.15 MiB/s |  1516.01 MiB/s |
rANS32x32 lut 16w     13<br/>decode_avx2 (sym indpt)      |          |    2.65 clk/byte |    2.83 clk/byte |  1619.20 MiB/s |  1514.05 MiB/s |
rANS32x32 lut 16w     12<br/>encode_scalar                |  12.99 % |    6.11 clk/byte |    6.23 clk/byte |   701.04 MiB/s |   687.34 MiB/s |
rANS32x32 lut 16w     12<br/>decode_scalar                |          |    5.01 clk/byte |    5.18 clk/byte |   855.78 MiB/s |   827.37 MiB/s |
rANS32x32 lut 16w     12<br/>decode_avx2 (sym dpndt)      |          |    2.66 clk/byte |    2.68 clk/byte |  1610.64 MiB/s |  1599.29 MiB/s |
rANS32x32 lut 16w     12<br/>decode_avx2 (sym indpt)      |          |    2.63 clk/byte |    2.65 clk/byte |  1630.17 MiB/s |  1617.95 MiB/s |
rANS32x32 lut 16w     12<br/>decode_avx2 (sngl gthr)      |          |    1.66 clk/byte |    1.67 clk/byte |  2584.02 MiB/s |  2569.45 MiB/s |
rANS32x32 lut 16w     11<br/>encode_scalar                |  13.73 % |    6.20 clk/byte |    6.34 clk/byte |   690.61 MiB/s |   675.28 MiB/s |
rANS32x32 lut 16w     11<br/>decode_scalar                |          |    5.13 clk/byte |    5.36 clk/byte |   834.93 MiB/s |   798.59 MiB/s |
rANS32x32 lut 16w     11<br/>decode_avx2 (sym dpndt)      |          |    2.66 clk/byte |    2.68 clk/byte |  1610.46 MiB/s |  1599.45 MiB/s |
rANS32x32 lut 16w     11<br/>decode_avx2 (sym indpt)      |          |    2.63 clk/byte |    2.64 clk/byte |  1630.37 MiB/s |  1621.19 MiB/s |
rANS32x32 lut 16w     11<br/>decode_avx2 (sngl gthr)      |          |    1.65 clk/byte |    1.67 clk/byte |  2598.47 MiB/s |  2569.08 MiB/s |
rANS32x32 lut 16w     10<br/>encode_scalar                |  16.34 % |    6.46 clk/byte |    6.51 clk/byte |   663.33 MiB/s |   657.91 MiB/s |
rANS32x32 lut 16w     10<br/>decode_scalar                |          |    5.48 clk/byte |    5.53 clk/byte |   782.06 MiB/s |   774.88 MiB/s |
rANS32x32 lut 16w     10<br/>decode_avx2 (sym dpndt)      |          |    2.66 clk/byte |    2.83 clk/byte |  1612.84 MiB/s |  1515.77 MiB/s |
rANS32x32 lut 16w     10<br/>decode_avx2 (sym indpt)      |          |    2.63 clk/byte |    2.80 clk/byte |  1631.36 MiB/s |  1531.17 MiB/s |
rANS32x32 lut 16w     10<br/>decode_avx2 (sngl gthr)      |          |    1.66 clk/byte |    1.82 clk/byte |  2587.57 MiB/s |  2359.58 MiB/s |
rANS32x32 32blk 16w   15<br/>encode_scalar                |  12.59 % |    6.12 clk/byte |    6.17 clk/byte |   700.45 MiB/s |   694.17 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_scalar                |          |    5.11 clk/byte |    5.17 clk/byte |   837.78 MiB/s |   828.40 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym dpndt)      |          |    3.25 clk/byte |    3.26 clk/byte |  1318.56 MiB/s |  1312.51 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym dpndt 2x)   |          |    3.09 clk/byte |    3.10 clk/byte |  1386.08 MiB/s |  1382.57 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym indpt)      |          |    3.58 clk/byte |    3.60 clk/byte |  1195.90 MiB/s |  1188.22 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym indpt 2x)   |          |    3.15 clk/byte |    3.17 clk/byte |  1359.88 MiB/s |  1351.61 MiB/s |
rANS32x32 32blk 16w   14<br/>encode_scalar                |  12.62 % |    6.11 clk/byte |    6.15 clk/byte |   701.35 MiB/s |   696.82 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_scalar                |          |    5.03 clk/byte |    5.14 clk/byte |   851.17 MiB/s |   833.59 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym dpndt)      |          |    3.24 clk/byte |    3.26 clk/byte |  1321.45 MiB/s |  1314.23 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym dpndt 2x)   |          |    3.02 clk/byte |    3.04 clk/byte |  1416.21 MiB/s |  1408.27 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym indpt)      |          |    3.49 clk/byte |    3.51 clk/byte |  1229.06 MiB/s |  1220.46 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym indpt 2x)   |          |    3.09 clk/byte |    3.18 clk/byte |  1384.68 MiB/s |  1347.04 MiB/s |
rANS32x32 32blk 16w   13<br/>encode_scalar                |  12.71 % |    6.12 clk/byte |    6.30 clk/byte |   700.38 MiB/s |   680.18 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_scalar                |          |    5.07 clk/byte |    5.19 clk/byte |   844.64 MiB/s |   824.84 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym dpndt)      |          |    3.26 clk/byte |    3.27 clk/byte |  1315.74 MiB/s |  1308.81 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym dpndt 2x)   |          |    3.03 clk/byte |    3.05 clk/byte |  1411.70 MiB/s |  1405.56 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym indpt)      |          |    3.43 clk/byte |    3.44 clk/byte |  1248.10 MiB/s |  1243.37 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym indpt 2x)   |          |    3.06 clk/byte |    3.16 clk/byte |  1401.00 MiB/s |  1355.94 MiB/s |
rANS32x32 32blk 16w   12<br/>encode_scalar                |  12.99 % |    6.18 clk/byte |    6.20 clk/byte |   693.43 MiB/s |   690.93 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_scalar                |          |    5.07 clk/byte |    5.15 clk/byte |   844.19 MiB/s |   831.29 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym dpndt)      |          |    3.27 clk/byte |    3.29 clk/byte |  1308.94 MiB/s |  1302.93 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym dpndt 2x)   |          |    3.05 clk/byte |    3.07 clk/byte |  1402.82 MiB/s |  1396.03 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym indpt)      |          |    3.42 clk/byte |    3.43 clk/byte |  1253.94 MiB/s |  1249.14 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym indpt 2x)   |          |    3.06 clk/byte |    3.08 clk/byte |  1400.94 MiB/s |  1391.95 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sngl gthr)      |          |    2.31 clk/byte |    2.32 clk/byte |  1853.39 MiB/s |  1845.10 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sngl gthr 2x)   |          |    2.05 clk/byte |    2.06 clk/byte |  2091.29 MiB/s |  2082.29 MiB/s |
rANS32x32 32blk 16w   11<br/>encode_scalar                |  13.73 % |    6.30 clk/byte |    6.43 clk/byte |   680.29 MiB/s |   666.60 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_scalar                |          |    5.21 clk/byte |    5.28 clk/byte |   822.87 MiB/s |   810.92 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym dpndt)      |          |    3.27 clk/byte |    3.30 clk/byte |  1307.98 MiB/s |  1296.87 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym dpndt 2x)   |          |    3.08 clk/byte |    3.59 clk/byte |  1389.62 MiB/s |  1193.80 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym indpt)      |          |    3.47 clk/byte |    4.41 clk/byte |  1235.32 MiB/s |   970.87 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym indpt 2x)   |          |    3.09 clk/byte |    3.20 clk/byte |  1384.15 MiB/s |  1340.50 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sngl gthr)      |          |    2.39 clk/byte |    2.56 clk/byte |  1790.88 MiB/s |  1675.09 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sngl gthr 2x)   |          |    2.02 clk/byte |    2.04 clk/byte |  2125.02 MiB/s |  2096.22 MiB/s |
rANS32x32 32blk 16w   10<br/>encode_scalar                |  16.34 % |    6.59 clk/byte |    6.63 clk/byte |   650.17 MiB/s |   645.86 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_scalar                |          |    5.60 clk/byte |    5.71 clk/byte |   764.24 MiB/s |   749.86 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym dpndt)      |          |    3.27 clk/byte |    3.29 clk/byte |  1308.58 MiB/s |  1301.62 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym dpndt 2x)   |          |    3.05 clk/byte |    3.06 clk/byte |  1403.81 MiB/s |  1399.79 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym indpt)      |          |    3.44 clk/byte |    3.49 clk/byte |  1245.18 MiB/s |  1227.07 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym indpt 2x)   |          |    3.07 clk/byte |    3.10 clk/byte |  1393.65 MiB/s |  1383.67 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sngl gthr)      |          |    2.34 clk/byte |    2.36 clk/byte |  1833.29 MiB/s |  1818.63 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sngl gthr 2x)   |          |    2.05 clk/byte |    2.06 clk/byte |  2093.18 MiB/s |  2083.65 MiB/s |
rANS32x32 32blk 8w    15<br/>encode_scalar                |  12.58 % |    7.06 clk/byte |    7.24 clk/byte |   606.66 MiB/s |   591.60 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_scalar                |          |    6.08 clk/byte |    6.36 clk/byte |   705.06 MiB/s |   673.50 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym dpndt)      |          |    3.68 clk/byte |    3.69 clk/byte |  1164.52 MiB/s |  1160.23 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym dpndt 2x)   |          |    3.49 clk/byte |    3.51 clk/byte |  1228.02 MiB/s |  1220.82 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym indpt)      |          |    4.03 clk/byte |    4.05 clk/byte |  1063.68 MiB/s |  1058.75 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym indpt 2x)   |          |    3.65 clk/byte |    3.66 clk/byte |  1174.82 MiB/s |  1170.56 MiB/s |
rANS32x32 32blk 8w    14<br/>encode_scalar                |  12.61 % |    7.10 clk/byte |    7.13 clk/byte |   603.05 MiB/s |   600.66 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_scalar                |          |    6.02 clk/byte |    6.10 clk/byte |   711.26 MiB/s |   702.01 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym dpndt)      |          |    3.63 clk/byte |    3.64 clk/byte |  1181.52 MiB/s |  1176.09 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym dpndt 2x)   |          |    3.40 clk/byte |    3.48 clk/byte |  1260.66 MiB/s |  1229.56 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym indpt)      |          |    3.85 clk/byte |    3.86 clk/byte |  1111.33 MiB/s |  1108.75 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym indpt 2x)   |          |    3.52 clk/byte |    3.54 clk/byte |  1215.88 MiB/s |  1209.19 MiB/s |
rANS32x32 32blk 8w    13<br/>encode_scalar                |  12.70 % |    7.12 clk/byte |    7.17 clk/byte |   601.92 MiB/s |   597.80 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_scalar                |          |    6.00 clk/byte |    6.06 clk/byte |   713.43 MiB/s |   707.32 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym dpndt)      |          |    3.65 clk/byte |    3.67 clk/byte |  1174.13 MiB/s |  1168.67 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym dpndt 2x)   |          |    3.39 clk/byte |    3.86 clk/byte |  1263.58 MiB/s |  1109.38 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym indpt)      |          |    3.69 clk/byte |    4.18 clk/byte |  1161.13 MiB/s |  1024.03 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym indpt 2x)   |          |    3.41 clk/byte |    3.42 clk/byte |  1255.93 MiB/s |  1251.61 MiB/s |
rANS32x32 32blk 8w    12<br/>encode_scalar                |  12.99 % |    7.13 clk/byte |    7.16 clk/byte |   600.82 MiB/s |   597.91 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_scalar                |          |    6.02 clk/byte |    6.11 clk/byte |   711.88 MiB/s |   701.35 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym dpndt)      |          |    3.62 clk/byte |    3.64 clk/byte |  1181.70 MiB/s |  1175.35 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym dpndt 2x)   |          |    3.41 clk/byte |    3.43 clk/byte |  1257.48 MiB/s |  1247.07 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym indpt)      |          |    3.60 clk/byte |    3.88 clk/byte |  1189.10 MiB/s |  1102.89 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym indpt 2x)   |          |    3.40 clk/byte |    3.42 clk/byte |  1260.02 MiB/s |  1252.04 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sngl gthr)      |          |    2.56 clk/byte |    2.57 clk/byte |  1675.51 MiB/s |  1666.80 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sngl gthr 2x)   |          |    2.42 clk/byte |    2.44 clk/byte |  1767.67 MiB/s |  1754.86 MiB/s |
rANS32x32 32blk 8w    11<br/>encode_scalar                |  13.73 % |    7.17 clk/byte |    7.20 clk/byte |   597.39 MiB/s |   595.20 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_scalar                |          |    6.06 clk/byte |    6.34 clk/byte |   706.41 MiB/s |   675.81 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym dpndt)      |          |    3.63 clk/byte |    3.89 clk/byte |  1179.74 MiB/s |  1100.64 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym dpndt 2x)   |          |    3.40 clk/byte |    3.42 clk/byte |  1260.48 MiB/s |  1253.58 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym indpt)      |          |    3.61 clk/byte |    4.08 clk/byte |  1187.09 MiB/s |  1049.05 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym indpt 2x)   |          |    3.37 clk/byte |    3.40 clk/byte |  1271.33 MiB/s |  1259.93 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sngl gthr)      |          |    2.52 clk/byte |    2.55 clk/byte |  1698.80 MiB/s |  1679.15 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sngl gthr 2x)   |          |    2.37 clk/byte |    2.48 clk/byte |  1806.24 MiB/s |  1729.90 MiB/s |
rANS32x32 32blk 8w    10<br/>encode_scalar                |  16.34 % |    7.16 clk/byte |    7.22 clk/byte |   597.86 MiB/s |   593.25 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_scalar                |          |    6.09 clk/byte |    6.17 clk/byte |   703.21 MiB/s |   694.57 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym dpndt)      |          |    3.64 clk/byte |    3.67 clk/byte |  1177.13 MiB/s |  1167.70 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym dpndt 2x)   |          |    3.38 clk/byte |    3.41 clk/byte |  1267.54 MiB/s |  1257.38 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym indpt)      |          |    3.62 clk/byte |    3.94 clk/byte |  1184.07 MiB/s |  1085.83 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym indpt 2x)   |          |    3.41 clk/byte |    3.52 clk/byte |  1257.39 MiB/s |  1215.29 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sngl gthr)      |          |    2.53 clk/byte |    2.55 clk/byte |  1691.52 MiB/s |  1676.84 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sngl gthr 2x)   |          |    2.40 clk/byte |    2.42 clk/byte |  1783.56 MiB/s |  1767.44 MiB/s |

### Clang (via WSL2)
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 lut 16w     15<br/>encode_scalar                |  12.59 % |    6.16 clk/byte |    6.21 clk/byte |   694.82 MiB/s |   689.78 MiB/s |
rANS32x32 lut 16w     15<br/>decode_scalar                |          |    5.22 clk/byte |    5.25 clk/byte |   821.13 MiB/s |   815.25 MiB/s |
rANS32x32 lut 16w     15<br/>decode_avx2 (sym dpndt)      |          |    2.43 clk/byte |    2.47 clk/byte |  1763.59 MiB/s |  1737.43 MiB/s |
rANS32x32 lut 16w     15<br/>decode_avx2 (sym indpt)      |          |    2.80 clk/byte |    2.82 clk/byte |  1530.90 MiB/s |  1518.45 MiB/s |
rANS32x32 lut 16w     14<br/>encode_scalar                |  12.62 % |    6.20 clk/byte |    6.23 clk/byte |   690.38 MiB/s |   687.84 MiB/s |
rANS32x32 lut 16w     14<br/>decode_scalar                |          |    5.23 clk/byte |    5.29 clk/byte |   818.95 MiB/s |   809.09 MiB/s |
rANS32x32 lut 16w     14<br/>decode_avx2 (sym dpndt)      |          |    2.40 clk/byte |    2.43 clk/byte |  1783.04 MiB/s |  1764.73 MiB/s |
rANS32x32 lut 16w     14<br/>decode_avx2 (sym indpt)      |          |    2.72 clk/byte |    2.73 clk/byte |  1575.59 MiB/s |  1566.31 MiB/s |
rANS32x32 lut 16w     13<br/>encode_scalar                |  12.71 % |    6.28 clk/byte |    6.32 clk/byte |   682.45 MiB/s |   678.23 MiB/s |
rANS32x32 lut 16w     13<br/>decode_scalar                |          |    5.22 clk/byte |    5.28 clk/byte |   820.54 MiB/s |   810.95 MiB/s |
rANS32x32 lut 16w     13<br/>decode_avx2 (sym dpndt)      |          |    2.41 clk/byte |    2.43 clk/byte |  1779.73 MiB/s |  1761.55 MiB/s |
rANS32x32 lut 16w     13<br/>decode_avx2 (sym indpt)      |          |    2.63 clk/byte |    2.65 clk/byte |  1630.20 MiB/s |  1615.93 MiB/s |
rANS32x32 lut 16w     12<br/>encode_scalar                |  12.99 % |    6.29 clk/byte |    6.35 clk/byte |   680.54 MiB/s |   674.76 MiB/s |
rANS32x32 lut 16w     12<br/>decode_scalar                |          |    5.31 clk/byte |    5.35 clk/byte |   806.31 MiB/s |   800.07 MiB/s |
rANS32x32 lut 16w     12<br/>decode_avx2 (sym dpndt)      |          |    2.40 clk/byte |    2.43 clk/byte |  1782.47 MiB/s |  1763.85 MiB/s |
rANS32x32 lut 16w     12<br/>decode_avx2 (sym indpt)      |          |    2.48 clk/byte |    2.51 clk/byte |  1727.33 MiB/s |  1708.64 MiB/s |
rANS32x32 lut 16w     12<br/>decode_avx2 (sngl gthr)      |          |    1.57 clk/byte |    1.60 clk/byte |  2732.08 MiB/s |  2682.05 MiB/s |
rANS32x32 lut 16w     11<br/>encode_scalar                |  13.73 % |    6.40 clk/byte |    6.43 clk/byte |   669.37 MiB/s |   666.16 MiB/s |
rANS32x32 lut 16w     11<br/>decode_scalar                |          |    5.34 clk/byte |    5.44 clk/byte |   801.59 MiB/s |   787.98 MiB/s |
rANS32x32 lut 16w     11<br/>decode_avx2 (sym dpndt)      |          |    2.38 clk/byte |    2.41 clk/byte |  1798.23 MiB/s |  1773.71 MiB/s |
rANS32x32 lut 16w     11<br/>decode_avx2 (sym indpt)      |          |    2.47 clk/byte |    2.49 clk/byte |  1734.64 MiB/s |  1717.25 MiB/s |
rANS32x32 lut 16w     11<br/>decode_avx2 (sngl gthr)      |          |    1.57 clk/byte |    1.61 clk/byte |  2720.53 MiB/s |  2665.42 MiB/s |
rANS32x32 lut 16w     10<br/>encode_scalar                |  16.34 % |    6.70 clk/byte |    6.73 clk/byte |   638.99 MiB/s |   636.58 MiB/s |
rANS32x32 lut 16w     10<br/>decode_scalar                |          |    5.73 clk/byte |    5.83 clk/byte |   748.11 MiB/s |   735.00 MiB/s |
rANS32x32 lut 16w     10<br/>decode_avx2 (sym dpndt)      |          |    2.39 clk/byte |    2.42 clk/byte |  1795.85 MiB/s |  1768.82 MiB/s |
rANS32x32 lut 16w     10<br/>decode_avx2 (sym indpt)      |          |    2.46 clk/byte |    2.48 clk/byte |  1742.99 MiB/s |  1728.48 MiB/s |
rANS32x32 lut 16w     10<br/>decode_avx2 (sngl gthr)      |          |    1.57 clk/byte |    1.59 clk/byte |  2729.19 MiB/s |  2686.88 MiB/s |
rANS32x32 32blk 16w   15<br/>encode_scalar                |  12.59 % |    6.19 clk/byte |    6.24 clk/byte |   691.61 MiB/s |   686.32 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_scalar                |          |    5.23 clk/byte |    5.28 clk/byte |   818.50 MiB/s |   811.32 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym dpndt)      |          |    2.96 clk/byte |    2.99 clk/byte |  1446.28 MiB/s |  1433.59 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym dpndt 2x)   |          |    2.71 clk/byte |    2.72 clk/byte |  1580.26 MiB/s |  1575.11 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym indpt)      |          |    3.35 clk/byte |    3.37 clk/byte |  1278.34 MiB/s |  1270.23 MiB/s |
rANS32x32 32blk 16w   15<br/>decode_avx2 (sym indpt 2x)   |          |    3.06 clk/byte |    3.07 clk/byte |  1398.85 MiB/s |  1396.90 MiB/s |
rANS32x32 32blk 16w   14<br/>encode_scalar                |  12.62 % |    6.13 clk/byte |    6.18 clk/byte |   698.42 MiB/s |   693.00 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_scalar                |          |    5.30 clk/byte |    5.32 clk/byte |   808.86 MiB/s |   805.77 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym dpndt)      |          |    2.95 clk/byte |    2.97 clk/byte |  1453.69 MiB/s |  1443.43 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym dpndt 2x)   |          |    2.69 clk/byte |    2.71 clk/byte |  1589.94 MiB/s |  1583.04 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym indpt)      |          |    3.22 clk/byte |    3.25 clk/byte |  1329.53 MiB/s |  1318.48 MiB/s |
rANS32x32 32blk 16w   14<br/>decode_avx2 (sym indpt 2x)   |          |    2.92 clk/byte |    2.95 clk/byte |  1467.20 MiB/s |  1454.28 MiB/s |
rANS32x32 32blk 16w   13<br/>encode_scalar                |  12.71 % |    6.24 clk/byte |    6.28 clk/byte |   686.63 MiB/s |   681.58 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_scalar                |          |    5.62 clk/byte |    5.70 clk/byte |   761.90 MiB/s |   751.52 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym dpndt)      |          |    2.95 clk/byte |    2.97 clk/byte |  1452.93 MiB/s |  1441.91 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym dpndt 2x)   |          |    2.70 clk/byte |    2.72 clk/byte |  1587.76 MiB/s |  1574.05 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym indpt)      |          |    3.14 clk/byte |    3.17 clk/byte |  1363.78 MiB/s |  1351.71 MiB/s |
rANS32x32 32blk 16w   13<br/>decode_avx2 (sym indpt 2x)   |          |    2.82 clk/byte |    2.85 clk/byte |  1518.49 MiB/s |  1500.59 MiB/s |
rANS32x32 32blk 16w   12<br/>encode_scalar                |  12.99 % |    6.31 clk/byte |    6.32 clk/byte |   679.27 MiB/s |   678.02 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_scalar                |          |    5.29 clk/byte |    5.34 clk/byte |   809.91 MiB/s |   801.94 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym dpndt)      |          |    2.96 clk/byte |    2.98 clk/byte |  1446.78 MiB/s |  1434.97 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym dpndt 2x)   |          |    2.72 clk/byte |    2.74 clk/byte |  1576.55 MiB/s |  1562.19 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym indpt)      |          |    3.13 clk/byte |    3.14 clk/byte |  1370.61 MiB/s |  1364.77 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sym indpt 2x)   |          |    2.74 clk/byte |    2.76 clk/byte |  1564.62 MiB/s |  1549.18 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sngl gthr)      |          |    2.27 clk/byte |    2.30 clk/byte |  1887.97 MiB/s |  1866.04 MiB/s |
rANS32x32 32blk 16w   12<br/>decode_avx2 (sngl gthr 2x)   |          |    1.82 clk/byte |    1.84 clk/byte |  2349.73 MiB/s |  2325.47 MiB/s |
rANS32x32 32blk 16w   11<br/>encode_scalar                |  13.73 % |    6.43 clk/byte |    6.46 clk/byte |   666.49 MiB/s |   663.38 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_scalar                |          |    5.41 clk/byte |    5.47 clk/byte |   791.09 MiB/s |   783.71 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym dpndt)      |          |    2.97 clk/byte |    2.99 clk/byte |  1442.33 MiB/s |  1432.81 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym dpndt 2x)   |          |    2.72 clk/byte |    2.75 clk/byte |  1574.94 MiB/s |  1555.49 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym indpt)      |          |    3.10 clk/byte |    3.12 clk/byte |  1382.15 MiB/s |  1372.67 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sym indpt 2x)   |          |    2.74 clk/byte |    2.77 clk/byte |  1564.67 MiB/s |  1545.85 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sngl gthr)      |          |    2.19 clk/byte |    2.22 clk/byte |  1951.93 MiB/s |  1933.14 MiB/s |
rANS32x32 32blk 16w   11<br/>decode_avx2 (sngl gthr 2x)   |          |    1.85 clk/byte |    1.87 clk/byte |  2321.36 MiB/s |  2287.62 MiB/s |
rANS32x32 32blk 16w   10<br/>encode_scalar                |  16.34 % |    6.74 clk/byte |    6.78 clk/byte |   635.21 MiB/s |   631.55 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_scalar                |          |    6.22 clk/byte |    6.28 clk/byte |   688.20 MiB/s |   681.60 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym dpndt)      |          |    2.92 clk/byte |    2.94 clk/byte |  1466.48 MiB/s |  1457.00 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym dpndt 2x)   |          |    2.70 clk/byte |    2.71 clk/byte |  1589.10 MiB/s |  1580.46 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym indpt)      |          |    3.06 clk/byte |    3.08 clk/byte |  1398.83 MiB/s |  1390.28 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sym indpt 2x)   |          |    2.72 clk/byte |    2.75 clk/byte |  1574.83 MiB/s |  1555.43 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sngl gthr)      |          |    2.16 clk/byte |    2.18 clk/byte |  1984.97 MiB/s |  1961.04 MiB/s |
rANS32x32 32blk 16w   10<br/>decode_avx2 (sngl gthr 2x)   |          |    1.85 clk/byte |    1.86 clk/byte |  2321.22 MiB/s |  2298.28 MiB/s |
rANS32x32 32blk 8w    15<br/>encode_scalar                |  12.58 % |    6.95 clk/byte |    7.00 clk/byte |   616.34 MiB/s |   611.48 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_scalar                |          |    6.37 clk/byte |    6.46 clk/byte |   672.60 MiB/s |   663.39 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym dpndt)      |          |    3.38 clk/byte |    3.40 clk/byte |  1266.70 MiB/s |  1259.22 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym dpndt 2x)   |          |    3.10 clk/byte |    3.11 clk/byte |  1380.85 MiB/s |  1376.93 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym indpt)      |          |    3.84 clk/byte |    3.86 clk/byte |  1114.93 MiB/s |  1110.30 MiB/s |
rANS32x32 32blk 8w    15<br/>decode_avx2 (sym indpt 2x)   |          |    3.54 clk/byte |    3.58 clk/byte |  1210.36 MiB/s |  1197.39 MiB/s |
rANS32x32 32blk 8w    14<br/>encode_scalar                |  12.61 % |    6.98 clk/byte |    7.01 clk/byte |   613.83 MiB/s |   611.38 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_scalar                |          |    6.38 clk/byte |    6.45 clk/byte |   671.17 MiB/s |   663.70 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym dpndt)      |          |    3.31 clk/byte |    3.32 clk/byte |  1295.17 MiB/s |  1288.40 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym dpndt 2x)   |          |    3.03 clk/byte |    3.05 clk/byte |  1413.00 MiB/s |  1402.35 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym indpt)      |          |    3.59 clk/byte |    3.60 clk/byte |  1194.11 MiB/s |  1188.18 MiB/s |
rANS32x32 32blk 8w    14<br/>decode_avx2 (sym indpt 2x)   |          |    3.34 clk/byte |    3.36 clk/byte |  1283.61 MiB/s |  1273.82 MiB/s |
rANS32x32 32blk 8w    13<br/>encode_scalar                |  12.70 % |    6.96 clk/byte |    7.00 clk/byte |   615.01 MiB/s |   611.78 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_scalar                |          |    6.38 clk/byte |    6.40 clk/byte |   671.61 MiB/s |   669.20 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym dpndt)      |          |    3.30 clk/byte |    3.32 clk/byte |  1296.58 MiB/s |  1290.05 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym dpndt 2x)   |          |    3.02 clk/byte |    3.06 clk/byte |  1416.07 MiB/s |  1400.30 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym indpt)      |          |    3.39 clk/byte |    3.42 clk/byte |  1263.83 MiB/s |  1253.54 MiB/s |
rANS32x32 32blk 8w    13<br/>decode_avx2 (sym indpt 2x)   |          |    3.15 clk/byte |    3.17 clk/byte |  1359.33 MiB/s |  1351.27 MiB/s |
rANS32x32 32blk 8w    12<br/>encode_scalar                |  12.99 % |    7.03 clk/byte |    7.06 clk/byte |   609.27 MiB/s |   606.95 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_scalar                |          |    6.36 clk/byte |    6.40 clk/byte |   673.63 MiB/s |   669.11 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym dpndt)      |          |    3.32 clk/byte |    3.34 clk/byte |  1288.27 MiB/s |  1281.61 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym dpndt 2x)   |          |    3.06 clk/byte |    3.08 clk/byte |  1402.03 MiB/s |  1391.30 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym indpt)      |          |    3.32 clk/byte |    3.33 clk/byte |  1289.38 MiB/s |  1285.07 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sym indpt 2x)   |          |    2.98 clk/byte |    2.99 clk/byte |  1436.98 MiB/s |  1430.95 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sngl gthr)      |          |    2.56 clk/byte |    2.57 clk/byte |  1673.74 MiB/s |  1668.32 MiB/s |
rANS32x32 32blk 8w    12<br/>decode_avx2 (sngl gthr 2x)   |          |    2.14 clk/byte |    2.16 clk/byte |  2003.05 MiB/s |  1983.38 MiB/s |
rANS32x32 32blk 8w    11<br/>encode_scalar                |  13.73 % |    7.23 clk/byte |    7.26 clk/byte |   592.07 MiB/s |   590.10 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_scalar                |          |    6.36 clk/byte |    6.41 clk/byte |   673.07 MiB/s |   668.53 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym dpndt)      |          |    3.31 clk/byte |    3.33 clk/byte |  1292.20 MiB/s |  1284.54 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym dpndt 2x)   |          |    3.06 clk/byte |    3.08 clk/byte |  1399.93 MiB/s |  1388.95 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym indpt)      |          |    3.30 clk/byte |    3.32 clk/byte |  1299.81 MiB/s |  1291.29 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sym indpt 2x)   |          |    2.93 clk/byte |    2.96 clk/byte |  1460.05 MiB/s |  1449.18 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sngl gthr)      |          |    2.38 clk/byte |    2.43 clk/byte |  1796.17 MiB/s |  1764.31 MiB/s |
rANS32x32 32blk 8w    11<br/>decode_avx2 (sngl gthr 2x)   |          |    2.12 clk/byte |    2.16 clk/byte |  2016.30 MiB/s |  1987.15 MiB/s |
rANS32x32 32blk 8w    10<br/>encode_scalar                |  16.34 % |    7.18 clk/byte |    7.21 clk/byte |   596.58 MiB/s |   593.93 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_scalar                |          |    6.38 clk/byte |    6.43 clk/byte |   671.56 MiB/s |   665.82 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym dpndt)      |          |    3.29 clk/byte |    3.31 clk/byte |  1303.38 MiB/s |  1295.15 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym dpndt 2x)   |          |    3.01 clk/byte |    3.07 clk/byte |  1422.66 MiB/s |  1392.98 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym indpt)      |          |    3.25 clk/byte |    3.28 clk/byte |  1317.60 MiB/s |  1305.60 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sym indpt 2x)   |          |    2.93 clk/byte |    2.97 clk/byte |  1460.40 MiB/s |  1442.82 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sngl gthr)      |          |    2.40 clk/byte |    2.42 clk/byte |  1783.06 MiB/s |  1768.29 MiB/s |
rANS32x32 32blk 8w    10<br/>decode_avx2 (sngl gthr 2x)   |          |    2.14 clk/byte |    2.15 clk/byte |  2001.81 MiB/s |  1990.76 MiB/s |

## License
Two Clause BSD
