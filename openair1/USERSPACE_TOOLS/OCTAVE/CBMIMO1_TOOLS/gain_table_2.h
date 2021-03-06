#ifndef _GAIN_TABLE_H__
#define _GAIN_TABLE_H__
#include "phy_implementation_defs.h"
#define TCXO_OFFSET
struct gain_table_entry {
  unsigned char gain0;
  unsigned char gain1;
};
struct gain_table_entry gain_table[MAX_RF_GAIN-MIN_RF_GAIN+1] = {
27, 20,
29, 23,
32, 26,
34, 28,
37, 31,
41, 34,
43, 37,
45, 39,
48, 42,
51, 45,
54, 47,
56, 50,
58, 52,
61, 55,
63, 57,
65, 60,
67, 63,
70, 65,
72, 67,
75, 70,
78, 72,
80, 75,
83, 77,
86, 80,
88, 82,
90, 85,
93, 87,
98, 90,
102, 93,
103, 95,
105, 97,
106, 100,
108, 102,
114, 105,
116, 107,
117, 110,
118, 113,
120, 115,
123, 118,
131, 121,
132, 123,
132, 126,
133, 128,
134, 131,
141, 134,
142, 136,
149, 139,
152, 142,
155, 145,
156, 148,
158, 151,
160, 154,
167, 157,
179, 160,
179, 164};
#endif /* _GAIN_TABLE_H__ */
