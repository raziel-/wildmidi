/*
 internal_midi.c

 Midi Wavetable Processing library

 Copyright (C) WildMIDI Developers 2001-2014

 This file is part of WildMIDI.

 WildMIDI is free software: you can redistribute and/or modify the player
 under the terms of the GNU General Public License and you can redistribute
 and/or modify the library under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation, either version 3 of
 the licenses, or(at your option) any later version.

 WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 the GNU Lesser General Public License for more details.

 You should have received a copy of the GNU General Public License and the
 GNU Lesser General Public License along with WildMIDI.  If not,  see
 <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "lock.h"
#include "wm_error.h"
#include "reverb.h"
#include "sample.h"
#include "wildmidi_lib.h"
#include "patches.h"
#include "internal_midi.h"


#define HOLD_OFF 0x02

#ifdef DEBUG_MIDI
#define MIDI_EVENT_DEBUG(dx,dy) printf("\r%s, %x\n",dx,dy)
#else
#define MIDI_EVENT_DEBUG(dx,dy)
#endif

/* f: ( VOLUME / 127.0 ) * 1024.0 */
int16_t _WM_lin_volume[] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72,
    80, 88, 96, 104, 112, 120, 129, 137, 145, 153, 161, 169, 177, 185, 193,
    201, 209, 217, 225, 233, 241, 249, 258, 266, 274, 282, 290, 298, 306,
    314, 322, 330, 338, 346, 354, 362, 370, 378, 387, 395, 403, 411, 419,
    427, 435, 443, 451, 459, 467, 475, 483, 491, 499, 507, 516, 524, 532,
    540, 548, 556, 564, 572, 580, 588, 596, 604, 612, 620, 628, 636, 645,
    653, 661, 669, 677, 685, 693, 701, 709, 717, 725, 733, 741, 749, 757,
    765, 774, 782, 790, 798, 806, 814, 822, 830, 838, 846, 854, 862, 870,
    878, 886, 894, 903, 911, 919, 927, 935, 943, 951, 959, 967, 975, 983,
    991, 999, 1007, 1015, 1024 };

/* f: pow(( VOLUME / 127.0 ), 2.0 ) * 1024.0 */
static int16_t sqr_volume[] = { 0, 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 7, 9,
    10, 12, 14, 16, 18, 20, 22, 25, 27, 30, 33, 36, 39, 42, 46, 49, 53, 57,
    61, 65, 69, 73, 77, 82, 86, 91, 96, 101, 106, 111, 117, 122, 128, 134,
    140, 146, 152, 158, 165, 171, 178, 185, 192, 199, 206, 213, 221, 228,
    236, 244, 251, 260, 268, 276, 284, 293, 302, 311, 320, 329, 338, 347,
    357, 366, 376, 386, 396, 406, 416, 426, 437, 447, 458, 469, 480, 491,
    502, 514, 525, 537, 549, 560, 572, 585, 597, 609, 622, 634, 647, 660,
    673, 686, 699, 713, 726, 740, 754, 768, 782, 796, 810, 825, 839, 854,
    869, 884, 899, 914, 929, 944, 960, 976, 992, 1007, 1024 };

/* f: pow(( VOLUME / 127.0 ), 0.5 ) * 1024.0 */
static int16_t pan_volume[] = { 0, 90, 128, 157, 181, 203, 222, 240,
    257, 272, 287, 301, 314, 327, 339, 351, 363, 374, 385, 396, 406, 416,
    426, 435, 445, 454, 463, 472, 480, 489, 497, 505, 514, 521, 529, 537,
    545, 552, 560, 567, 574, 581, 588, 595, 602, 609, 616, 622, 629, 636,
    642, 648, 655, 661, 667, 673, 679, 686, 692, 697, 703, 709, 715, 721,
    726, 732, 738, 743, 749, 754, 760, 765, 771, 776, 781, 786, 792, 797,
    802, 807, 812, 817, 822, 827, 832, 837, 842, 847, 852, 857, 862, 866,
    871, 876, 880, 885, 890, 894, 899, 904, 908, 913, 917, 922, 926, 931,
    935, 939, 944, 948, 953, 957, 961, 965, 970, 974, 978, 982, 987, 991,
    995, 999, 1003, 1007, 1011, 1015, 1019, 1024 };

static uint32_t freq_table[] = { 837201792, 837685632, 838169728,
    838653568, 839138240, 839623232, 840108480, 840593984, 841079680,
    841565184, 842051648, 842538240, 843025152, 843512320, 843999232,
    844486976, 844975040, 845463360, 845951936, 846440320, 846929536,
    847418944, 847908608, 848398656, 848888960, 849378944, 849869824,
    850361024, 850852416, 851344192, 851835584, 852327872, 852820480,
    853313280, 853806464, 854299328, 854793024, 855287040, 855781312,
    856275904, 856770752, 857265344, 857760704, 858256448, 858752448,
    859248704, 859744768, 860241600, 860738752, 861236160, 861733888,
    862231360, 862729600, 863228160, 863727104, 864226176, 864725696,
    865224896, 865724864, 866225152, 866725760, 867226688, 867727296,
    868228736, 868730496, 869232576, 869734912, 870236928, 870739904,
    871243072, 871746560, 872250368, 872754496, 873258240, 873762880,
    874267840, 874773184, 875278720, 875783936, 876290112, 876796480,
    877303232, 877810176, 878317504, 878824512, 879332416, 879840576,
    880349056, 880857792, 881366272, 881875712, 882385280, 882895296,
    883405440, 883915456, 884426304, 884937408, 885448832, 885960512,
    886472512, 886984192, 887496768, 888009728, 888522944, 889036352,
    889549632, 890063680, 890578048, 891092736, 891607680, 892122368,
    892637952, 893153792, 893670016, 894186496, 894703232, 895219648,
    895737024, 896254720, 896772672, 897290880, 897808896, 898327744,
    898846912, 899366336, 899886144, 900405568, 900925952, 901446592,
    901967552, 902488768, 903010368, 903531584, 904053760, 904576256,
    905099008, 905622016, 906144896, 906668480, 907192512, 907716800,
    908241408, 908765632, 909290816, 909816256, 910342144, 910868160,
    911394624, 911920768, 912447680, 912975104, 913502720, 914030592,
    914558208, 915086784, 915615552, 916144768, 916674176, 917203968,
    917733440, 918263744, 918794496, 919325440, 919856704, 920387712,
    920919616, 921451840, 921984320, 922517184, 923049728, 923583168,
    924116928, 924651008, 925185344, 925720000, 926254336, 926789696,
    927325312, 927861120, 928397440, 928933376, 929470208, 930007296,
    930544768, 931082560, 931619968, 932158464, 932697152, 933236160,
    933775488, 934315072, 934854464, 935394688, 935935296, 936476224,
    937017344, 937558208, 938100160, 938642304, 939184640, 939727488,
    940269888, 940813312, 941357056, 941900992, 942445440, 942990016,
    943534400, 944079680, 944625280, 945171200, 945717440, 946263360,
    946810176, 947357376, 947904832, 948452672, 949000192, 949548608,
    950097280, 950646400, 951195776, 951745472, 952294912, 952845184,
    953395904, 953946880, 954498176, 955049216, 955601088, 956153408,
    956705920, 957258816, 957812032, 958364928, 958918848, 959472960,
    960027456, 960582272, 961136768, 961692224, 962248000, 962804032,
    963360448, 963916608, 964473600, 965031040, 965588736, 966146816,
    966705152, 967263168, 967822144, 968381440, 968941120, 969501056,
    970060736, 970621376, 971182272, 971743488, 972305088, 972866368,
    973428608, 973991104, 974554048, 975117312, 975680768, 976243968,
    976808192, 977372736, 977937536, 978502656, 979067584, 979633344,
    980199488, 980765888, 981332736, 981899200, 982466688, 983034432,
    983602624, 984171008, 984739776, 985308160, 985877632, 986447360,
    987017472, 987587904, 988157952, 988729088, 989300416, 989872192,
    990444224, 991016000, 991588672, 992161728, 992735168, 993308864,
    993882880, 994456576, 995031296, 995606336, 996181696, 996757440,
    997332800, 997909184, 998485888, 999062912, 999640256, 1000217984,
    1000795392, 1001373696, 1001952448, 1002531520, 1003110848, 1003689920,
    1004270016, 1004850304, 1005431040, 1006012160, 1006592832, 1007174592,
    1007756608, 1008339008, 1008921792, 1009504768, 1010087552, 1010671296,
    1011255360, 1011839808, 1012424576, 1013009024, 1013594368, 1014180160,
    1014766272, 1015352768, 1015938880, 1016526016, 1017113472, 1017701248,
    1018289408, 1018877824, 1019465984, 1020055104, 1020644672, 1021234496,
    1021824768, 1022414528, 1023005440, 1023596608, 1024188160, 1024780096,
    1025371584, 1025964160, 1026557120, 1027150336, 1027744000, 1028337920,
    1028931520, 1029526144, 1030121152, 1030716480, 1031312128, 1031907456,
    1032503808, 1033100480, 1033697536, 1034294912, 1034892032, 1035490048,
    1036088512, 1036687232, 1037286336, 1037885824, 1038484928, 1039085056,
    1039685632, 1040286464, 1040887680, 1041488448, 1042090368, 1042692608,
    1043295168, 1043898176, 1044501440, 1045104384, 1045708288, 1046312640,
    1046917376, 1047522368, 1048127040, 1048732800, 1049338816, 1049945280,
    1050552128, 1051158528, 1051765952, 1052373824, 1052982016, 1053590592,
    1054199424, 1054807936, 1055417600, 1056027456, 1056637760, 1057248448,
    1057858752, 1058470016, 1059081728, 1059693824, 1060306304, 1060918336,
    1061531392, 1062144896, 1062758656, 1063372928, 1063987392, 1064601664,
    1065216896, 1065832448, 1066448448, 1067064704, 1067680704, 1068297728,
    1068915136, 1069532864, 1070150976, 1070768640, 1071387520, 1072006720,
    1072626240, 1073246080, 1073866368, 1074486272, 1075107200, 1075728512,
    1076350208, 1076972160, 1077593856, 1078216704, 1078839680, 1079463296,
    1080087040, 1080710528, 1081335168, 1081960064, 1082585344, 1083211008,
    1083836928, 1084462592, 1085089280, 1085716352, 1086343936, 1086971648,
    1087599104, 1088227712, 1088856576, 1089485824, 1090115456, 1090745472,
    1091375104, 1092005760, 1092636928, 1093268352, 1093900160, 1094531584,
    1095164160, 1095796992, 1096430336, 1097064064, 1097697280, 1098331648,
    1098966400, 1099601536, 1100237056, 1100872832, 1101508224, 1102144768,
    1102781824, 1103419136, 1104056832, 1104694144, 1105332608, 1105971328,
    1106610432, 1107249920, 1107889152, 1108529408, 1109170048, 1109811072,
    1110452352, 1111094144, 1111735552, 1112377984, 1113020928, 1113664128,
    1114307712, 1114950912, 1115595264, 1116240000, 1116885120, 1117530624,
    1118175744, 1118821888, 1119468416, 1120115456, 1120762752, 1121410432,
    1122057856, 1122706176, 1123355136, 1124004224, 1124653824, 1125303040,
    1125953408, 1126604160, 1127255168, 1127906560, 1128557696, 1129209984,
    1129862528, 1130515456, 1131168768, 1131822592, 1132475904, 1133130368,
    1133785216, 1134440448, 1135096064, 1135751296, 1136407680, 1137064448,
    1137721472, 1138379008, 1139036800, 1139694336, 1140353024, 1141012096,
    1141671424, 1142331264, 1142990592, 1143651200, 1144312192, 1144973440,
    1145635200, 1146296448, 1146958976, 1147621760, 1148285056, 1148948608,
    1149612672, 1150276224, 1150940928, 1151606144, 1152271616, 1152937600,
    1153603072, 1154269824, 1154936832, 1155604352, 1156272128, 1156939648,
    1157608192, 1158277248, 1158946560, 1159616384, 1160286464, 1160956288,
    1161627264, 1162298624, 1162970240, 1163642368, 1164314112, 1164987008,
    1165660160, 1166333824, 1167007872, 1167681536, 1168356352, 1169031552,
    1169707136, 1170383104, 1171059584, 1171735552, 1172412672, 1173090304,
    1173768192, 1174446592, 1175124480, 1175803648, 1176483072, 1177163008,
    1177843328, 1178523264, 1179204352, 1179885824, 1180567680, 1181249920,
    1181932544, 1182614912, 1183298304, 1183982208, 1184666368, 1185351040,
    1186035328, 1186720640, 1187406464, 1188092672, 1188779264, 1189466368,
    1190152960, 1190840832, 1191528960, 1192217600, 1192906624, 1193595136,
    1194285056, 1194975232, 1195665792, 1196356736, 1197047296, 1197739136,
    1198431360, 1199123968, 1199816960, 1200510336, 1201203328, 1201897600,
    1202592128, 1203287040, 1203982464, 1204677504, 1205373696, 1206070272,
    1206767232, 1207464704, 1208161664, 1208859904, 1209558528, 1210257536,
    1210956928, 1211656832, 1212356224, 1213056768, 1213757952, 1214459392,
    1215161216, 1215862656, 1216565376, 1217268352, 1217971840, 1218675712,
    1219379200, 1220083840, 1220788992, 1221494528, 1222200448, 1222906752,
    1223612672, 1224319872, 1225027456, 1225735424, 1226443648, 1227151616,
    1227860864, 1228570496, 1229280512, 1229990912, 1230700928, 1231412096,
    1232123776, 1232835840, 1233548288, 1234261248, 1234973696, 1235687424,
    1236401536, 1237116032, 1237831040, 1238545536, 1239261312, 1239977472,
    1240694144, 1241411072, 1242128512, 1242845568, 1243563776, 1244282496,
    1245001600, 1245721088, 1246440192, 1247160448, 1247881216, 1248602368,
    1249324032, 1250045184, 1250767616, 1251490432, 1252213632, 1252937344,
    1253661440, 1254385152, 1255110016, 1255835392, 1256561152, 1257287424,
    1258013184, 1258740096, 1259467648, 1260195456, 1260923648, 1261651584,
    1262380800, 1263110272, 1263840256, 1264570624, 1265301504, 1266031872,
    1266763520, 1267495552, 1268227968, 1268961024, 1269693440, 1270427264,
    1271161472, 1271896064, 1272631168, 1273365760, 1274101632, 1274838016,
    1275574784, 1276311808, 1277049472, 1277786624, 1278525056, 1279264000,
    1280003328, 1280743040, 1281482368, 1282222976, 1282963968, 1283705344,
    1284447232, 1285188736, 1285931392, 1286674560, 1287418240, 1288162176,
    1288906624, 1289650688, 1290395904, 1291141760, 1291887872, 1292634496,
    1293380608, 1294128128, 1294875904, 1295624320, 1296373120, 1297122304,
    1297870976, 1298621056, 1299371520, 1300122496, 1300873856, 1301624832,
    1302376960, 1303129600, 1303882752, 1304636288, 1305389312, 1306143872,
    1306898688, 1307654016, 1308409600, 1309165696, 1309921536, 1310678528,
    1311435904, 1312193920, 1312952192, 1313710080, 1314469248, 1315228928,
    1315988992, 1316749568, 1317509632, 1318271104, 1319032960, 1319795200,
    1320557952, 1321321088, 1322083840, 1322847872, 1323612416, 1324377216,
    1325142656, 1325907584, 1326673920, 1327440512, 1328207744, 1328975360,
    1329742464, 1330510976, 1331279872, 1332049152, 1332819072, 1333589248,
    1334359168, 1335130240, 1335901824, 1336673920, 1337446400, 1338218368,
    1338991744, 1339765632, 1340539904, 1341314560, 1342088832, 1342864512,
    1343640576, 1344417024, 1345193984, 1345971456, 1346748416, 1347526656,
    1348305408, 1349084672, 1349864320, 1350643456, 1351424000, 1352205056,
    1352986496, 1353768448, 1354550784, 1355332608, 1356115968, 1356899712,
    1357683840, 1358468480, 1359252608, 1360038144, 1360824192, 1361610624,
    1362397440, 1363183872, 1363971712, 1364760064, 1365548672, 1366337792,
    1367127424, 1367916672, 1368707200, 1369498240, 1370289664, 1371081472,
    1371873024, 1372665856, 1373459072, 1374252800, 1375047040, 1375840768,
    1376635904, 1377431552, 1378227584, 1379024000, 1379820928, 1380617472,
    1381415296, 1382213760, 1383012480, 1383811840, 1384610560, 1385410816,
    1386211456, 1387012480, 1387814144, 1388615168, 1389417728, 1390220672,
    1391024128, 1391827968, 1392632320, 1393436288, 1394241536, 1395047296,
    1395853568, 1396660224, 1397466368, 1398274048, 1399082112, 1399890688,
    1400699648, 1401508224, 1402318080, 1403128576, 1403939456, 1404750848,
    1405562624, 1406374016, 1407186816, 1408000000, 1408813696, 1409627904,
    1410441728, 1411256704, 1412072320, 1412888320, 1413704960, 1414521856,
    1415338368, 1416156288, 1416974720, 1417793664, 1418612992, 1419431808,
    1420252160, 1421072896, 1421894144, 1422715904, 1423537280, 1424359808,
    1425183104, 1426006784, 1426830848, 1427655296, 1428479488, 1429305088,
    1430131072, 1430957568, 1431784576, 1432611072, 1433438976, 1434267392,
    1435096192, 1435925632, 1436754432, 1437584768, 1438415616, 1439246848,
    1440078720, 1440910848, 1441742720, 1442575872, 1443409664, 1444243584,
    1445078400, 1445912576, 1446748032, 1447584256, 1448420864, 1449257856,
    1450094464, 1450932480, 1451771008, 1452609920, 1453449472, 1454289408,
    1455128960, 1455969920, 1456811264, 1457653248, 1458495616, 1459337600,
    1460180864, 1461024768, 1461869056, 1462713984, 1463558272, 1464404096,
    1465250304, 1466097152, 1466944384, 1467792128, 1468639488, 1469488256,
    1470337408, 1471187200, 1472037376, 1472887168, 1473738368, 1474589952,
    1475442304, 1476294912, 1477148160, 1478000768, 1478854912, 1479709696,
    1480564608, 1481420288, 1482275456, 1483132160, 1483989248, 1484846976,
    1485704960, 1486562688, 1487421696, 1488281344, 1489141504, 1490002048,
    1490863104, 1491723776, 1492585856, 1493448448, 1494311424, 1495175040,
    1496038144, 1496902656, 1497767808, 1498633344, 1499499392, 1500365056,
    1501232128, 1502099712, 1502967808, 1503836416, 1504705536, 1505574016,
    1506444032, 1507314688, 1508185856, 1509057408, 1509928576, 1510801280,
    1511674240, 1512547840, 1513421952, 1514295680, 1515170816, 1516046464,
    1516922624, 1517799296, 1518676224, 1519552896, 1520431104, 1521309824,
    1522188928, 1523068800, 1523948032, 1524828672, 1525709824, 1526591616,
    1527473792, 1528355456, 1529238784, 1530122496, 1531006720, 1531891712,
    1532776832, 1533661824, 1534547968, 1535434880, 1536322304, 1537210112,
    1538097408, 1538986368, 1539875840, 1540765696, 1541656192, 1542547072,
    1543437440, 1544329472, 1545221888, 1546114944, 1547008384, 1547901440,
    1548796032, 1549691136, 1550586624, 1551482752, 1552378368, 1553275520,
    1554173184, 1555071232, 1555970048, 1556869248, 1557767936, 1558668288,
    1559568896, 1560470272, 1561372032, 1562273408, 1563176320, 1564079616,
    1564983424, 1565888000, 1566791808, 1567697408, 1568603392, 1569509760,
    1570416896, 1571324416, 1572231424, 1573140096, 1574049152, 1574958976,
    1575869184, 1576778752, 1577689984, 1578601728, 1579514112, 1580426880,
    1581339264, 1582253056, 1583167488, 1584082432, 1584997888, 1585913984,
    1586829440, 1587746304, 1588663936, 1589582080, 1590500736, 1591418880,
    1592338560, 1593258752, 1594179584, 1595100928, 1596021632, 1596944000,
    1597866880, 1598790272, 1599714304, 1600638848, 1601562752, 1602488320,
    1603414272, 1604340992, 1605268224, 1606194816, 1607123072, 1608051968,
    1608981120, 1609911040, 1610841344, 1611771264, 1612702848, 1613634688,
    1614567168, 1615500288, 1616432896, 1617367040, 1618301824, 1619237120,
    1620172800, 1621108096, 1622044928, 1622982272, 1623920128, 1624858752,
    1625797632, 1626736256, 1627676416, 1628616960, 1629558272, 1630499968,
    1631441152, 1632384000, 1633327232, 1634271232, 1635215744, 1636159744,
    1637105152, 1638051328, 1638998016, 1639945088, 1640892928, 1641840128,
    1642788992, 1643738368, 1644688384, 1645638784, 1646588672, 1647540352,
    1648492416, 1649445120, 1650398464, 1651351168, 1652305408, 1653260288,
    1654215808, 1655171712, 1656128256, 1657084288, 1658041856, 1659000064,
    1659958784, 1660918272, 1661876992, 1662837376, 1663798400, 1664759936,
    1665721984, 1666683520, 1667646720, 1668610560, 1669574784, 1670539776,
    1671505024, 1672470016, 1673436544, };


void _WM_CheckEventMemoryPool(struct _mdi *mdi) {
	if (mdi->event_count >= mdi->events_size) {
		mdi->events_size += MEM_CHUNK;
		mdi->events = realloc(mdi->events,
                              (mdi->events_size * sizeof(struct _event)));
	}
}

void _WM_do_note_off_extra(struct _note *nte) {
    
	nte->is_off = 0;
    
	if (nte->hold) {
		nte->hold |= HOLD_OFF;
	} else {
		if (!(nte->modes & SAMPLE_ENVELOPE)) {
			if (nte->modes & SAMPLE_LOOP) {
				nte->modes ^= SAMPLE_LOOP;
			}
			nte->env_inc = 0;
            
		} else if (nte->modes & SAMPLE_CLAMPED) {
			if (nte->env < 5) {
				nte->env = 5;
				if (nte->env_level > nte->sample->env_target[5]) {
					nte->env_inc = -nte->sample->env_rate[5];
				} else {
					nte->env_inc = nte->sample->env_rate[5];
				}
			}
#if 1
		} else if (nte->modes & SAMPLE_SUSTAIN) {
			if (nte->env < 3) {
				nte->env = 3;
				if (nte->env_level > nte->sample->env_target[3]) {
					nte->env_inc = -nte->sample->env_rate[3];
				} else {
					nte->env_inc = nte->sample->env_rate[3];
				}
			}
#endif
		} else if (nte->env < 4) {
			nte->env = 4;
			if (nte->env_level > nte->sample->env_target[4]) {
				nte->env_inc = -nte->sample->env_rate[4];
			} else {
				nte->env_inc = nte->sample->env_rate[4];
			}
		}
	}
}


void _WM_do_midi_divisions(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record divisions in the event stream
    // for conversion function _WM_Event2Midi()
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

void _WM_do_note_off(struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	uint8_t ch = data->channel;
    
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
    
	nte = &mdi->note_table[0][ch][(data->data >> 8)];
	if (!nte->active)
		nte = &mdi->note_table[1][ch][(data->data >> 8)];
	if (!nte->active) {
		return;
	}
    
	if ((mdi->channel[ch].isdrum) && (!(nte->modes & SAMPLE_LOOP))) {
		return;
	}
    
	if (nte->env == 0) {
		nte->is_off = 1;
	} else {
		_WM_do_note_off_extra(nte);
	}
}

static inline uint32_t get_inc(struct _mdi *mdi, struct _note *nte) {
	int ch = nte->noteid >> 8;
	int32_t note_f;
	uint32_t freq;
    
	if (__builtin_expect((nte->patch->note != 0), 0)) {
		note_f = nte->patch->note * 100;
	} else {
		note_f = (nte->noteid & 0x7f) * 100;
	}
	note_f += mdi->channel[ch].pitch_adjust;
	if (__builtin_expect((note_f < 0), 0)) {
		note_f = 0;
	} else if (__builtin_expect((note_f > 12700), 0)) {
		note_f = 12700;
	}
	freq = freq_table[(note_f % 1200)] >> (10 - (note_f / 1200));
	return (((freq / ((_WM_SampleRate * 100) / 1024)) * 1024
             / nte->sample->inc_div));
}

uint32_t _WM_get_volume(struct _mdi *mdi, uint8_t ch, struct _note *nte) {
	int32_t volume;
    
	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {
		volume = (sqr_volume[mdi->channel[ch].volume]
                  * sqr_volume[mdi->channel[ch].expression]
                  * sqr_volume[nte->velocity]) / 1048576;
	} else {
		volume = (_WM_lin_volume[mdi->channel[ch].volume]
                  * _WM_lin_volume[mdi->channel[ch].expression]
                  * _WM_lin_volume[nte->velocity]) / 1048576;
	}
    
	volume = volume * nte->patch->amp / 100;
	return (volume);
}

void _WM_do_note_on(struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	struct _note *prev_nte;
	struct _note *nte_array;
	uint32_t freq = 0;
	struct _patch *patch;
	struct _sample *sample;
	uint8_t ch = data->channel;
	uint8_t note = (data->data >> 8);
	uint8_t velocity = (data->data & 0xFF);
    
	if (velocity == 0x00) {
		_WM_do_note_off(mdi, data);
		return;
	}
    
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
    
	if (!mdi->channel[ch].isdrum) {
		patch = mdi->channel[ch].patch;
		if (patch == NULL) {
			return;
		}
		freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
	} else {
		patch = _WM_get_patch_data(mdi,
                               ((mdi->channel[ch].bank << 8) | note | 0x80));
		if (patch == NULL) {
			return;
		}
		if (patch->note) {
			freq = freq_table[(patch->note % 12) * 100]
            >> (10 - (patch->note / 12));
		} else {
			freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
		}
	}
    
	sample = _WM_get_sample_data(patch, (freq / 100));
	if (sample == NULL) {
		return;
	}
    
	nte = &mdi->note_table[0][ch][note];
    
	if (nte->active) {
		if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3)
            && (!(nte->hold & HOLD_OFF)))
			return;
		nte->replay = &mdi->note_table[1][ch][note];
		nte->env = 6;
		nte->env_inc = -nte->sample->env_rate[6];
		nte = nte->replay;
	} else {
		if (mdi->note_table[1][ch][note].active) {
			if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3)
                && (!(nte->hold & HOLD_OFF)))
				return;
			mdi->note_table[1][ch][note].replay = nte;
			mdi->note_table[1][ch][note].env = 6;
			mdi->note_table[1][ch][note].env_inc =
            -mdi->note_table[1][ch][note].sample->env_rate[6];
		} else {
			nte_array = mdi->note;
			if (nte_array == NULL) {
				mdi->note = nte;
			} else {
				do {
					prev_nte = nte_array;
					nte_array = nte_array->next;
				} while (nte_array);
				prev_nte->next = nte;
			}
			nte->active = 1;
			nte->next = NULL;
		}
	}
	nte->noteid = (ch << 8) | note;
	nte->patch = patch;
	nte->sample = sample;
	nte->sample_pos = 0;
	nte->sample_inc = get_inc(mdi, nte);
	nte->velocity = velocity;
	nte->env = 0;
	nte->env_inc = nte->sample->env_rate[0];
	nte->env_level = 0;
	nte->modes = sample->modes;
	nte->hold = mdi->channel[ch].hold;
	nte->vol_lvl = _WM_get_volume(mdi, ch, nte);
	nte->replay = NULL;
	nte->is_off = 0;
}

void _WM_do_aftertouch(struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	uint8_t ch = data->channel;
    
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
    
	nte = &mdi->note_table[0][ch][(data->data >> 8)];
	if (!nte->active) {
		nte = &mdi->note_table[1][ch][(data->data >> 8)];
		if (!nte->active) {
			return;
		}
	}
    
	nte->velocity = data->data & 0xff;
	nte->vol_lvl = _WM_get_volume(mdi, ch, nte);
    
	if (nte->replay) {
		nte->replay->velocity = data->data & 0xff;
		nte->replay->vol_lvl = _WM_get_volume(mdi, ch, nte->replay);
	}
}

void _WM_do_pan_adjust(struct _mdi *mdi, uint8_t ch) {
	int16_t pan_adjust = mdi->channel[ch].balance
    + mdi->channel[ch].pan;
	int16_t left, right;
	int amp = 32;
    
	if (pan_adjust > 63) {
		pan_adjust = 63;
	} else if (pan_adjust < -64) {
		pan_adjust = -64;
	}
    
	pan_adjust += 64;
    /*	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {*/
	left = (pan_volume[127 - pan_adjust] * _WM_MasterVolume * amp) / 1048576;
	right = (pan_volume[pan_adjust] * _WM_MasterVolume * amp) / 1048576;
    /*	} else {
     left = (_WM_lin_volume[127 - pan_adjust] * _WM_MasterVolume * amp) / 1048576;
     right= (_WM_lin_volume[pan_adjust] * _WM_MasterVolume * amp) / 1048576;
     }*/
    
	mdi->channel[ch].left_adjust = left;
	mdi->channel[ch].right_adjust = right;
}

void _WM_do_control_bank_select(struct _mdi *mdi, struct _event_data *data) {
	uint8_t ch = data->channel;
	mdi->channel[ch].bank = data->data;
}

void _WM_do_control_data_entry_course(struct _mdi *mdi,
                                         struct _event_data *data) {
	uint8_t ch = data->channel;
	int data_tmp;
    
	if ((mdi->channel[ch].reg_non == 0)
        && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		data_tmp = mdi->channel[ch].pitch_range % 100;
		mdi->channel[ch].pitch_range = data->data * 100 + data_tmp;
        /*	printf("Data Entry Course: pitch_range: %i\n\r",mdi->channel[ch].pitch_range);*/
        /*	printf("Data Entry Course: data %li\n\r",data->data);*/
	}
}

void _WM_do_control_channel_volume(struct _mdi *mdi,
                                      struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	mdi->channel[ch].volume = data->data;
    
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->vol_lvl = _WM_get_volume(mdi, ch, note_data);
				if (note_data->replay)
					note_data->replay->vol_lvl = _WM_get_volume(mdi, ch,
                                                            note_data->replay);
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

void _WM_do_control_channel_balance(struct _mdi *mdi,
                                       struct _event_data *data) {
	uint8_t ch = data->channel;
    
	mdi->channel[ch].balance = data->data - 64;
	_WM_do_pan_adjust(mdi, ch);
}

void _WM_do_control_channel_pan(struct _mdi *mdi, struct _event_data *data) {
	uint8_t ch = data->channel;
    
	mdi->channel[ch].pan = data->data - 64;
	_WM_do_pan_adjust(mdi, ch);
}

void _WM_do_control_channel_expression(struct _mdi *mdi,
                                          struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	mdi->channel[ch].expression = data->data;
    
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->vol_lvl = _WM_get_volume(mdi, ch, note_data);
				if (note_data->replay)
					note_data->replay->vol_lvl = _WM_get_volume(mdi, ch,
                                                            note_data->replay);
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

void _WM_do_control_data_entry_fine(struct _mdi *mdi,
                                       struct _event_data *data) {
	uint8_t ch = data->channel;
	int data_tmp;
    
	if ((mdi->channel[ch].reg_non == 0)
        && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		data_tmp = mdi->channel[ch].pitch_range / 100;
		mdi->channel[ch].pitch_range = (data_tmp * 100) + data->data;
        /*	printf("Data Entry Fine: pitch_range: %i\n\r",mdi->channel[ch].pitch_range);*/
        /*	printf("Data Entry Fine: data: %li\n\r", data->data);*/
	}
    
}

void _WM_do_control_channel_hold(struct _mdi *mdi, struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	if (data->data > 63) {
		mdi->channel[ch].hold = 1;
	} else {
		mdi->channel[ch].hold = 0;
		if (note_data) {
			do {
				if ((note_data->noteid >> 8) == ch) {
					if (note_data->hold & HOLD_OFF) {
						if (note_data->modes & SAMPLE_ENVELOPE) {
							if (note_data->modes & SAMPLE_CLAMPED) {
								if (note_data->env < 5) {
									note_data->env = 5;
									if (note_data->env_level
                                        > note_data->sample->env_target[5]) {
										note_data->env_inc =
                                        -note_data->sample->env_rate[5];
									} else {
										note_data->env_inc =
                                        note_data->sample->env_rate[5];
									}
								}
							} else if (note_data->env < 4) {
								note_data->env = 4;
								if (note_data->env_level
                                    > note_data->sample->env_target[4]) {
									note_data->env_inc =
                                    -note_data->sample->env_rate[4];
								} else {
									note_data->env_inc =
                                    note_data->sample->env_rate[4];
								}
							}
						} else {
							if (note_data->modes & SAMPLE_LOOP) {
								note_data->modes ^= SAMPLE_LOOP;
							}
							note_data->env_inc = 0;
						}
					}
					note_data->hold = 0x00;
				}
				note_data = note_data->next;
			} while (note_data);
		}
	}
}

void _WM_do_control_data_increment(struct _mdi *mdi,
                                      struct _event_data *data) {
	uint8_t ch = data->channel;
    
	if ((mdi->channel[ch].reg_non == 0)
        && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		if (mdi->channel[ch].pitch_range < 0x3FFF)
			mdi->channel[ch].pitch_range++;
	}
}

void _WM_do_control_data_decrement(struct _mdi *mdi,
                                      struct _event_data *data) {
	uint8_t ch = data->channel;
    
	if ((mdi->channel[ch].reg_non == 0)
        && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		if (mdi->channel[ch].pitch_range > 0)
			mdi->channel[ch].pitch_range--;
	}
}
void _WM_do_control_non_registered_param_fine(struct _mdi *mdi,
                                            struct _event_data *data) {
	uint8_t ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x3F80)
    | data->data;
    mdi->channel[ch].reg_non = 1;
}

void _WM_do_control_non_registered_param_course(struct _mdi *mdi,
                                     struct _event_data *data) {
	uint8_t ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x7F)
    | (data->data << 7);
	mdi->channel[ch].reg_non = 1;
}

void _WM_do_control_registered_param_fine(struct _mdi *mdi,
                                             struct _event_data *data) {
	uint8_t ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x3F80)
    | data->data;
	mdi->channel[ch].reg_non = 0;
}

void _WM_do_control_registered_param_course(struct _mdi *mdi,
                                               struct _event_data *data) {
	uint8_t ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x7F)
    | (data->data << 7);
	mdi->channel[ch].reg_non = 0;
}

void _WM_do_control_channel_sound_off(struct _mdi *mdi,
                                         struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->active = 0;
				if (note_data->replay) {
					note_data->replay = NULL;
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

void _WM_do_control_channel_controllers_off(struct _mdi *mdi,
                                               struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	mdi->channel[ch].expression = 127;
	mdi->channel[ch].pressure = 127;
	mdi->channel[ch].volume = 100;
	mdi->channel[ch].pan = 0;
	mdi->channel[ch].balance = 0;
	mdi->channel[ch].reg_data = 0xffff;
	mdi->channel[ch].pitch_range = 200;
	mdi->channel[ch].pitch = 0;
	mdi->channel[ch].pitch_adjust = 0;
	mdi->channel[ch].hold = 0;
	_WM_do_pan_adjust(mdi, ch);
    
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->sample_inc = get_inc(mdi, note_data);
				note_data->velocity = 0;
				note_data->vol_lvl = _WM_get_volume(mdi, ch, note_data);
				note_data->hold = 0;
                
				if (note_data->replay) {
					note_data->replay->velocity = data->data;
					note_data->replay->vol_lvl = _WM_get_volume(mdi, ch,
                                                            note_data->replay);
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

void _WM_do_control_channel_notes_off(struct _mdi *mdi,
                                         struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	if (mdi->channel[ch].isdrum)
		return;
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				if (!note_data->hold) {
					if (note_data->modes & SAMPLE_ENVELOPE) {
						if (note_data->env < 5) {
							if (note_data->env_level
                                > note_data->sample->env_target[5]) {
								note_data->env_inc =
                                -note_data->sample->env_rate[5];
							} else {
								note_data->env_inc =
                                note_data->sample->env_rate[5];
							}
							note_data->env = 5;
						}
					}
				} else {
					note_data->hold |= HOLD_OFF;
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

void _WM_do_patch(struct _mdi *mdi, struct _event_data *data) {
	uint8_t ch = data->channel;
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
	if (!mdi->channel[ch].isdrum) {
		mdi->channel[ch].patch = _WM_get_patch_data(mdi,
                                                ((mdi->channel[ch].bank << 8) | data->data));
	} else {
		mdi->channel[ch].bank = data->data;
	}
}

void _WM_do_channel_pressure(struct _mdi *mdi, struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
    
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->velocity = data->data;
				note_data->vol_lvl = _WM_get_volume(mdi, ch, note_data);
                
				if (note_data->replay) {
					note_data->replay->velocity = data->data;
					note_data->replay->vol_lvl = _WM_get_volume(mdi, ch,
                                                            note_data->replay);
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

void _WM_do_pitch(struct _mdi *mdi, struct _event_data *data) {
	struct _note *note_data = mdi->note;
	uint8_t ch = data->channel;
    
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
	mdi->channel[ch].pitch = data->data - 0x2000;
    
	if (mdi->channel[ch].pitch < 0) {
		mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range
        * mdi->channel[ch].pitch / 8192;
	} else {
		mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range
        * mdi->channel[ch].pitch / 8191;
	}
    
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->sample_inc = get_inc(mdi, note_data);
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

void _WM_do_sysex_roland_drum_track(struct _mdi *mdi,
                                       struct _event_data *data) {
	uint8_t ch = data->channel;
    
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
    
	if (data->data > 0) {
		mdi->channel[ch].isdrum = 1;
		mdi->channel[ch].patch = NULL;
	} else {
		mdi->channel[ch].isdrum = 0;
		mdi->channel[ch].patch = _WM_get_patch_data(mdi, 0);
	}
}

void _WM_do_sysex_gm_reset(struct _mdi *mdi, struct _event_data *data) {
	int i;
	for (i = 0; i < 16; i++) {
		mdi->channel[i].bank = 0;
		if (i != 9) {
			mdi->channel[i].patch = _WM_get_patch_data(mdi, 0);
		} else {
			mdi->channel[i].patch = NULL;
		}
		mdi->channel[i].hold = 0;
		mdi->channel[i].volume = 100;
		mdi->channel[i].pressure = 127;
		mdi->channel[i].expression = 127;
		mdi->channel[i].balance = 0;
		mdi->channel[i].pan = 0;
		mdi->channel[i].left_adjust = 1;
		mdi->channel[i].right_adjust = 1;
		mdi->channel[i].pitch = 0;
		mdi->channel[i].pitch_range = 200;
		mdi->channel[i].reg_data = 0xFFFF;
		mdi->channel[i].isdrum = 0;
		_WM_do_pan_adjust(mdi, i);
	}
	mdi->channel[9].isdrum = 1;
	UNUSED(data); /* NOOP, to please the compiler gods */
}

void _WM_do_sysex_roland_reset(struct _mdi *mdi, struct _event_data *data) {
    _WM_do_sysex_gm_reset(mdi,data);
}

void _WM_do_sysex_yamaha_reset(struct _mdi *mdi, struct _event_data *data) {
    _WM_do_sysex_gm_reset(mdi,data);
}

// ff 00 00
// ff 00 02 xx xx
void _WM_do_meta_sequence_number(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record sequence number in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 01 vl (xx * vl)
void _WM_do_meta_text(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record text in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 02 vl (xx * vl)
void _WM_do_meta_copyright(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record copyright in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 03 vl (xx * vl)
void _WM_do_meta_track_name(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record track name in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 04 vl (xx * vl)
void _WM_do_meta_instrument_name(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record instrument name in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 05 vl (xx * vl)
void _WM_do_meta_lyric(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record lyrics in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 06 vl (xx * vl)
void _WM_do_meta_marker(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record markers in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 07 vl (xx * vl)
void _WM_do_meta_cuepoint(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record tcuepoints in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 08 vl (xx * vl)
void _WM_do_meta_program_name(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record program name in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 09 vl (xx * vl)
void _WM_do_meta_device_name(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record device name in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}


// ff 20 01 xx
void _WM_do_meta_midi_channel(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record midi channel in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 21 01 xx
void _WM_do_meta_midi_port(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record midi port in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 2f 00
void _WM_do_meta_endoftrack(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record eot in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

// ff 51 03 xx xx xx
void _WM_do_meta_tempo(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record tempo in the event stream
    // for conversion function _WM_Event2Midi
    
    UNUSED(mdi);
    UNUSED(data);
    return;
}

void _WM_ResetToStart(struct _mdi *mdi) {
	mdi->current_event = mdi->events;
	mdi->samples_to_mix = 0;
	mdi->info.current_sample = 0;
    
	_WM_do_sysex_gm_reset(mdi, NULL);
}

int _WM_midi_setup_divisions(struct _mdi *mdi, uint32_t divisions) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_midi_divisions;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = divisions;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_midi_divisions;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = divisions;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

int _WM_midi_setup_noteoff(struct _mdi *mdi, uint8_t channel,
                              uint8_t note, uint8_t velocity) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_note_off;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = (note << 8)
        | velocity;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_note_off;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

static int midi_setup_noteon(struct _mdi *mdi, uint8_t channel,
                             uint8_t note, uint8_t velocity) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_note_on;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = (note << 8)
        | velocity;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_note_on;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
    
	if (mdi->channel[channel].isdrum)
		_WM_load_patch(mdi, ((mdi->channel[channel].bank << 8) | (note | 0x80)));
	return (0);
}

static int midi_setup_aftertouch(struct _mdi *mdi, uint8_t channel,
                                 uint8_t note, uint8_t pressure) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_aftertouch;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = (note << 8)
        | pressure;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_aftertouch;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = (note << 8) | pressure;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

static int midi_setup_control(struct _mdi *mdi, uint8_t channel,
                              uint8_t controller, uint8_t setting) {
	void (*tmp_event)(struct _mdi *mdi, struct _event_data *data) = NULL;
    
	switch (controller) {
        case 0:
            tmp_event = *_WM_do_control_bank_select;
            mdi->channel[channel].bank = setting;
            break;
        case 6:
            tmp_event = *_WM_do_control_data_entry_course;
            break;
        case 7:
            tmp_event = *_WM_do_control_channel_volume;
            mdi->channel[channel].volume = setting;
            break;
        case 8:
            tmp_event = *_WM_do_control_channel_balance;
            break;
        case 10:
            tmp_event = *_WM_do_control_channel_pan;
            break;
        case 11:
            tmp_event = *_WM_do_control_channel_expression;
            break;
        case 38:
            tmp_event = *_WM_do_control_data_entry_fine;
            break;
        case 64:
            tmp_event = *_WM_do_control_channel_hold;
            break;
        case 96:
            tmp_event = *_WM_do_control_data_increment;
            break;
        case 97:
            tmp_event = *_WM_do_control_data_decrement;
            break;
        case 98:
            tmp_event = *_WM_do_control_non_registered_param_fine;
            break;
        case 99:
            tmp_event = *_WM_do_control_non_registered_param_course;
            break;
        case 100:
            tmp_event = *_WM_do_control_registered_param_fine;
            break;
        case 101:
            tmp_event = *_WM_do_control_registered_param_course;
            break;
        case 120:
            tmp_event = *_WM_do_control_channel_sound_off;
            break;
        case 121:
            tmp_event = *_WM_do_control_channel_controllers_off;
            break;
        case 123:
            tmp_event = *_WM_do_control_channel_notes_off;
            break;
        default:
            return (0);
	}
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = tmp_event;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = setting;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = tmp_event;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = setting;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

static int midi_setup_patch(struct _mdi *mdi, uint8_t channel, uint8_t patch) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_patch;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = patch;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_patch;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = patch;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	if (mdi->channel[channel].isdrum) {
		mdi->channel[channel].bank = patch;
	} else {
		_WM_load_patch(mdi, ((mdi->channel[channel].bank << 8) | patch));
		mdi->channel[channel].patch = _WM_get_patch_data(mdi,
                                                     ((mdi->channel[channel].bank << 8) | patch));
	}
	return (0);
}

static int midi_setup_channel_pressure(struct _mdi *mdi, uint8_t channel,
                                       uint8_t pressure) {
    
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_channel_pressure;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = pressure;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_channel_pressure;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = pressure;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
    
	return (0);
}

static int midi_setup_pitch(struct _mdi *mdi, uint8_t channel, uint16_t pitch) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_pitch;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = pitch;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_pitch;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = pitch;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

static int midi_setup_sysex_roland_drum_track(struct _mdi *mdi,
                                              uint8_t channel, uint16_t setting) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event =
        *_WM_do_sysex_roland_drum_track;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = setting;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_sysex_roland_drum_track;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = setting;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
    
	if (setting > 0) {
		mdi->channel[channel].isdrum = 1;
	} else {
		mdi->channel[channel].isdrum = 0;
	}
    
	return (0);
}

static int midi_setup_sysex_gm_reset(struct _mdi *mdi) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_sysex_gm_reset;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = 0;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_sysex_roland_reset;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = 0;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

static int midi_setup_sysex_roland_reset(struct _mdi *mdi) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_sysex_roland_reset;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = 0;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_sysex_roland_reset;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = 0;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

static int midi_setup_sysex_yamaha_reset(struct _mdi *mdi) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_sysex_yamaha_reset;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = 0;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_sysex_roland_reset;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = 0;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}


static int midi_setup_midi_channel(struct _mdi *mdi, uint8_t channel) {
    if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_meta_midi_channel;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = 0;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_meta_midi_channel;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = 0;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

static int midi_setup_midi_port(struct _mdi *mdi, uint8_t port) {
    if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_meta_midi_port;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = port;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_meta_midi_port;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = port;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}


static int midi_setup_endoftrack(struct _mdi *mdi) {
    	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_meta_endoftrack;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = 0;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_meta_endoftrack;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = 0;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

int _WM_midi_setup_tempo(struct _mdi *mdi, uint32_t setting) {
	if ((mdi->event_count)
        && (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *_WM_do_meta_tempo;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = setting;
	} else {
		_WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *_WM_do_meta_tempo;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = setting;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return (0);
}

struct _mdi *
_WM_initMDI(void) {
	struct _mdi *mdi;
    
	mdi = malloc(sizeof(struct _mdi));
	memset(mdi, 0, (sizeof(struct _mdi)));
    
	mdi->info.copyright = NULL;
	mdi->info.mixer_options = _WM_MixerOptions;
    
	_WM_load_patch(mdi, 0x0000);
    
	mdi->events_size = MEM_CHUNK;
	mdi->events = malloc(mdi->events_size * sizeof(struct _event));
	mdi->events[0].do_event = NULL;
	mdi->events[0].event_data.channel = 0;
	mdi->events[0].event_data.data = 0;
	mdi->events[0].samples_to_next = 0;
	mdi->event_count++;
    
	mdi->current_event = mdi->events;
	mdi->samples_to_mix = 0;
	mdi->info.current_sample = 0;
	mdi->info.total_midi_time = 0;
	mdi->info.approx_total_samples = 0;
    
	_WM_do_sysex_roland_reset(mdi, NULL);
    
	return (mdi);
}

void _WM_freeMDI(struct _mdi *mdi) {
	struct _sample *tmp_sample;
	uint32_t i;
    
	if (mdi->patch_count != 0) {
		_WM_Lock(&_WM_patch_lock);
		for (i = 0; i < mdi->patch_count; i++) {
			mdi->patches[i]->inuse_count--;
			if (mdi->patches[i]->inuse_count == 0) {
				/* free samples here */
				while (mdi->patches[i]->first_sample) {
					tmp_sample = mdi->patches[i]->first_sample->next;
					free(mdi->patches[i]->first_sample->data);
					free(mdi->patches[i]->first_sample);
					mdi->patches[i]->first_sample = tmp_sample;
				}
				mdi->patches[i]->loaded = 0;
			}
		}
		_WM_Unlock(&_WM_patch_lock);
		free(mdi->patches);
	}
    
	free(mdi->events);
	free(mdi->tmp_info);
	_WM_free_reverb(mdi->reverb);
	free(mdi->mix_buffer);
	free(mdi);
}

#if 0
static uint32_t get_decay_samples(struct _patch *patch, uint8_t note) {
    
	struct _sample *sample = NULL;
	uint32_t freq = 0;
	uint32_t decay_samples = 0;
    
	if (patch == NULL) return (0);
    
	/* first get the freq we need so we can check the right sample */
	if (patch->patchid & 0x80) {
		/* is a drum patch */
		if (patch->note) {
			freq = freq_table[(patch->note % 12) * 100]
            >> (10 - (patch->note / 12));
		} else {
			freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
		}
	} else {
		freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
	}
    
	/* get the sample */
	sample = _WM_get_sample_data(patch, (freq / 100));
	if (sample == NULL) return (0);
    
	if (patch->patchid & 0x80) {
		float sratedata = ((float) sample->rate / (float) _WM_SampleRate)
        * (float) (sample->data_length >> 10);
		decay_samples = (uint32_t) sratedata;
        /*	printf("Drums (%i / %i) * %lu = %f\n", sample->rate, _WM_SampleRate, (sample->data_length >> 10), sratedata);*/
	} else if (sample->modes & SAMPLE_CLAMPED) {
		decay_samples = (4194303 / sample->env_rate[5]);
        /*	printf("clamped 4194303 / %lu = %lu\n", sample->env_rate[5], decay_samples);*/
	} else {
		decay_samples =
        ((4194303 - sample->env_target[4]) / sample->env_rate[4])
        + (sample->env_target[4] / sample->env_rate[5]);
        /*	printf("NOT clamped ((4194303 - %lu) / %lu) + (%lu / %lu)) = %lu\n", sample->env_target[4], sample->env_rate[4], sample->env_target[4], sample->env_rate[5], decay_samples);*/
	}
	return (decay_samples);
}
#endif

uint32_t
_WM_SetupMidiEvent(struct _mdi *mdi, uint8_t * event_data, uint8_t running_event) {
    /*
     Only add standard MIDI and Sysex events in here.
     Non-standard events need to be handled by calling function
     to avoid compatibility issues.
     
     TODO:
     Add value limit checks
     */
    uint32_t ret_cnt = 0;
    uint8_t command = 0;
    uint8_t channel = 0;
    uint8_t data_1 = 0;
    uint8_t data_2 = 0;
    
    if (event_data[0] >= 0x80) {
        command = *event_data & 0xf0;
        channel = *event_data++ & 0x0f;
        ret_cnt++;
    } else {
        command = running_event & 0xf0;
        channel = running_event & 0x0f;
    }
    
    switch(command) {
        case 0x80:
        _SETUP_NOTEOFF:
            data_1 = *event_data++;
            data_2 = *event_data++;
            _WM_midi_setup_noteoff(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0x90:
            if (event_data[1] == 0) goto _SETUP_NOTEOFF; // A velocity of 0 in a note on is actually a note off
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_noteon(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0xa0:
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_aftertouch(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0xb0:
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_control(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0xc0:
            data_1 = *event_data++;
            midi_setup_patch(mdi, channel, data_1);
            ret_cnt++;
            break;
        case 0xd0:
            data_1 = *event_data++;
            midi_setup_channel_pressure(mdi, channel, data_1);
            ret_cnt++;
            break;
        case 0xe0:
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_pitch(mdi, channel, ((data_2 << 7) | (data_1 & 0x7f)));
            ret_cnt += 2;
            break;
        case 0xf0:
            if (channel == 0x0f) {
                /*
                 MIDI Meta Events
                 */
                uint32_t tmp_length = 0;
                if (event_data[0] == 0x02) {
                    /* Copyright Event */
                    
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (*event_data > 0x7f) {
                        do {
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    
                    /* Copy copyright info in the getinfo struct */
                    if (mdi->info.copyright) {
                        mdi->info.copyright = realloc(mdi->info.copyright,(strlen(mdi->info.copyright) + 1 + tmp_length + 1));
                        memcpy(&mdi->info.copyright[strlen(mdi->info.copyright) + 1], event_data, tmp_length);
                        mdi->info.copyright[strlen(mdi->info.copyright) + 1 + tmp_length] = '\0';
                        mdi->info.copyright[strlen(mdi->info.copyright)] = '\n';
                    } else {
                        mdi->info.copyright = malloc(tmp_length + 1);
                        memcpy(mdi->info.copyright, event_data, tmp_length);
                        mdi->info.copyright[tmp_length] = '\0';
                    }
                    ret_cnt += tmp_length;
                    
                } else if ((event_data[0] == 0x2F) && (event_data[1] == 0x00)) {
                    /*
                     End of Track
                     
                     Deal with this inside calling function
                     */
                    ret_cnt += 2;
                } else if ((event_data[0] == 0x51) && (event_data[1] == 0x03)) {
                    /*
                     Tempo
                     
                     Deal with this inside calling function.
                     
                     We only setting this up here for _WM_Event2Midi function
                     */
                    _WM_midi_setup_tempo(mdi, ((event_data[2] << 16) + (event_data[3] << 8) + event_data[4]));
                    ret_cnt += 5;
                } else {
                    /*
                     Unsupported Meta Event
                     */
                    event_data++;
                    ret_cnt++;
                    if (*event_data > 0x7f) {
                        do {
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    ret_cnt++;
                    ret_cnt += tmp_length;
                }
                
            } else if ((channel == 0) || (channel == 7)) {
                /*
                 Sysex Events
                 */
                uint32_t sysex_len = 0;
                uint8_t *sysex_store = NULL;
                uint32_t sysex_store_len = 0;
                
                
                if (*event_data > 0x7f) {
                    do {
                        sysex_len = (sysex_len << 7) + (*event_data & 0x7F);
                        event_data++;
                        ret_cnt++;
                    } while (*event_data > 0x7f);
                }
                sysex_len = (sysex_len << 7) + (*event_data & 0x7F);
                event_data++;
                ret_cnt++;
                
                sysex_store = realloc(sysex_store,sizeof(uint8_t) * (sysex_store_len + sysex_len));
                memcpy(&sysex_store[sysex_store_len], event_data, sysex_len);
                sysex_store_len += sysex_len;
                
                if (sysex_store[sysex_store_len - 1] == 0xF7) {
                    uint8_t rolandsysexid[] = { 0x41, 0x10, 0x42, 0x12 };
                    if (memcmp(rolandsysexid, sysex_store, 4) == 0) {
                        // For Roland Sysex Messages
                        /* checksum */
                        uint8_t sysex_cs = 0;
                        uint32_t sysex_ofs = 4;
                        do {
                            sysex_cs += sysex_store[sysex_ofs];
                            if (sysex_cs > 0x7F) {
                                sysex_cs -= 0x80;
                            }
                            sysex_ofs++;
                        } while (sysex_store[sysex_ofs + 1] != 0xf7);
                        sysex_cs = 128 - sysex_cs;
                        /* is roland sysex message valid */
                        if (sysex_cs == sysex_store[sysex_ofs]) {
                            /* process roland sysex event */
                            if (sysex_store[4] == 0x40) {
                                if (((sysex_store[5] & 0xf0) == 0x10) && (sysex_store[6] == 0x15)) {
                                    /* Roland Drum Track Setting */
                                    uint8_t sysex_ch = 0x0f & sysex_store[5];
                                    if (sysex_ch == 0x00) {
                                        sysex_ch = 0x09;
                                    } else if (sysex_ch <= 0x09) {
                                        sysex_ch -= 1;
                                    }
                                    midi_setup_sysex_roland_drum_track(mdi, sysex_ch, sysex_store[7]);
                                } else if ((sysex_store[5] == 0x00) && (sysex_store[6] == 0x7F) && (sysex_store[7] == 0x00)) {
                                    /* Roland GS Reset */
                                    midi_setup_sysex_roland_reset(mdi);
                                }
                            }
                        }
                    } else {
                        // For non-Roland Sysex Messages
                        int8_t gm_reset[] = {0x7e, 0x7f, 0x09, 0x01, 0xf7};
                        int8_t yamaha_reset[] = {0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7};
                        
                        if (memcmp(gm_reset, sysex_store, 5) == 0) {
                            // GM Reset
                            midi_setup_sysex_gm_reset(mdi);
                        } else if (memcmp(yamaha_reset,sysex_store,8) == 0) {
                            // Yamaha Reset
                            midi_setup_sysex_yamaha_reset(mdi);
                        }
                    }
                }
                free(sysex_store);
                sysex_store = NULL;
                //                sysex_store_len = 0;
                //                event_data += sysex_len;
                ret_cnt += sysex_len;
            } else {
                _WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(unrecognized meta event)", 0);
                return 0;
            }
            
            break;
        default:
            // Should NEVER get here
            return 0;
            
    }
    return ret_cnt;
}
