# rans_playground
Playground for rANS codecs

## [video-frame.raw](https://www.dropbox.com/s/yvsl1lg98c4maq1/video_frame.raw?dl=1) (heavily quantized video frame DCTs, 88,473,600 Bytes)

### MSVC
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 32blk 16w   15<br/> enc scalar                |  12.59 % |    6.11 clk/byte |    6.13 clk/byte |   701.24 MiB/s |   698.43 MiB/s |
rANS32x32 32blk 16w   15<br/> dec scalar                |          |    5.11 clk/byte |    5.17 clk/byte |   838.32 MiB/s |   829.17 MiB/s |
rANS32x32 32blk 16w   15<br/> dec avx2 (sym dpndt)      |          |    3.39 clk/byte |    3.49 clk/byte |  1263.16 MiB/s |  1227.21 MiB/s |
rANS32x32 32blk 16w   15<br/> dec avx2 (sym dpndt 2x)   |          |    3.18 clk/byte |    3.19 clk/byte |  1347.59 MiB/s |  1340.84 MiB/s |
rANS32x32 32blk 16w   15<br/> dec avx2 (sym indpt)      |          |    3.60 clk/byte |    3.86 clk/byte |  1190.22 MiB/s |  1109.44 MiB/s |
rANS32x32 32blk 16w   15<br/> dec avx2 (sym indpt 2x)   |          |    3.22 clk/byte |    3.24 clk/byte |  1332.18 MiB/s |  1323.38 MiB/s |
rANS32x32 32blk 16w   14<br/> enc scalar                |  12.62 % |    6.12 clk/byte |    6.13 clk/byte |   700.35 MiB/s |   698.27 MiB/s |
rANS32x32 32blk 16w   14<br/> dec scalar                |          |    5.04 clk/byte |    5.12 clk/byte |   849.56 MiB/s |   836.74 MiB/s |
rANS32x32 32blk 16w   14<br/> dec avx2 (sym dpndt)      |          |    3.36 clk/byte |    3.38 clk/byte |  1274.31 MiB/s |  1267.27 MiB/s |
rANS32x32 32blk 16w   14<br/> dec avx2 (sym dpndt 2x)   |          |    3.13 clk/byte |    3.15 clk/byte |  1367.08 MiB/s |  1358.33 MiB/s |
rANS32x32 32blk 16w   14<br/> dec avx2 (sym indpt)      |          |    3.50 clk/byte |    3.74 clk/byte |  1222.58 MiB/s |  1145.25 MiB/s |
rANS32x32 32blk 16w   14<br/> dec avx2 (sym indpt 2x)   |          |    3.15 clk/byte |    3.32 clk/byte |  1361.61 MiB/s |  1290.02 MiB/s |
rANS32x32 32blk 16w   13<br/> enc scalar                |  12.71 % |    6.11 clk/byte |    6.16 clk/byte |   701.37 MiB/s |   695.06 MiB/s |
rANS32x32 32blk 16w   13<br/> dec scalar                |          |    5.03 clk/byte |    5.24 clk/byte |   851.47 MiB/s |   817.05 MiB/s |
rANS32x32 32blk 16w   13<br/> dec avx2 (sym dpndt)      |          |    3.35 clk/byte |    3.38 clk/byte |  1278.04 MiB/s |  1268.20 MiB/s |
rANS32x32 32blk 16w   13<br/> dec avx2 (sym dpndt 2x)   |          |    3.12 clk/byte |    3.14 clk/byte |  1371.83 MiB/s |  1365.72 MiB/s |
rANS32x32 32blk 16w   13<br/> dec avx2 (sym indpt)      |          |    3.44 clk/byte |    3.45 clk/byte |  1245.42 MiB/s |  1241.04 MiB/s |
rANS32x32 32blk 16w   13<br/> dec avx2 (sym indpt 2x)   |          |    3.11 clk/byte |    3.13 clk/byte |  1377.42 MiB/s |  1369.88 MiB/s |
rANS32x32 32blk 16w   12<br/> enc scalar                |  12.99 % |    6.15 clk/byte |    6.20 clk/byte |   696.23 MiB/s |   691.03 MiB/s |
rANS32x32 32blk 16w   12<br/> dec scalar                |          |    5.12 clk/byte |    5.44 clk/byte |   836.74 MiB/s |   787.95 MiB/s |
rANS32x32 32blk 16w   12<br/> dec avx2 (sym dpndt)      |          |    3.37 clk/byte |    3.39 clk/byte |  1271.94 MiB/s |  1263.43 MiB/s |
rANS32x32 32blk 16w   12<br/> dec avx2 (sym dpndt 2x)   |          |    3.14 clk/byte |    3.16 clk/byte |  1363.89 MiB/s |  1354.67 MiB/s |
rANS32x32 32blk 16w   12<br/> dec avx2 (sym indpt)      |          |    3.41 clk/byte |    3.42 clk/byte |  1257.42 MiB/s |  1253.48 MiB/s |
rANS32x32 32blk 16w   12<br/> dec avx2 (sym indpt 2x)   |          |    3.10 clk/byte |    3.12 clk/byte |  1380.62 MiB/s |  1374.31 MiB/s |
rANS32x32 32blk 16w   12<br/> dec avx2 (sngl gthr)      |          |    2.38 clk/byte |    2.54 clk/byte |  1797.12 MiB/s |  1683.38 MiB/s |
rANS32x32 32blk 16w   12<br/> dec avx2 (sngl gthr 2x)   |          |    2.13 clk/byte |    2.25 clk/byte |  2008.10 MiB/s |  1903.31 MiB/s |
rANS32x32 32blk 16w   11<br/> enc scalar                |  13.73 % |    6.28 clk/byte |    6.39 clk/byte |   681.65 MiB/s |   670.56 MiB/s |
rANS32x32 32blk 16w   11<br/> dec scalar                |          |    5.20 clk/byte |    5.35 clk/byte |   823.07 MiB/s |   799.91 MiB/s |
rANS32x32 32blk 16w   11<br/> dec avx2 (sym dpndt)      |          |    3.38 clk/byte |    3.40 clk/byte |  1268.15 MiB/s |  1258.41 MiB/s |
rANS32x32 32blk 16w   11<br/> dec avx2 (sym dpndt 2x)   |          |    3.10 clk/byte |    3.14 clk/byte |  1380.16 MiB/s |  1363.64 MiB/s |
rANS32x32 32blk 16w   11<br/> dec avx2 (sym indpt)      |          |    3.42 clk/byte |    3.44 clk/byte |  1251.35 MiB/s |  1246.36 MiB/s |
rANS32x32 32blk 16w   11<br/> dec avx2 (sym indpt 2x)   |          |    3.13 clk/byte |    3.14 clk/byte |  1370.30 MiB/s |  1364.01 MiB/s |
rANS32x32 32blk 16w   11<br/> dec avx2 (sngl gthr)      |          |    2.37 clk/byte |    2.40 clk/byte |  1803.91 MiB/s |  1788.38 MiB/s |
rANS32x32 32blk 16w   11<br/> dec avx2 (sngl gthr 2x)   |          |    2.15 clk/byte |    2.21 clk/byte |  1994.54 MiB/s |  1939.57 MiB/s |
rANS32x32 32blk 16w   10<br/> enc scalar                |  16.34 % |    6.60 clk/byte |    6.71 clk/byte |   648.83 MiB/s |   637.91 MiB/s |
rANS32x32 32blk 16w   10<br/> dec scalar                |          |    5.59 clk/byte |    5.69 clk/byte |   766.90 MiB/s |   753.24 MiB/s |
rANS32x32 32blk 16w   10<br/> dec avx2 (sym dpndt)      |          |    3.37 clk/byte |    3.41 clk/byte |  1270.64 MiB/s |  1256.52 MiB/s |
rANS32x32 32blk 16w   10<br/> dec avx2 (sym dpndt 2x)   |          |    3.13 clk/byte |    3.15 clk/byte |  1370.64 MiB/s |  1358.50 MiB/s |
rANS32x32 32blk 16w   10<br/> dec avx2 (sym indpt)      |          |    3.43 clk/byte |    3.46 clk/byte |  1247.76 MiB/s |  1237.42 MiB/s |
rANS32x32 32blk 16w   10<br/> dec avx2 (sym indpt 2x)   |          |    3.13 clk/byte |    3.15 clk/byte |  1367.54 MiB/s |  1359.77 MiB/s |
rANS32x32 32blk 16w   10<br/> dec avx2 (sngl gthr)      |          |    2.36 clk/byte |    2.60 clk/byte |  1813.35 MiB/s |  1648.25 MiB/s |
rANS32x32 32blk 16w   10<br/> dec avx2 (sngl gthr 2x)   |          |    2.11 clk/byte |    2.26 clk/byte |  2025.45 MiB/s |  1896.49 MiB/s |
rANS32x32 32blk 8w    15<br/> enc scalar                |  12.58 % |    7.04 clk/byte |    7.09 clk/byte |   608.77 MiB/s |   604.40 MiB/s |
rANS32x32 32blk 8w    15<br/> dec scalar                |          |    6.01 clk/byte |    6.11 clk/byte |   713.25 MiB/s |   700.61 MiB/s |
rANS32x32 32blk 8w    15<br/> dec avx2 (sym dpndt)      |          |    3.74 clk/byte |    3.75 clk/byte |  1144.79 MiB/s |  1140.80 MiB/s |
rANS32x32 32blk 8w    15<br/> dec avx2 (sym dpndt 2x)   |          |    3.60 clk/byte |    3.62 clk/byte |  1191.19 MiB/s |  1183.59 MiB/s |
rANS32x32 32blk 8w    15<br/> dec avx2 (sym indpt)      |          |    4.18 clk/byte |    4.20 clk/byte |  1024.95 MiB/s |  1020.18 MiB/s |
rANS32x32 32blk 8w    15<br/> dec avx2 (sym indpt 2x)   |          |    4.00 clk/byte |    4.02 clk/byte |  1070.25 MiB/s |  1064.93 MiB/s |
rANS32x32 32blk 8w    14<br/> enc scalar                |  12.61 % |    7.09 clk/byte |    7.13 clk/byte |   603.93 MiB/s |   600.90 MiB/s |
rANS32x32 32blk 8w    14<br/> dec scalar                |          |    5.99 clk/byte |    6.07 clk/byte |   714.72 MiB/s |   705.59 MiB/s |
rANS32x32 32blk 8w    14<br/> dec avx2 (sym dpndt)      |          |    3.72 clk/byte |    3.74 clk/byte |  1151.78 MiB/s |  1146.23 MiB/s |
rANS32x32 32blk 8w    14<br/> dec avx2 (sym dpndt 2x)   |          |    3.50 clk/byte |    3.54 clk/byte |  1222.70 MiB/s |  1210.84 MiB/s |
rANS32x32 32blk 8w    14<br/> dec avx2 (sym indpt)      |          |    3.97 clk/byte |    3.99 clk/byte |  1077.94 MiB/s |  1072.22 MiB/s |
rANS32x32 32blk 8w    14<br/> dec avx2 (sym indpt 2x)   |          |    3.81 clk/byte |    4.00 clk/byte |  1124.59 MiB/s |  1070.92 MiB/s |
rANS32x32 32blk 8w    13<br/> enc scalar                |  12.70 % |    7.09 clk/byte |    7.13 clk/byte |   603.82 MiB/s |   600.37 MiB/s |
rANS32x32 32blk 8w    13<br/> dec scalar                |          |    6.00 clk/byte |    6.15 clk/byte |   713.32 MiB/s |   696.07 MiB/s |
rANS32x32 32blk 8w    13<br/> dec avx2 (sym dpndt)      |          |    3.71 clk/byte |    3.72 clk/byte |  1153.94 MiB/s |  1150.29 MiB/s |
rANS32x32 32blk 8w    13<br/> dec avx2 (sym dpndt 2x)   |          |    3.50 clk/byte |    3.52 clk/byte |  1223.09 MiB/s |  1218.19 MiB/s |
rANS32x32 32blk 8w    13<br/> dec avx2 (sym indpt)      |          |    3.81 clk/byte |    3.82 clk/byte |  1123.73 MiB/s |  1120.10 MiB/s |
rANS32x32 32blk 8w    13<br/> dec avx2 (sym indpt 2x)   |          |    3.64 clk/byte |    3.79 clk/byte |  1175.14 MiB/s |  1131.15 MiB/s |
rANS32x32 32blk 8w    12<br/> enc scalar                |  12.99 % |    7.13 clk/byte |    7.16 clk/byte |   601.09 MiB/s |   598.31 MiB/s |
rANS32x32 32blk 8w    12<br/> dec scalar                |          |    6.00 clk/byte |    6.06 clk/byte |   713.89 MiB/s |   706.53 MiB/s |
rANS32x32 32blk 8w    12<br/> dec avx2 (sym dpndt)      |          |    3.80 clk/byte |    3.83 clk/byte |  1125.95 MiB/s |  1117.90 MiB/s |
rANS32x32 32blk 8w    12<br/> dec avx2 (sym dpndt 2x)   |          |    3.52 clk/byte |    3.53 clk/byte |  1216.35 MiB/s |  1211.74 MiB/s |
rANS32x32 32blk 8w    12<br/> dec avx2 (sym indpt)      |          |    3.71 clk/byte |    3.72 clk/byte |  1155.09 MiB/s |  1150.04 MiB/s |
rANS32x32 32blk 8w    12<br/> dec avx2 (sym indpt 2x)   |          |    3.57 clk/byte |    3.58 clk/byte |  1199.31 MiB/s |  1196.45 MiB/s |
rANS32x32 32blk 8w    12<br/> dec avx2 (sngl gthr)      |          |    2.65 clk/byte |    3.10 clk/byte |  1614.29 MiB/s |  1379.54 MiB/s |
rANS32x32 32blk 8w    12<br/> dec avx2 (sngl gthr 2x)   |          |    2.58 clk/byte |    2.60 clk/byte |  1659.62 MiB/s |  1649.98 MiB/s |
rANS32x32 32blk 8w    11<br/> enc scalar                |  13.73 % |    7.15 clk/byte |    7.26 clk/byte |   598.70 MiB/s |   590.09 MiB/s |
rANS32x32 32blk 8w    11<br/> dec scalar                |          |    6.04 clk/byte |    6.11 clk/byte |   709.51 MiB/s |   700.68 MiB/s |
rANS32x32 32blk 8w    11<br/> dec avx2 (sym dpndt)      |          |    3.73 clk/byte |    3.75 clk/byte |  1149.48 MiB/s |  1142.82 MiB/s |
rANS32x32 32blk 8w    11<br/> dec avx2 (sym dpndt 2x)   |          |    3.52 clk/byte |    3.54 clk/byte |  1217.18 MiB/s |  1209.16 MiB/s |
rANS32x32 32blk 8w    11<br/> dec avx2 (sym indpt)      |          |    3.71 clk/byte |    3.76 clk/byte |  1155.91 MiB/s |  1140.00 MiB/s |
rANS32x32 32blk 8w    11<br/> dec avx2 (sym indpt 2x)   |          |    3.58 clk/byte |    3.62 clk/byte |  1196.08 MiB/s |  1182.30 MiB/s |
rANS32x32 32blk 8w    11<br/> dec avx2 (sngl gthr)      |          |    2.69 clk/byte |    2.71 clk/byte |  1593.41 MiB/s |  1582.01 MiB/s |
rANS32x32 32blk 8w    11<br/> dec avx2 (sngl gthr 2x)   |          |    2.57 clk/byte |    2.67 clk/byte |  1667.82 MiB/s |  1606.42 MiB/s |
rANS32x32 32blk 8w    10<br/> enc scalar                |  16.34 % |    7.16 clk/byte |    7.21 clk/byte |   598.37 MiB/s |   594.32 MiB/s |
rANS32x32 32blk 8w    10<br/> dec scalar                |          |    6.10 clk/byte |    6.18 clk/byte |   702.57 MiB/s |   693.22 MiB/s |
rANS32x32 32blk 8w    10<br/> dec avx2 (sym dpndt)      |          |    3.74 clk/byte |    3.77 clk/byte |  1146.39 MiB/s |  1135.25 MiB/s |
rANS32x32 32blk 8w    10<br/> dec avx2 (sym dpndt 2x)   |          |    3.51 clk/byte |    3.54 clk/byte |  1218.81 MiB/s |  1208.92 MiB/s |
rANS32x32 32blk 8w    10<br/> dec avx2 (sym indpt)      |          |    3.73 clk/byte |    3.76 clk/byte |  1148.83 MiB/s |  1138.01 MiB/s |
rANS32x32 32blk 8w    10<br/> dec avx2 (sym indpt 2x)   |          |    3.58 clk/byte |    3.59 clk/byte |  1197.55 MiB/s |  1193.37 MiB/s |
rANS32x32 32blk 8w    10<br/> dec avx2 (sngl gthr)      |          |    2.66 clk/byte |    2.69 clk/byte |  1610.09 MiB/s |  1590.72 MiB/s |
rANS32x32 32blk 8w    10<br/> dec avx2 (sngl gthr 2x)   |          |    2.58 clk/byte |    2.59 clk/byte |  1660.14 MiB/s |  1650.80 MiB/s |

### Clang (via WSL2)
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average<br/>Clocks/Byte | Maximum<br/>Throughput | Average<br/>Throughput |
| -- | --: | --: | --: | --: | --: |
rANS32x32 32blk 16w   15<br/>enc scalar                |  12.59 % |    6.24 clk/byte |    6.28 clk/byte |   686.89 MiB/s |   681.59 MiB/s |
rANS32x32 32blk 16w   15<br/>dec scalar                |          |    5.67 clk/byte |    5.75 clk/byte |   755.71 MiB/s |   744.30 MiB/s |
rANS32x32 32blk 16w   15<br/>dec avx2 (sym dpndt)      |          |    2.95 clk/byte |    2.98 clk/byte |  1449.87 MiB/s |  1437.71 MiB/s |
rANS32x32 32blk 16w   15<br/>dec avx2 (sym dpndt 2x)   |          |    2.73 clk/byte |    2.74 clk/byte |  1569.27 MiB/s |  1561.55 MiB/s |
rANS32x32 32blk 16w   15<br/>dec avx2 (sym indpt)      |          |    3.26 clk/byte |    3.28 clk/byte |  1312.65 MiB/s |  1307.43 MiB/s |
rANS32x32 32blk 16w   15<br/>dec avx2 (sym indpt 2x)   |          |    3.09 clk/byte |    3.11 clk/byte |  1387.69 MiB/s |  1377.03 MiB/s |
rANS32x32 32blk 16w   14<br/>enc scalar                |  12.62 % |    6.16 clk/byte |    6.19 clk/byte |   695.03 MiB/s |   691.58 MiB/s |
rANS32x32 32blk 16w   14<br/>dec scalar                |          |    5.23 clk/byte |    5.30 clk/byte |   819.27 MiB/s |   808.54 MiB/s |
rANS32x32 32blk 16w   14<br/>dec avx2 (sym dpndt)      |          |    2.94 clk/byte |    2.95 clk/byte |  1457.62 MiB/s |  1449.70 MiB/s |
rANS32x32 32blk 16w   14<br/>dec avx2 (sym dpndt 2x)   |          |    2.70 clk/byte |    2.72 clk/byte |  1586.73 MiB/s |  1577.51 MiB/s |
rANS32x32 32blk 16w   14<br/>dec avx2 (sym indpt)      |          |    3.14 clk/byte |    3.16 clk/byte |  1362.66 MiB/s |  1354.91 MiB/s |
rANS32x32 32blk 16w   14<br/>dec avx2 (sym indpt 2x)   |          |    2.98 clk/byte |    2.99 clk/byte |  1439.24 MiB/s |  1431.46 MiB/s |
rANS32x32 32blk 16w   13<br/>enc scalar                |  12.71 % |    6.24 clk/byte |    6.26 clk/byte |   686.45 MiB/s |   684.39 MiB/s |
rANS32x32 32blk 16w   13<br/>dec scalar                |          |    5.23 clk/byte |    5.27 clk/byte |   818.72 MiB/s |   812.43 MiB/s |
rANS32x32 32blk 16w   13<br/>dec avx2 (sym dpndt)      |          |    2.96 clk/byte |    2.97 clk/byte |  1446.53 MiB/s |  1443.37 MiB/s |
rANS32x32 32blk 16w   13<br/>dec avx2 (sym dpndt 2x)   |          |    2.72 clk/byte |    2.73 clk/byte |  1577.52 MiB/s |  1570.86 MiB/s |
rANS32x32 32blk 16w   13<br/>dec avx2 (sym indpt)      |          |    3.06 clk/byte |    3.08 clk/byte |  1399.29 MiB/s |  1388.99 MiB/s |
rANS32x32 32blk 16w   13<br/>dec avx2 (sym indpt 2x)   |          |    2.89 clk/byte |    2.91 clk/byte |  1482.21 MiB/s |  1472.08 MiB/s |
rANS32x32 32blk 16w   12<br/>enc scalar                |  12.99 % |    6.22 clk/byte |    6.23 clk/byte |   688.69 MiB/s |   687.22 MiB/s |
rANS32x32 32blk 16w   12<br/>dec scalar                |          |    5.26 clk/byte |    5.30 clk/byte |   814.56 MiB/s |   808.66 MiB/s |
rANS32x32 32blk 16w   12<br/>dec avx2 (sym dpndt)      |          |    2.95 clk/byte |    2.97 clk/byte |  1451.70 MiB/s |  1440.11 MiB/s |
rANS32x32 32blk 16w   12<br/>dec avx2 (sym dpndt 2x)   |          |    2.69 clk/byte |    2.71 clk/byte |  1589.52 MiB/s |  1578.56 MiB/s |
rANS32x32 32blk 16w   12<br/>dec avx2 (sym indpt)      |          |    2.99 clk/byte |    3.01 clk/byte |  1433.31 MiB/s |  1425.18 MiB/s |
rANS32x32 32blk 16w   12<br/>dec avx2 (sym indpt 2x)   |          |    2.78 clk/byte |    2.79 clk/byte |  1543.19 MiB/s |  1536.23 MiB/s |
rANS32x32 32blk 16w   12<br/>dec avx2 (sngl gthr)      |          |    2.18 clk/byte |    2.19 clk/byte |  1966.51 MiB/s |  1959.15 MiB/s |
rANS32x32 32blk 16w   12<br/>dec avx2 (sngl gthr 2x)   |          |    1.99 clk/byte |    2.00 clk/byte |  2151.62 MiB/s |  2142.50 MiB/s |
rANS32x32 32blk 16w   11<br/>enc scalar                |  13.73 % |    6.42 clk/byte |    6.45 clk/byte |   667.46 MiB/s |   664.58 MiB/s |
rANS32x32 32blk 16w   11<br/>dec scalar                |          |    5.82 clk/byte |    5.85 clk/byte |   736.01 MiB/s |   732.23 MiB/s |
rANS32x32 32blk 16w   11<br/>dec avx2 (sym dpndt)      |          |    2.97 clk/byte |    2.99 clk/byte |  1441.30 MiB/s |  1434.26 MiB/s |
rANS32x32 32blk 16w   11<br/>dec avx2 (sym dpndt 2x)   |          |    2.71 clk/byte |    2.73 clk/byte |  1582.69 MiB/s |  1568.34 MiB/s |
rANS32x32 32blk 16w   11<br/>dec avx2 (sym indpt)      |          |    3.02 clk/byte |    3.03 clk/byte |  1417.84 MiB/s |  1412.98 MiB/s |
rANS32x32 32blk 16w   11<br/>dec avx2 (sym indpt 2x)   |          |    2.78 clk/byte |    2.79 clk/byte |  1538.45 MiB/s |  1532.61 MiB/s |
rANS32x32 32blk 16w   11<br/>dec avx2 (sngl gthr)      |          |    2.20 clk/byte |    2.22 clk/byte |  1944.30 MiB/s |  1927.85 MiB/s |
rANS32x32 32blk 16w   11<br/>dec avx2 (sngl gthr 2x)   |          |    1.99 clk/byte |    2.00 clk/byte |  2150.81 MiB/s |  2142.17 MiB/s |
rANS32x32 32blk 16w   10<br/>enc scalar                |  16.34 % |    6.63 clk/byte |    6.66 clk/byte |   645.84 MiB/s |   642.96 MiB/s |
rANS32x32 32blk 16w   10<br/>dec scalar                |          |    5.76 clk/byte |    5.82 clk/byte |   743.98 MiB/s |   735.84 MiB/s |
rANS32x32 32blk 16w   10<br/>dec avx2 (sym dpndt)      |          |    2.94 clk/byte |    2.98 clk/byte |  1458.60 MiB/s |  1438.54 MiB/s |
rANS32x32 32blk 16w   10<br/>dec avx2 (sym dpndt 2x)   |          |    2.71 clk/byte |    2.72 clk/byte |  1580.55 MiB/s |  1574.31 MiB/s |
rANS32x32 32blk 16w   10<br/>dec avx2 (sym indpt)      |          |    3.00 clk/byte |    3.03 clk/byte |  1426.83 MiB/s |  1415.74 MiB/s |
rANS32x32 32blk 16w   10<br/>dec avx2 (sym indpt 2x)   |          |    2.78 clk/byte |    2.79 clk/byte |  1540.86 MiB/s |  1535.17 MiB/s |
rANS32x32 32blk 16w   10<br/>dec avx2 (sngl gthr)      |          |    2.21 clk/byte |    2.21 clk/byte |  1940.03 MiB/s |  1934.51 MiB/s |
rANS32x32 32blk 16w   10<br/>dec avx2 (sngl gthr 2x)   |          |    1.98 clk/byte |    2.00 clk/byte |  2160.16 MiB/s |  2142.16 MiB/s |
rANS32x32 32blk 8w    15<br/>enc scalar                |  12.58 % |    7.01 clk/byte |    7.05 clk/byte |   610.76 MiB/s |   607.55 MiB/s |
rANS32x32 32blk 8w    15<br/>dec scalar                |          |    6.26 clk/byte |    6.30 clk/byte |   684.51 MiB/s |   680.02 MiB/s |
rANS32x32 32blk 8w    15<br/>dec avx2 (sym dpndt)      |          |    3.38 clk/byte |    3.39 clk/byte |  1267.54 MiB/s |  1263.79 MiB/s |
rANS32x32 32blk 8w    15<br/>dec avx2 (sym dpndt 2x)   |          |    3.17 clk/byte |    3.18 clk/byte |  1349.08 MiB/s |  1346.13 MiB/s |
rANS32x32 32blk 8w    15<br/>dec avx2 (sym indpt)      |          |    4.06 clk/byte |    4.07 clk/byte |  1055.60 MiB/s |  1051.83 MiB/s |
rANS32x32 32blk 8w    15<br/>dec avx2 (sym indpt 2x)   |          |    3.88 clk/byte |    3.89 clk/byte |  1104.90 MiB/s |  1100.49 MiB/s |
rANS32x32 32blk 8w    14<br/>enc scalar                |  12.61 % |    6.99 clk/byte |    7.03 clk/byte |   613.00 MiB/s |   609.72 MiB/s |
rANS32x32 32blk 8w    14<br/>dec scalar                |          |    6.35 clk/byte |    6.41 clk/byte |   674.69 MiB/s |   668.11 MiB/s |
rANS32x32 32blk 8w    14<br/>dec avx2 (sym dpndt)      |          |    3.34 clk/byte |    3.35 clk/byte |  1283.59 MiB/s |  1278.98 MiB/s |
rANS32x32 32blk 8w    14<br/>dec avx2 (sym dpndt 2x)   |          |    3.12 clk/byte |    3.13 clk/byte |  1372.85 MiB/s |  1366.74 MiB/s |
rANS32x32 32blk 8w    14<br/>dec avx2 (sym indpt)      |          |    3.78 clk/byte |    3.81 clk/byte |  1133.48 MiB/s |  1125.34 MiB/s |
rANS32x32 32blk 8w    14<br/>dec avx2 (sym indpt 2x)   |          |    3.64 clk/byte |    3.65 clk/byte |  1176.85 MiB/s |  1172.48 MiB/s |
rANS32x32 32blk 8w    13<br/>enc scalar                |  12.70 % |    7.06 clk/byte |    7.14 clk/byte |   606.43 MiB/s |   599.79 MiB/s |
rANS32x32 32blk 8w    13<br/>dec scalar                |          |    6.36 clk/byte |    6.39 clk/byte |   673.93 MiB/s |   670.16 MiB/s |
rANS32x32 32blk 8w    13<br/>dec avx2 (sym dpndt)      |          |    3.35 clk/byte |    3.37 clk/byte |  1279.86 MiB/s |  1270.95 MiB/s |
rANS32x32 32blk 8w    13<br/>dec avx2 (sym dpndt 2x)   |          |    3.11 clk/byte |    3.13 clk/byte |  1379.27 MiB/s |  1367.27 MiB/s |
rANS32x32 32blk 8w    13<br/>dec avx2 (sym indpt)      |          |    3.58 clk/byte |    3.60 clk/byte |  1196.56 MiB/s |  1190.65 MiB/s |
rANS32x32 32blk 8w    13<br/>dec avx2 (sym indpt 2x)   |          |    3.45 clk/byte |    3.48 clk/byte |  1241.57 MiB/s |  1231.82 MiB/s |
rANS32x32 32blk 8w    12<br/>enc scalar                |  12.99 % |    7.15 clk/byte |    7.19 clk/byte |   599.38 MiB/s |   596.07 MiB/s |
rANS32x32 32blk 8w    12<br/>dec scalar                |          |    6.28 clk/byte |    6.35 clk/byte |   682.57 MiB/s |   674.07 MiB/s |
rANS32x32 32blk 8w    12<br/>dec avx2 (sym dpndt)      |          |    3.34 clk/byte |    3.36 clk/byte |  1280.89 MiB/s |  1273.99 MiB/s |
rANS32x32 32blk 8w    12<br/>dec avx2 (sym dpndt 2x)   |          |    3.12 clk/byte |    3.14 clk/byte |  1371.03 MiB/s |  1365.86 MiB/s |
rANS32x32 32blk 8w    12<br/>dec avx2 (sym indpt)      |          |    3.45 clk/byte |    3.47 clk/byte |  1242.77 MiB/s |  1235.19 MiB/s |
rANS32x32 32blk 8w    12<br/>dec avx2 (sym indpt 2x)   |          |    3.20 clk/byte |    3.22 clk/byte |  1337.91 MiB/s |  1331.05 MiB/s |
rANS32x32 32blk 8w    12<br/>dec avx2 (sngl gthr)      |          |    2.54 clk/byte |    2.55 clk/byte |  1689.19 MiB/s |  1677.16 MiB/s |
rANS32x32 32blk 8w    12<br/>dec avx2 (sngl gthr 2x)   |          |    2.44 clk/byte |    2.45 clk/byte |  1756.65 MiB/s |  1747.14 MiB/s |
rANS32x32 32blk 8w    11<br/>enc scalar                |  13.73 % |    7.25 clk/byte |    7.30 clk/byte |   590.60 MiB/s |   586.90 MiB/s |
rANS32x32 32blk 8w    11<br/>dec scalar                |          |    6.38 clk/byte |    6.42 clk/byte |   671.88 MiB/s |   667.52 MiB/s |
rANS32x32 32blk 8w    11<br/>dec avx2 (sym dpndt)      |          |    3.31 clk/byte |    3.36 clk/byte |  1292.47 MiB/s |  1274.41 MiB/s |
rANS32x32 32blk 8w    11<br/>dec avx2 (sym dpndt 2x)   |          |    3.12 clk/byte |    3.14 clk/byte |  1372.36 MiB/s |  1365.46 MiB/s |
rANS32x32 32blk 8w    11<br/>dec avx2 (sym indpt)      |          |    3.43 clk/byte |    3.45 clk/byte |  1248.05 MiB/s |  1240.92 MiB/s |
rANS32x32 32blk 8w    11<br/>dec avx2 (sym indpt 2x)   |          |    3.17 clk/byte |    3.19 clk/byte |  1349.89 MiB/s |  1342.86 MiB/s |
rANS32x32 32blk 8w    11<br/>dec avx2 (sngl gthr)      |          |    2.53 clk/byte |    2.55 clk/byte |  1690.85 MiB/s |  1678.90 MiB/s |
rANS32x32 32blk 8w    11<br/>dec avx2 (sngl gthr 2x)   |          |    2.39 clk/byte |    2.42 clk/byte |  1789.60 MiB/s |  1766.70 MiB/s |
rANS32x32 32blk 8w    10<br/>enc scalar                |  16.34 % |    7.19 clk/byte |    7.25 clk/byte |   595.96 MiB/s |   590.84 MiB/s |
rANS32x32 32blk 8w    10<br/>dec scalar                |          |    6.42 clk/byte |    6.46 clk/byte |   667.36 MiB/s |   662.79 MiB/s |
rANS32x32 32blk 8w    10<br/>dec avx2 (sym dpndt)      |          |    3.35 clk/byte |    3.40 clk/byte |  1277.34 MiB/s |  1260.80 MiB/s |
rANS32x32 32blk 8w    10<br/>dec avx2 (sym dpndt 2x)   |          |    3.13 clk/byte |    3.16 clk/byte |  1367.59 MiB/s |  1357.47 MiB/s |
rANS32x32 32blk 8w    10<br/>dec avx2 (sym indpt)      |          |    3.44 clk/byte |    3.46 clk/byte |  1244.08 MiB/s |  1237.43 MiB/s |
rANS32x32 32blk 8w    10<br/>dec avx2 (sym indpt 2x)   |          |    3.17 clk/byte |    3.21 clk/byte |  1352.86 MiB/s |  1336.04 MiB/s |
rANS32x32 32blk 8w    10<br/>dec avx2 (sngl gthr)      |          |    2.55 clk/byte |    2.57 clk/byte |  1681.85 MiB/s |  1666.12 MiB/s |
rANS32x32 32blk 8w    10<br/>dec avx2 (sngl gthr 2x)   |          |    2.40 clk/byte |    2.43 clk/byte |  1783.47 MiB/s |  1760.11 MiB/s |

## License
Two Clause BSD
