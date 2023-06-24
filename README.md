# rans_playground
Playground for rANS codecs

## [video-frame.raw](https://www.dropbox.com/s/yvsl1lg98c4maq1/video_frame.raw?dl=1) (heavily quantized video frame DCTs, 88,473,600 Bytes)

### MSVC
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average (StdDev.) | Maximum<br/>Throughput<br/>MiB/s | Average (StdDev.) |
| -- | --: | --: | --: | --: | --: |
rANS32x32 multi block 15<br/>encode_basic                 |  12.58 % |  7.031 |  7.063<br/>( 7.020 ~  7.106) |   609.20 |   606.42<br/>(  602.75 ~   610.15) |
rANS32x32 multi block 15<br/>decode_basic                 |          |  5.995 |  6.054<br/>( 6.025 ~  6.083) |   714.45 |   707.55<br/>(  704.19 ~   710.95) |
rANS32x32 multi block 15<br/>decode_avx2 (sym dpndt)      |          |  3.756 |  3.904<br/>( 3.360 ~  4.449) |  1140.39 |  1097.04<br/>(  962.87 ~  1274.66) |
rANS32x32 multi block 15<br/>decode_avx2 (sym dpndt 2x)   |          |  3.586 |  3.730<br/>( 3.314 ~  4.145) |  1194.42 |  1148.45<br/>( 1033.37 ~  1292.39) |
rANS32x32 multi block 15<br/>decode_avx2 (sym indpt)      |          |  4.169 |  4.344<br/>( 3.800 ~  4.888) |  1027.36 |   986.12<br/>(  876.35 ~  1127.32) |
rANS32x32 multi block 15<br/>decode_avx2 (sym indpt 2x)   |          |  3.971 |  4.005<br/>( 3.964 ~  4.047) |  1078.68 |  1069.46<br/>( 1058.49 ~  1080.66) |
rANS32x32 multi block 14<br/>encode_basic                 |  12.61 % |  7.033 |  7.062<br/>( 7.043 ~  7.082) |   609.04 |   606.51<br/>(  604.84 ~   608.20) |
rANS32x32 multi block 14<br/>decode_basic                 |          |  5.950 |  6.034<br/>( 5.978 ~  6.090) |   719.94 |   709.88<br/>(  703.37 ~   716.50) |
rANS32x32 multi block 14<br/>decode_avx2 (sym dpndt)      |          |  3.683 |  3.695<br/>( 3.684 ~  3.706) |  1163.09 |  1159.24<br/>( 1155.69 ~  1162.81) |
rANS32x32 multi block 14<br/>decode_avx2 (sym dpndt 2x)   |          |  3.471 |  3.512<br/>( 3.448 ~  3.575) |  1233.90 |  1219.72<br/>( 1198.05 ~  1242.18) |
rANS32x32 multi block 14<br/>decode_avx2 (sym indpt)      |          |  3.949 |  3.964<br/>( 3.953 ~  3.976) |  1084.66 |  1080.54<br/>( 1077.40 ~  1083.69) |
rANS32x32 multi block 14<br/>decode_avx2 (sym indpt 2x)   |          |  3.783 |  3.813<br/>( 3.746 ~  3.879) |  1132.15 |  1123.46<br/>( 1104.29 ~  1143.30) |
rANS32x32 multi block 13<br/>encode_basic                 |  12.70 % |  7.029 |  7.095<br/>( 7.017 ~  7.172) |   609.39 |   603.75<br/>(  597.24 ~   610.39) |
rANS32x32 multi block 13<br/>decode_basic                 |          |  5.924 |  5.998<br/>( 5.927 ~  6.069) |   723.05 |   714.13<br/>(  705.78 ~   722.67) |
rANS32x32 multi block 13<br/>decode_avx2 (sym dpndt)      |          |  3.689 |  3.819<br/>( 3.435 ~  4.204) |  1161.23 |  1121.54<br/>( 1018.97 ~  1247.08) |
rANS32x32 multi block 13<br/>decode_avx2 (sym dpndt 2x)   |          |  3.492 |  3.524<br/>( 3.460 ~  3.588) |  1226.59 |  1215.40<br/>( 1193.67 ~  1237.93) |
rANS32x32 multi block 13<br/>decode_avx2 (sym indpt)      |          |  3.757 |  3.916<br/>( 3.356 ~  4.477) |  1140.16 |  1093.70<br/>(  956.69 ~  1276.51) |
rANS32x32 multi block 13<br/>decode_avx2 (sym indpt 2x)   |          |  3.619 |  3.636<br/>( 3.625 ~  3.647) |  1183.47 |  1178.12<br/>( 1174.58 ~  1181.69) |
rANS32x32 multi block 12<br/>encode_basic                 |  12.99 % |  7.055 |  7.087<br/>( 7.067 ~  7.108) |   607.15 |   604.36<br/>(  602.58 ~   606.14) |
rANS32x32 multi block 12<br/>decode_basic                 |          |  5.969 |  6.035<br/>( 6.000 ~  6.070) |   717.58 |   709.76<br/>(  705.62 ~   713.94) |
rANS32x32 multi block 12<br/>decode_avx2 (sym dpndt)      |          |  3.683 |  3.748<br/>( 3.640 ~  3.857) |  1162.93 |  1142.78<br/>( 1110.67 ~  1176.80) |
rANS32x32 multi block 12<br/>decode_avx2 (sym dpndt 2x)   |          |  3.485 |  3.623<br/>( 3.214 ~  4.032) |  1228.91 |  1182.30<br/>( 1062.43 ~  1332.66) |
rANS32x32 multi block 12<br/>decode_avx2 (sym indpt)      |          |  3.663 |  3.821<br/>( 3.256 ~  4.385) |  1169.22 |  1121.05<br/>(  976.73 ~  1315.43) |
rANS32x32 multi block 12<br/>decode_avx2 (sym indpt 2x)   |          |  3.553 |  3.593<br/>( 3.514 ~  3.673) |  1205.48 |  1192.04<br/>( 1166.26 ~  1218.98) |
rANS32x32 multi block 12<br/>decode_avx2 (sngl gathr)     |          |  2.618 |  2.634<br/>( 2.620 ~  2.647) |  1636.36 |  1626.22<br/>( 1617.94 ~  1634.59) |
rANS32x32 multi block 12<br/>decode_avx2 (sngl gathr 2x)  |          |  2.569 |  2.590<br/>( 2.579 ~  2.600) |  1667.31 |  1654.08<br/>( 1647.36 ~  1660.85) |
rANS32x32 multi block 11<br/>encode_basic                 |  13.73 % |  7.110 |  7.133<br/>( 7.115 ~  7.151) |   602.42 |   600.50<br/>(  599.00 ~   602.01) |
rANS32x32 multi block 11<br/>decode_basic                 |          |  6.021 |  6.187<br/>( 5.723 ~  6.651) |   711.36 |   692.28<br/>(  644.00 ~   748.39) |
rANS32x32 multi block 11<br/>decode_avx2 (sym dpndt)      |          |  3.745 |  3.887<br/>( 3.381 ~  4.392) |  1143.62 |  1102.10<br/>(  975.23 ~  1266.93) |
rANS32x32 multi block 11<br/>decode_avx2 (sym dpndt 2x)   |          |  3.507 |  3.591<br/>( 3.379 ~  3.803) |  1221.21 |  1192.70<br/>( 1126.26 ~  1267.47) |
rANS32x32 multi block 11<br/>decode_avx2 (sym indpt)      |          |  3.685 |  3.715<br/>( 3.697 ~  3.733) |  1162.52 |  1152.91<br/>( 1147.36 ~  1158.52) |
rANS32x32 multi block 11<br/>decode_avx2 (sym indpt 2x)   |          |  3.545 |  3.568<br/>( 3.553 ~  3.583) |  1208.14 |  1200.42<br/>( 1195.33 ~  1205.55) |
rANS32x32 multi block 11<br/>decode_avx2 (sngl gathr)     |          |  2.670 |  2.681<br/>( 2.674 ~  2.689) |  1604.10 |  1597.39<br/>( 1593.11 ~  1601.69) |
rANS32x32 multi block 11<br/>decode_avx2 (sngl gathr 2x)  |          |  2.568 |  2.607<br/>( 2.582 ~  2.633) |  1667.95 |  1642.82<br/>( 1626.88 ~  1659.08) |
rANS32x32 multi block 10<br/>encode_basic                 |  16.34 % |  7.223 |  7.295<br/>( 7.230 ~  7.359) |   593.02 |   587.20<br/>(  582.02 ~   592.47) |
rANS32x32 multi block 10<br/>decode_basic                 |          |  6.119 |  6.177<br/>( 6.107 ~  6.247) |   700.04 |   693.43<br/>(  685.66 ~   701.37) |
rANS32x32 multi block 10<br/>decode_avx2 (sym dpndt)      |          |  3.738 |  3.842<br/>( 3.527 ~  4.157) |  1145.90 |  1114.89<br/>( 1030.48 ~  1214.36) |
rANS32x32 multi block 10<br/>decode_avx2 (sym dpndt 2x)   |          |  3.526 |  3.547<br/>( 3.531 ~  3.562) |  1214.68 |  1207.68<br/>( 1202.35 ~  1213.06) |
rANS32x32 multi block 10<br/>decode_avx2 (sym indpt)      |          |  3.716 |  3.744<br/>( 3.727 ~  3.761) |  1152.62 |  1144.11<br/>( 1138.91 ~  1149.36) |
rANS32x32 multi block 10<br/>decode_avx2 (sym indpt 2x)   |          |  3.576 |  3.602<br/>( 3.583 ~  3.621) |  1197.83 |  1189.28<br/>( 1183.05 ~  1195.58) |
rANS32x32 multi block 10<br/>decode_avx2 (sngl gathr)     |          |  2.690 |  2.953<br/>( 2.365 ~  3.542) |  1592.17 |  1450.28<br/>( 1209.28 ~  1811.24) |
rANS32x32 multi block 10<br/>decode_avx2 (sngl gathr 2x)  |          |  2.576 |  2.676<br/>( 2.397 ~  2.954) |  1662.92 |  1600.89<br/>( 1449.93 ~  1786.95) |

### Clang (via WSL2)
| Codec Type | Ratio | Minimum<br/>Clocks/Byte | Average (StdDev.) | Maximum<br/>Throughput<br/>MiB/s | Average (StdDev.) |
| -- | --: | --: | --: | --: | --: |
rANS32x32 multi block 15<br/>encode_basic                 |  12.58 % |  6.909 |  6.959<br/>( 6.927 ~  6.990) |   619.97 |   615.55<br/>(  612.79 ~   618.35) |
rANS32x32 multi block 15<br/>decode_basic                 |          |  6.368 |  6.409<br/>( 6.380 ~  6.439) |   672.68 |   668.31<br/>(  665.23 ~   671.42) |
rANS32x32 multi block 15<br/>decode_avx2 (sym dpndt)      |          |  3.367 |  3.393<br/>( 3.371 ~  3.415) |  1272.22 |  1262.44<br/>( 1254.17 ~  1270.81) |
rANS32x32 multi block 15<br/>decode_avx2 (sym dpndt 2x)   |          |  3.196 |  3.224<br/>( 3.208 ~  3.240) |  1340.24 |  1328.71<br/>( 1322.08 ~  1335.40) |
rANS32x32 multi block 15<br/>decode_avx2 (sym indpt)      |          |  3.863 |  3.894<br/>( 3.879 ~  3.909) |  1108.87 |  1100.08<br/>( 1095.86 ~  1104.33) |
rANS32x32 multi block 15<br/>decode_avx2 (sym indpt 2x)   |          |  3.889 |  3.911<br/>( 3.898 ~  3.925) |  1101.54 |  1095.12<br/>( 1091.33 ~  1098.94) |
rANS32x32 multi block 14<br/>encode_basic                 |  12.61 % |  7.057 |  7.101<br/>( 7.077 ~  7.125) |   606.95 |   603.20<br/>(  601.13 ~   605.28) |
rANS32x32 multi block 14<br/>decode_basic                 |          |  6.281 |  6.354<br/>( 6.311 ~  6.396) |   681.96 |   674.16<br/>(  669.66 ~   678.72) |
rANS32x32 multi block 14<br/>decode_avx2 (sym dpndt)      |          |  3.299 |  3.323<br/>( 3.305 ~  3.340) |  1298.26 |  1289.14<br/>( 1282.31 ~  1296.05) |
rANS32x32 multi block 14<br/>decode_avx2 (sym dpndt 2x)   |          |  3.136 |  3.163<br/>( 3.141 ~  3.185) |  1365.72 |  1354.00<br/>( 1344.62 ~  1363.52) |
rANS32x32 multi block 14<br/>decode_avx2 (sym indpt)      |          |  3.638 |  3.658<br/>( 3.643 ~  3.672) |  1177.45 |  1171.07<br/>( 1166.45 ~  1175.73) |
rANS32x32 multi block 14<br/>decode_avx2 (sym indpt 2x)   |          |  3.658 |  3.692<br/>( 3.669 ~  3.714) |  1171.03 |  1160.28<br/>( 1153.21 ~  1167.44) |
rANS32x32 multi block 13<br/>encode_basic                 |  12.70 % |  6.968 |  7.007<br/>( 6.980 ~  7.035) |   614.76 |   611.26<br/>(  608.87 ~   613.68) |
rANS32x32 multi block 13<br/>decode_basic                 |          |  6.317 |  6.389<br/>( 6.337 ~  6.440) |   678.11 |   670.45<br/>(  665.07 ~   675.91) |
rANS32x32 multi block 13<br/>decode_avx2 (sym dpndt)      |          |  3.316 |  3.353<br/>( 3.333 ~  3.372) |  1291.86 |  1277.61<br/>( 1270.23 ~  1285.09) |
rANS32x32 multi block 13<br/>decode_avx2 (sym dpndt 2x)   |          |  3.140 |  3.167<br/>( 3.153 ~  3.180) |  1363.88 |  1352.64<br/>( 1346.90 ~  1358.43) |
rANS32x32 multi block 13<br/>decode_avx2 (sym indpt)      |          |  3.460 |  3.481<br/>( 3.467 ~  3.495) |  1237.94 |  1230.49<br/>( 1225.44 ~  1235.59) |
rANS32x32 multi block 13<br/>decode_avx2 (sym indpt 2x)   |          |  3.477 |  3.497<br/>( 3.486 ~  3.508) |  1231.98 |  1224.81<br/>( 1220.87 ~  1228.77) |
rANS32x32 multi block 12<br/>encode_basic                 |  12.99 % |  7.120 |  7.177<br/>( 7.133 ~  7.221) |   601.63 |   596.82<br/>(  593.18 ~   600.50) |
rANS32x32 multi block 12<br/>decode_basic                 |          |  6.258 |  6.324<br/>( 6.282 ~  6.366) |   684.47 |   677.34<br/>(  672.87 ~   681.88) |
rANS32x32 multi block 12<br/>decode_avx2 (sym dpndt)      |          |  3.327 |  3.358<br/>( 3.342 ~  3.374) |  1287.58 |  1275.50<br/>( 1269.36 ~  1281.69) |
rANS32x32 multi block 12<br/>decode_avx2 (sym dpndt 2x)   |          |  3.133 |  3.161<br/>( 3.143 ~  3.179) |  1367.14 |  1355.06<br/>( 1347.42 ~  1362.78) |
rANS32x32 multi block 12<br/>decode_avx2 (sym indpt)      |          |  3.357 |  3.382<br/>( 3.366 ~  3.398) |  1275.89 |  1266.49<br/>( 1260.61 ~  1272.42) |
rANS32x32 multi block 12<br/>decode_avx2 (sym indpt 2x)   |          |  3.227 |  3.249<br/>( 3.232 ~  3.266) |  1327.18 |  1318.35<br/>( 1311.67 ~  1325.09) |
rANS32x32 multi block 12<br/>decode_avx2 (sngl gathr)     |          |  2.517 |  2.533<br/>( 2.520 ~  2.545) |  1701.49 |  1691.29<br/>( 1683.09 ~  1699.57) |
rANS32x32 multi block 12<br/>decode_avx2 (sngl gathr 2x)  |          |  2.461 |  2.477<br/>( 2.464 ~  2.489) |  1740.33 |  1729.48<br/>( 1720.92 ~  1738.13) |
rANS32x32 multi block 11<br/>encode_basic                 |  13.73 % |  7.144 |  7.187<br/>( 7.166 ~  7.207) |   599.58 |   596.02<br/>(  594.33 ~   597.71) |
rANS32x32 multi block 11<br/>decode_basic                 |          |  6.314 |  6.369<br/>( 6.335 ~  6.404) |   678.43 |   672.49<br/>(  668.87 ~   676.16) |
rANS32x32 multi block 11<br/>decode_avx2 (sym dpndt)      |          |  3.315 |  3.358<br/>( 3.329 ~  3.388) |  1292.24 |  1275.44<br/>( 1264.44 ~  1286.62) |
rANS32x32 multi block 11<br/>decode_avx2 (sym dpndt 2x)   |          |  3.155 |  3.186<br/>( 3.160 ~  3.211) |  1357.67 |  1344.61<br/>( 1333.94 ~  1355.46) |
rANS32x32 multi block 11<br/>decode_avx2 (sym indpt)      |          |  3.360 |  3.386<br/>( 3.365 ~  3.407) |  1274.74 |  1265.03<br/>( 1257.11 ~  1273.06) |
rANS32x32 multi block 11<br/>decode_avx2 (sym indpt 2x)   |          |  3.214 |  3.239<br/>( 3.224 ~  3.254) |  1332.64 |  1322.39<br/>( 1316.18 ~  1328.66) |
rANS32x32 multi block 11<br/>decode_avx2 (sngl gathr)     |          |  2.491 |  2.525<br/>( 2.509 ~  2.542) |  1719.58 |  1696.05<br/>( 1684.87 ~  1707.37) |
rANS32x32 multi block 11<br/>decode_avx2 (sngl gathr 2x)  |          |  2.433 |  2.451<br/>( 2.436 ~  2.465) |  1760.30 |  1747.73<br/>( 1737.30 ~  1758.29) |
rANS32x32 multi block 10<br/>encode_basic                 |  16.34 % |  7.219 |  7.292<br/>( 7.243 ~  7.342) |   593.32 |   587.37<br/>(  583.42 ~   591.37) |
rANS32x32 multi block 10<br/>decode_basic                 |          |  6.411 |  6.607<br/>( 6.440 ~  6.774) |   668.10 |   648.32<br/>(  632.32 ~   665.16) |
rANS32x32 multi block 10<br/>decode_avx2 (sym dpndt)      |          |  3.343 |  3.398<br/>( 3.364 ~  3.433) |  1281.41 |  1260.46<br/>( 1247.86 ~  1273.33) |
rANS32x32 multi block 10<br/>decode_avx2 (sym dpndt 2x)   |          |  3.131 |  3.178<br/>( 3.151 ~  3.205) |  1368.06 |  1347.76<br/>( 1336.33 ~  1359.39) |
rANS32x32 multi block 10<br/>decode_avx2 (sym indpt)      |          |  3.375 |  3.396<br/>( 3.384 ~  3.408) |  1269.15 |  1261.39<br/>( 1256.89 ~  1265.92) |
rANS32x32 multi block 10<br/>decode_avx2 (sym indpt 2x)   |          |  3.221 |  3.245<br/>( 3.229 ~  3.262) |  1329.62 |  1319.77<br/>( 1313.17 ~  1326.43) |
rANS32x32 multi block 10<br/>decode_avx2 (sngl gathr)     |          |  2.517 |  2.541<br/>( 2.528 ~  2.553) |  1701.48 |  1685.87<br/>( 1677.76 ~  1694.07) |
rANS32x32 multi block 10<br/>decode_avx2 (sngl gathr 2x)  |          |  2.439 |  2.472<br/>( 2.449 ~  2.494) |  1755.90 |  1732.94<br/>( 1717.17 ~  1749.00) |

## License
Two Clause BSD
