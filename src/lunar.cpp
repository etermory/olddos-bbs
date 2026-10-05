#include "main.h"

// 만세력 / 음력 변환
// 음력 자료와 변환 방식은 korean_lunar_calendar_py (MIT, Jinil Lee) 를 옮긴 것으로
// 한국천문연구원(KASI) 기준이다.

struct termio sys_term;

char title[1024] = "만세력 / 음력";

char tty[10];

char host_name[256];

#define LUNAR_BASE_YEAR		1000
#define LUNAR_YEAR_COUNT	1051
#define SOLAR_LUNAR_DAY_DIFF	43

// 메뉴에서 받는 연도 범위
#define MIN_YEAR	1900
#define MAX_YEAR	2049

static const unsigned int lunar_data[LUNAR_YEAR_COUNT] = {
	0x82c60a57, 0x82fec52b, 0x82c40d2a, 0x82c60d55, 0xc30095ad, 0x82c4056a, 0x82c6096d, 0x830054dd,
	0xc2c404ad, 0x82c40a4d, 0x83002e4d, 0x82c40b26, 0xc300ab56, 0x82c60ad5, 0x82c4035a, 0x8300697a,
	0xc2c6095b, 0x82c4049b, 0x83004a9b, 0x82c40a4b, 0xc301caa5, 0x82c406aa, 0x82c60ad5, 0x830092dd,
	0xc2c402b5, 0x82c60957, 0x82fe54ae, 0x82c60c97, 0xc2c4064b, 0x82ff254a, 0x82c60da9, 0x8300a6b6,
	0xc2c6066d, 0x82c4026e, 0x8301692e, 0x82c4092e, 0xc2c40c96, 0x83004d95, 0x82c40d4a, 0x8300cd69,
	0xc2c40b58, 0x82c80d6b, 0x8301926b, 0x82c4025d, 0xc2c4092b, 0x83005aab, 0x82c40a95, 0x82c40b4a,
	0xc3021eab, 0x82c402d5, 0x8301b55a, 0x82c604bb, 0xc2c4025b, 0x83007537, 0x82c4052b, 0x82c40695,
	0xc3003755, 0x82c406aa, 0x8303cab5, 0x82c40275, 0xc2c404b6, 0x83008a5e, 0x82c40a56, 0x82c40d26,
	0xc3005ea6, 0x82c60d55, 0x82c405aa, 0x83001d6a, 0xc2c6096d, 0x8300b4af, 0x82c4049d, 0x82c40a4d,
	0xc3007d2d, 0x82c40aa6, 0x82c60b55, 0x830045d5, 0xc2c4035a, 0x82c6095d, 0x83011173, 0x82c4045b,
	0xc3009a4f, 0x82c4064b, 0x82c40aa5, 0x83006b69, 0xc2c606b5, 0x82c402da, 0x83002ab6, 0x82c60937,
	0xc2fec497, 0x82c60c97, 0x82c4064b, 0x82fe86aa, 0xc2c60da5, 0x82c405b4, 0x83034a6d, 0x82c402ae,
	0xc2c40e61, 0x83002d2e, 0x82c40c96, 0x83009d4d, 0x82c40d4a, 0x82c60d65, 0x83016595, 0x82c6055d,
	0xc2c4026d, 0x83002a5d, 0x82c4092b, 0x8300aa97, 0xc2c40a95, 0x82c40b4a, 0x83008b5a, 0x82c60ad5,
	0xc2c6055b, 0x830042b7, 0x82c40457, 0x82c4052b, 0xc3001d2b, 0x82c40695, 0x8300972d, 0x82c405aa,
	0xc2c60ab5, 0x830054ed, 0x82c404b6, 0x82c60a57, 0xc2ff344e, 0x82c40d26, 0x8301be92, 0x82c60d55,
	0xc2c405aa, 0x830089ba, 0x82c6096d, 0x82c404ae, 0xc3004a9d, 0x82c40a4d, 0x82c40d25, 0x83002f25,
	0xc2c40b54, 0x8303ad69, 0x82c402da, 0x82c6095d, 0xc301649b, 0x82c4049b, 0x82c40a4b, 0x83004b4b,
	0xc2c406a5, 0x8300bb53, 0x82c406b4, 0x82c60ab6, 0xc3018956, 0x82c60997, 0x82c40497, 0x83004697,
	0xc2c4054b, 0x82fec6a5, 0x82c60da5, 0x82c405ac, 0xc303aab5, 0x82c4026e, 0x82c4092e, 0x83006cae,
	0xc2c40c96, 0x82c40d4a, 0x83002f4a, 0x82c60d55, 0xc300b56b, 0x82c6055b, 0x82c4025d, 0x8300793d,
	0xc2c40927, 0x82c40a95, 0x83015d15, 0x82c40b4a, 0xc2c60b55, 0x830112d5, 0x82c604db, 0x82fe925e,
	0xc2c60a57, 0x82c4052b, 0x83006aab, 0x82c40695, 0xc2c406aa, 0x83003baa, 0x82c60ab5, 0x8300b4b7,
	0xc2c404ae, 0x82c60a57, 0x82fe752e, 0x82c40d26, 0xc2c60e93, 0x830056d5, 0x82c405aa, 0x82c609b5,
	0xc300256d, 0x82c404ae, 0x8301aa4d, 0x82c40a4d, 0xc2c40d26, 0x83006d65, 0x82c40b52, 0x82c60d6a,
	0xc30026da, 0x82c6095d, 0x8301c49d, 0x82c4049b, 0xc2c40a4b, 0x83008aab, 0x82c406a5, 0x82c40b54,
	0xc3004bb4, 0x82c60ab6, 0x82c6095b, 0x83002537, 0xc2c40497, 0x8300964f, 0x82c4054b, 0x82c406a5,
	0xc30176c5, 0x82c405ac, 0x82c60ab6, 0x8301386e, 0xc2c4092e, 0x8300cc97, 0x82c40c96, 0x82c40d4a,
	0xc3008daa, 0x82c60b55, 0x82c4056a, 0x83025adb, 0xc2c4025d, 0x82c4092e, 0x83002d2b, 0x82c40a95,
	0xc3009d4d, 0x82c40b2a, 0x82c60b55, 0x83007575, 0xc2c404da, 0x82c60a5b, 0x83004557, 0x82c4052b,
	0xc301ca93, 0x82c40693, 0x82c406aa, 0x83008ada, 0xc2c60ae5, 0x82c404b6, 0x83004aae, 0x82c60a57,
	0xc2c40527, 0x82ff2526, 0x82c60e53, 0x8300a6cb, 0xc2c405aa, 0x82c605ad, 0x830164ad, 0x82c404ae,
	0xc2c40a4e, 0x83004d4d, 0x82c40d26, 0x8300bd53, 0xc2c40b52, 0x82c60b6a, 0x8301956a, 0x82c60557,
	0xc2c4049d, 0x83015a1b, 0x82c40a4b, 0x82c40aa5, 0xc3001ea5, 0x82c40b52, 0x8300bb5a, 0x82c60ab6,
	0xc2c6095b, 0x830064b7, 0x82c40497, 0x82c4064b, 0xc300374b, 0x82c406a5, 0x8300b6b3, 0x82c405ac,
	0xc2c60ab6, 0x830182ad, 0x82c4049e, 0x82c40a4d, 0xc3005d4b, 0x82c40b25, 0x82c40b52, 0x83012e52,
	0xc2c60b5a, 0x8300a95e, 0x82c6095b, 0x82c4049b, 0xc3006a57, 0x82c40a4b, 0x82c40aa5, 0x83004ba5,
	0xc2c406d4, 0x8300cad6, 0x82c60ab6, 0x82c60937, 0x8300849f, 0x82c40497, 0x82c4064b, 0x82fe56ca,
	0xc2c60da5, 0x82c405aa, 0x83001d6c, 0x82c60a6e, 0xc300b92f, 0x82c4092e, 0x82c40c96, 0x83007d55,
	0xc2c40d4a, 0x82c60d55, 0x83013555, 0x82c4056a, 0xc2c60a6d, 0x83001a5d, 0x82c4092b, 0x83008a5b,
	0xc2c40a95, 0x82c40b2a, 0x83015b2a, 0x82c60ad5, 0xc2c404da, 0x83001cba, 0x82c60a57, 0x8300952f,
	0xc2c40527, 0x82c40693, 0x830076b3, 0x82c406aa, 0xc2c60ab5, 0x83003575, 0x82c404b6, 0x8300ca67,
	0xc2c40a2e, 0x82c40d16, 0x83008e96, 0x82c40d4a, 0xc2c60daa, 0x830055ea, 0x82c6056d, 0x82c404ae,
	0xc301285d, 0x82c40a2d, 0x8300ad17, 0x82c40aa5, 0xc2c40b52, 0x83007d74, 0x82c60ada, 0x82c6055d,
	0xc300353b, 0x82c4045b, 0x82c40a2b, 0x83011a2b, 0xc2c40aa5, 0x83009b55, 0x82c406b2, 0x82c60ad6,
	0xc3015536, 0x82c60937, 0x82c40457, 0x83003a57, 0xc2c4052b, 0x82feaaa6, 0x82c60d95, 0x82c405aa,
	0xc3017aac, 0x82c60a6e, 0x82c4052e, 0x83003cae, 0xc2c40a56, 0x8300bd2b, 0x82c40d2a, 0x82c60d55,
	0xc30095ad, 0x82c4056a, 0x82c60a6d, 0x8300555d, 0xc2c4052b, 0x82c40a8d, 0x83002e55, 0x82c40b2a,
	0xc300ab56, 0x82c60ad5, 0x82c404da, 0x83006a7a, 0xc2c60a57, 0x82c4051b, 0x83014a17, 0x82c40653,
	0xc301c6a9, 0x82c405aa, 0x82c60ab5, 0x830092bd, 0xc2c402b6, 0x82c60a37, 0x82fe552e, 0x82c40d16,
	0x82c60e4b, 0x82fe3752, 0x82c60daa, 0x8301b5b4, 0xc2c6056d, 0x82c402ae, 0x83007a3d, 0x82c40a2d,
	0xc2c40d15, 0x83004d95, 0x82c40b52, 0x8300cb69, 0xc2c60ada, 0x82c6055d, 0x8301925b, 0x82c4045b,
	0xc2c40a2b, 0x83005aab, 0x82c40a95, 0x82c40b52, 0xc3001eaa, 0x82c60ab6, 0x8300c55b, 0x82c604b7,
	0xc2c40457, 0x83007537, 0x82c4052b, 0x82c40695, 0xc3014695, 0x82c405aa, 0x8300cab5, 0x82c60a6e,
	0xc2c404ae, 0x83008a5e, 0x82c40a56, 0x82c40d2a, 0xc3006eaa, 0x82c60d55, 0x82c4056a, 0x8301295a,
	0xc2c6095d, 0x8300b4af, 0x82c4049b, 0x82c40a4d, 0xc3007d2d, 0x82c40b2a, 0x82c60b55, 0x830045d5,
	0xc2c402da, 0x82c6095b, 0x83011157, 0x82c4049b, 0xc3009a4f, 0x82c4064b, 0x82c406a9, 0x83006aea,
	0xc2c606b5, 0x82c402b6, 0x83002aae, 0x82c60937, 0xc2ffb496, 0x82c40c96, 0x82c60e4b, 0x82fe76b2,
	0xc2c60daa, 0x82c605ad, 0x8300336d, 0x82c4026e, 0xc2c4092e, 0x83002d2d, 0x82c40c95, 0x83009d4d,
	0xc2c40b4a, 0x82c60b69, 0x8301655a, 0x82c6055b, 0xc2c4025d, 0x83002a5b, 0x82c4092b, 0x8300aa97,
	0xc2c40695, 0x82c4074a, 0x83008b5a, 0x82c60ab6, 0xc2c6053b, 0x830042b7, 0x82c40257, 0x82c4052b,
	0xc3001d2b, 0x82c40695, 0x830096ad, 0x82c405aa, 0xc2c60ab5, 0x830054ed, 0x82c404ae, 0x82c60a57,
	0xc2ff344e, 0x82c40d2a, 0x8301bd94, 0x82c60b55, 0x82c4056a, 0x8300797a, 0x82c6095d, 0x82c404ae,
	0xc3004a9b, 0x82c40a4d, 0x82c40d25, 0x83011aaa, 0xc2c60b55, 0x8300956d, 0x82c402da, 0x82c6095b,
	0xc30054b7, 0x82c40497, 0x82c40a4b, 0x83004b4b, 0xc2c406a9, 0x8300cad5, 0x82c605b5, 0x82c402b6,
	0xc300895e, 0x82c6092f, 0x82c40497, 0x82fe4696, 0xc2c40d4a, 0x8300cea5, 0x82c60d69, 0x82c6056d,
	0xc301a2b5, 0x82c4026e, 0x82c4092e, 0x83006cad, 0xc2c40c95, 0x82c40d4a, 0x83002f4a, 0x82c60b59,
	0xc300c56d, 0x82c6055b, 0x82c4025d, 0x8300793b, 0xc2c4092b, 0x82c40a95, 0x83015b15, 0x82c406ca,
	0xc2c60ad5, 0x830112b6, 0x82c604bb, 0x8300925f, 0xc2c40257, 0x82c4052b, 0x82fe6aaa, 0x82c60e95,
	0xc2c406aa, 0x83003baa, 0x82c60ab5, 0x8300b4b7, 0xc2c404ae, 0x82c60a57, 0x82fe752d, 0x82c40d26,
	0xc2c60d95, 0x830055d5, 0x82c4056a, 0x82c6096d, 0xc300255d, 0x82c404ae, 0x8300aa4f, 0x82c40a4d,
	0xc2c40d25, 0x83006d69, 0x82c60b55, 0x82c4035a, 0xc3002aba, 0x82c6095b, 0x8301c49b, 0x82c40497,
	0xc2c40a4b, 0x83008b2b, 0x82c406a5, 0x82c406d4, 0xc3034ab5, 0x82c402b6, 0x82c60937, 0x8300252f,
	0xc2c40497, 0x82fe964e, 0x82c40d4a, 0x82c60ea5, 0xc30166a9, 0x82c6056d, 0x82c402b6, 0x8301385e,
	0xc2c4092e, 0x8300bc97, 0x82c40a95, 0x82c40d4a, 0xc3008daa, 0x82c60b4d, 0x82c6056b, 0x830042db,
	0xc2c4025d, 0x82c4092d, 0x83002d2b, 0x82c40a95, 0xc3009b4d, 0x82c406aa, 0x82c60ad5, 0x83006575,
	0xc2c604bb, 0x82c4025b, 0x83013457, 0x82c4052b, 0xc2ffba94, 0x82c60e95, 0x82c406aa, 0x83008ada,
	0xc2c609b5, 0x82c404b6, 0x83004aae, 0x82c60a4f, 0xc2c20526, 0x83012d26, 0x82c60d55, 0x8301a5a9,
	0xc2c4056a, 0x82c6096d, 0x8301649d, 0x82c4049e, 0xc2c40a4d, 0x83004d4d, 0x82c40d25, 0x8300bd53,
	0xc2c40b54, 0x82c60b5a, 0x8301895a, 0x82c6095b, 0xc2c4049b, 0x83004a97, 0x82c40a4b, 0x82c40aa5,
	0xc3001ea5, 0x82c406d4, 0x8302badb, 0x82c402b6, 0xc2c60937, 0x830064af, 0x82c40497, 0x82c4064b,
	0xc2fe374a, 0x82c60da5, 0x8300b6b5, 0x82c6056d, 0xc2c402ae, 0x8300793e, 0x82c4092e, 0x82c40c96,
	0xc3015d15, 0x82c40d4a, 0x82c60da5, 0x83013555, 0xc2c4056a, 0x83007a7a, 0x82c60a5d, 0x82c4092d,
	0xc3006aab, 0x82c40a95, 0x82c40b4a, 0x83004baa, 0xc2c60ad5, 0x82c4055a, 0x830128ba, 0x82c60a5b,
	0xc3007537, 0x82c4052b, 0x82c40693, 0x83015715, 0xc2c406aa, 0x82c60ad5, 0x830035b5, 0x82c404b6,
	0xc3008a5e, 0x82c40a4e, 0x82c40d26, 0x83006ea6, 0xc2c40d52, 0x82c60daa, 0x8301466a, 0x82c6056d,
	0xc2c404ae, 0x83003a9d, 0x82c40a4d, 0x83007d2b, 0xc2c40b25, 0x82c40d52, 0x83015d54, 0x82c60b5a,
	0xc2c6055d, 0x8300355b, 0x82c4049b, 0x83007657, 0x82c40a4b, 0x82c40aa5, 0x83006b65, 0x82c406d2,
	0xc2c60ada, 0x830045b6, 0x82c60937, 0x82c40497, 0xc3003697, 0x82c4064d, 0x82fe76aa, 0x82c60da5,
	0xc2c405aa, 0x83005aec, 0x82c60aae, 0x82c4092e, 0xc3003d2e, 0x82c40c96, 0x83018d45, 0x82c40d4a,
	0xc2c60d55, 0x83016595, 0x82c4056a, 0x82c60a6d, 0xc300455d, 0x82c4052d, 0x82c40a95, 0x83013c95,
	0xc2c40b4a, 0x83017b4a, 0x82c60ad5, 0x82c4055a, 0xc3015a3a, 0x82c60a5b, 0x82c4052b, 0x83014a17,
	0xc2c40693, 0x830096ab, 0x82c406aa, 0x82c60ab5, 0xc30064f5, 0x82c404b6, 0x82c60a57, 0x82fe452e,
	0xc2c40d16, 0x82c60e93, 0x82fe3752, 0x82c60daa, 0xc30175aa, 0x82c6056d, 0x82c404ae, 0x83015a1d,
	0xc2c40a2d, 0x82c40d15, 0x83004da5, 0x82c40b52, 0xc3009d6a, 0x82c60ada, 0x82c6055d, 0x8301629b,
	0xc2c4045b, 0x82c40a2b, 0x83005b2b, 0x82c40a95, 0xc2c40b52, 0x83012ab2, 0x82c60ad6, 0x83017556,
	0xc2c60537, 0x82c40457, 0x83005657, 0x82c4052b, 0xc2c40695, 0x83003795, 0x82c405aa, 0x8300aab6,
	0xc2c60a6d, 0x82c404ae, 0x83006a6e, 0x82c40a56, 0xc2c40d2a, 0x83005eaa, 0x82c60d55, 0x82c405aa,
	0xc3003b6a, 0x82c60a6d, 0x830074bd, 0x82c404ab, 0xc2c40a8d, 0x83005d55, 0x82c40b2a, 0x82c60b55,
	0xc30045d5, 0x82c404da, 0x82c6095d, 0x83002557, 0xc2c4049b, 0x83006a97, 0x82c4064b, 0x82c406a9,
	0x83004baa, 0x82c606b5, 0x82c402ba, 0x83002ab6, 0xc2c60937, 0x82fe652e, 0x82c40d16, 0x82c60e4b,
	0xc2fe56d2, 0x82c60da9, 0x82c605b5, 0x8300336d, 0xc2c402ae, 0x82c40a2e, 0x83002e2d, 0x82c40c95,
	0xc3006d55, 0x82c40b52, 0x82c60b69, 0x830045da, 0xc2c6055d, 0x82c4025d, 0x83003a5b, 0x82c40a2b,
	0xc3017a8b, 0x82c40a95, 0x82c40b4a, 0x83015b2a, 0xc2c60ad5, 0x82c6055b, 0x830042b7, 0x82c40257,
	0xc300952f, 0x82c4052b, 0x82c40695, 0x830066d5, 0xc2c405aa, 0x82c60ab5, 0x8300456d, 0x82c404ae,
	0xc2c60a57, 0x82ff3456, 0x82c40d2a, 0x83017e8a, 0xc2c60d55, 0x82c405aa, 0x83005ada, 0x82c6095d,
	0xc2c404ae, 0x83004aab, 0x82c40a4d, 0x83008d2b, 0xc2c40b29, 0x82c60b55, 0x83007575, 0x82c402da,
	0xc2c6095d, 0x830054d7, 0x82c4049b, 0x82c40a4b, 0xc3013a4b, 0x82c406a9, 0x83008ad9, 0x82c606b5,
	0xc2c402b6, 0x83015936, 0x82c60937, 0x82c40497, 0xc2fe4696, 0x82c40e4a, 0x8300aea6, 0x82c60da9,
	0xc2c605ad, 0x830162ad, 0x82c402ae, 0x82c4092e, 0xc3005cad, 0x82c40c95, 0x82c40d4a, 0x83013d4a,
	0xc2c60b69, 0x8300757a, 0x82c6055b, 0x82c4025d, 0xc300595b, 0x82c4092b, 0x82c40a95, 0x83004d95,
	0xc2c40b4a, 0x82c60b55, 0x830026d5, 0x82c6055b, 0xc3006277, 0x82c40257, 0x82c4052b, 0x82fe5aaa,
	0xc2c60e95, 0x82c406aa, 0x83003baa, 0x82c60ab5, 0x830084bd, 0x82c404ae, 0x82c60a57, 0x82fe554d,
	0xc2c40d26, 0x82c60d95, 0x83014655, 0x82c4056a, 0xc2c609ad, 0x8300255d, 0x82c404ae, 0x83006a5b,
	0xc2c40a4d, 0x82c40d25, 0x83005da9, 0x82c60b55, 0xc2c4056a, 0x83002ada, 0x82c6095d, 0x830074bb,
	0xc2c4049b, 0x82c40a4b, 0x83005b4b, 0x82c406a9, 0xc2c40ad4, 0x83024bb5, 0x82c402b6, 0x82c6095b,
	0xc3002537, 0x82c40497, 0x82fe6656, 0x82c40e4a, 0xc2c60ea5, 0x830156a9, 0x82c605b5, 0x82c402b6,
	0xc30138ae, 0x82c4092e, 0x83017c8d, 0x82c40c95, 0xc2c40d4a, 0x83016d8a, 0x82c60b69, 0x82c6056d,
	0xc301425b, 0x82c4025d, 0x82c4092d, 0x83002d2b, 0xc2c40a95, 0x83007d55, 0x82c40b4a, 0x82c60b55,
	0xc3015555, 0x82c604db, 0x82c4025b, 0x83013857, 0xc2c4052b, 0x83008a9b, 0x82c40695, 0x82c406aa,
	0xc3006aea, 0x82c60ab5, 0x82c404b6, 0x83004aae, 0xc2c60a57, 0x82c40527, 0x82fe3726, 0x82c60d95,
	0xc30076b5, 0x82c4056a, 0x82c609ad, 0x830054dd, 0xc2c404ae, 0x82c40a4e, 0x83004d4d, 0x82c40d25,
	0xc3008d59, 0x82c40b54, 0x82c60d6a, 0x8301695a, 0xc2c6095b, 0x82c4049b, 0x83004a9b, 0x82c40a4b,
	0xc300ab27, 0x82c406a5, 0x82c406d4, 0x83026b75, 0xc2c402b6, 0x82c6095b, 0x830054b7, 0x82c40497,
	0xc2c4064b, 0x82fe374a, 0x82c60ea5, 0x830086d9, 0xc2c605ad, 0x82c402b6, 0x8300596e, 0x82c4092e,
	0xc2c40c96, 0x83004e95, 0x82c40d4a, 0x82c60da5, 0xc3002755, 0x82c4056c, 0x83027abb, 0x82c4025d,
	0xc2c4092d, 0x83005cab, 0x82c40a95, 0x82c40b4a, 0xc3013b4a, 0x82c60b55, 0x8300955d, 0x82c404ba,
	0xc2c60a5b, 0x83005557, 0x82c4052b, 0x82c40a95, 0xc3004b95, 0x82c406aa, 0x82c60ad5, 0x830026b5,
	0xc2c404b6, 0x83006a6e, 0x82c60a57, 0x82c40527, 0xc2fe56a6, 0x82c60d93, 0x82c405aa, 0x83003b6a,
	0xc2c6096d, 0x8300b4af, 0x82c404ae, 0x82c40a4d, 0xc3016d0d, 0x82c40d25, 0x82c40d52, 0x83005dd4,
	0xc2c60b6a, 0x82c6096d, 0x8300255b, 0x82c4049b, 0xc3007a57, 0x82c40a4b, 0x82c40b25, 0x83015b25,
	0xc2c406d4, 0x82c60ada, 0x830138b6
};

static long cum_lunar_days[LUNAR_YEAR_COUNT];
static long cum_solar_days[LUNAR_YEAR_COUNT];

static const char *cheongan[] = { "갑", "을", "병", "정", "무", "기", "경", "신", "임", "계" };
static const char *jiji[] = { "자", "축", "인", "묘", "진", "사", "오", "미", "신", "유", "술", "해" };
static const char *ddi[] = { "쥐", "소", "호랑이", "토끼", "용", "뱀", "말", "양", "원숭이", "닭", "개", "돼지" };
static const char *weekdays[] = { "일", "월", "화", "수", "목", "금", "토" };

// ------------------------------------------------------------------
// 음력 계산
// ------------------------------------------------------------------
static unsigned int ldata(int year)
{
	return lunar_data[year - LUNAR_BASE_YEAR];
}

// 윤달 (없으면 0)
int leap_month(int year)
{
	return (ldata(year) >> 12) & 0x0F;
}

int lunar_month_days(int year, int month, bool leap)
{
	unsigned int d = ldata(year);
	if ( leap && leap_month(year) == month ) {
		return ((d >> 16) & 0x01) ? 30 : 29;
	}
	return ((d >> (12 - month)) & 0x01) ? 30 : 29;
}

static int lunar_year_days(int year)
{
	return (ldata(year) >> 17) & 0x01FF;
}

static bool solar_leap_year(int year)
{
	return ((ldata(year) >> 30) & 0x01) != 0;
}

int solar_month_days(int year, int month)
{
	static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if ( month == 2 && solar_leap_year(year) ) return 29;
	return days[month - 1];
}

static void init_lunar(void)
{
	long a = 0, b = 0;
	for (int i=0; i<LUNAR_YEAR_COUNT; i++) {
		a += lunar_year_days(LUNAR_BASE_YEAR + i);
		b += solar_leap_year(LUNAR_BASE_YEAR + i) ? 366 : 365;
		cum_lunar_days[i] = a;
		cum_solar_days[i] = b;
	}
}

// 기준 연도부터 year 까지의 날 수
static long lunar_days_through_year(int year)
{
	if ( year < LUNAR_BASE_YEAR ) return 0;
	return cum_lunar_days[year - LUNAR_BASE_YEAR];
}

static long solar_days_through_year(int year)
{
	if ( year < LUNAR_BASE_YEAR ) return 0;
	return cum_solar_days[year - LUNAR_BASE_YEAR];
}

static long lunar_days_through_month(int year, int month, bool leap)
{
	long days = 0;
	if ( year >= LUNAR_BASE_YEAR && month > 0 ) {
		for (int m=1; m<=month; m++) {
			days += lunar_month_days(year, m, false);
		}
		if ( leap ) {
			int lm = leap_month(year);
			if ( lm > 0 && lm < month + 1 ) {
				days += lunar_month_days(year, lm, true);
			}
		}
	}
	return days;
}

static long lunar_abs_days(int year, int month, int day, bool leap)
{
	long days = lunar_days_through_year(year - 1) + lunar_days_through_month(year, month - 1, true) + day;
	if ( leap && leap_month(year) == month ) {
		days += lunar_month_days(year, month, false);
	}
	return days;
}

static long solar_abs_days(int year, int month, int day)
{
	long days = solar_days_through_year(year - 1) + day;
	for (int m=1; m<month; m++) {
		days += solar_month_days(year, m);
	}
	return days - SOLAR_LUNAR_DAY_DIFF;
}

// 양력 -> 음력
void solar_to_lunar(int sy, int sm, int sd, int *ly, int *lm, int *ld, bool *leap)
{
	long abs_days = solar_abs_days(sy, sm, sd);
	int year = (abs_days >= lunar_abs_days(sy, 1, 1, false)) ? sy : sy - 1;

	*ly = year;
	*lm = 0;
	*ld = 0;
	*leap = false;

	for (int m=12; m>0; m--) {
		long by_month = lunar_abs_days(year, m, 1, false);
		if ( abs_days >= by_month ) {
			*lm = m;
			if ( leap_month(year) == m ) {
				*leap = abs_days >= lunar_abs_days(year, m, 1, true);
			}
			*ld = abs_days - lunar_abs_days(year, m, 1, *leap) + 1;
			break;
		}
	}
}

// 음력 -> 양력
void lunar_to_solar(int ly, int lm, int ld, bool leap, int *sy, int *sm, int *sd)
{
	long abs_days = lunar_abs_days(ly, lm, ld, leap);
	int year = (abs_days < solar_abs_days(ly + 1, 1, 1)) ? ly : ly + 1;

	*sy = year;
	*sm = 0;
	*sd = 0;

	for (int m=12; m>0; m--) {
		long by_month = solar_abs_days(year, m, 1);
		if ( abs_days >= by_month ) {
			*sm = m;
			*sd = abs_days - by_month + 1;
			break;
		}
	}
}

// 간지 (년/월/일)
std::string gapja_string(int ly, int lm, int ld, bool leap)
{
	long abs_days = lunar_abs_days(ly, lm, ld, leap);
	long month_count = lm + 12L * (ly - LUNAR_BASE_YEAR);

	std::string s;
	s += cheongan[(ly + 6 - LUNAR_BASE_YEAR) % 10];
	s += jiji[(ly - LUNAR_BASE_YEAR) % 12];
	s += "년 ";
	s += cheongan[(month_count + 3) % 10];
	s += jiji[(month_count + 1) % 12];
	s += "월 ";
	s += cheongan[(abs_days + 4) % 10];
	s += jiji[(abs_days + 2) % 12];
	s += "일";
	return s;
}

std::string ddi_string(int ly)
{
	return ddi[(ly - LUNAR_BASE_YEAR) % 12];
}

// 요일 (0: 일요일)
int day_of_week(int y, int m, int d)
{
	static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
	if ( m < 3 ) y -= 1;
	return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

bool valid_solar(int y, int m, int d)
{
	return y >= MIN_YEAR && y <= MAX_YEAR && m >= 1 && m <= 12 && d >= 1 && d <= solar_month_days(y, m);
}

bool valid_lunar(int y, int m, int d, bool leap)
{
	if ( y < MIN_YEAR || y > MAX_YEAR || m < 1 || m > 12 || d < 1 ) return false;
	if ( leap && leap_month(y) != m ) return false;
	return d <= lunar_month_days(y, m, leap);
}

// ------------------------------------------------------------------
// 공휴일 / 명절
// ------------------------------------------------------------------
struct holiday {
	int y, m, d;
	std::string name;
};

static bool holiday_less(const holiday &a, const holiday &b)
{
	if ( a.y != b.y ) return a.y < b.y;
	if ( a.m != b.m ) return a.m < b.m;
	return a.d < b.d;
}

static void add_solar_holiday(std::vector<holiday> &list, int y, int m, int d, const char *name)
{
	holiday h;
	h.y = y; h.m = m; h.d = d; h.name = name;
	list.push_back(h);
}

// 음력 날짜에 offset 일을 더한 양력 날짜
static void add_lunar_holiday(std::vector<holiday> &list, int ly, int lm, int ld, int offset, const char *name)
{
	if ( ly < MIN_YEAR || ly > MAX_YEAR ) return;
	int sy, sm, sd;
	lunar_to_solar(ly, lm, ld, false, &sy, &sm, &sd);

	struct tm t;
	memset(&t, 0, sizeof(t));
	t.tm_year = sy - 1900;
	t.tm_mon = sm - 1;
	t.tm_mday = sd + offset;
	t.tm_hour = 12;
	mktime(&t);

	add_solar_holiday(list, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, name);
}

std::vector<holiday> holidays_of_year(int y)
{
	std::vector<holiday> list;
	add_solar_holiday(list, y, 1, 1, "신정");
	add_lunar_holiday(list, y, 1, 1, -1, "설날 연휴");
	add_lunar_holiday(list, y, 1, 1, 0, "설날");
	add_lunar_holiday(list, y, 1, 1, 1, "설날 연휴");
	add_solar_holiday(list, y, 3, 1, "삼일절");
	add_solar_holiday(list, y, 5, 5, "어린이날");
	add_lunar_holiday(list, y, 4, 8, 0, "부처님오신날");
	add_solar_holiday(list, y, 6, 6, "현충일");
	add_solar_holiday(list, y, 8, 15, "광복절");
	add_lunar_holiday(list, y, 8, 15, -1, "추석 연휴");
	add_lunar_holiday(list, y, 8, 15, 0, "추석");
	add_lunar_holiday(list, y, 8, 15, 1, "추석 연휴");
	add_solar_holiday(list, y, 10, 3, "개천절");
	add_solar_holiday(list, y, 10, 9, "한글날");
	add_solar_holiday(list, y, 12, 25, "성탄절");
	std::sort(list.begin(), list.end(), holiday_less);
	return list;
}

bool is_holiday(int y, int m, int d)
{
	std::vector<holiday> list = holidays_of_year(y);
	for (unsigned int i=0; i<list.size(); i++) {
		if ( list[i].y == y && list[i].m == m && list[i].d == d ) return true;
	}
	return false;
}

// ------------------------------------------------------------------
// 화면
// ------------------------------------------------------------------
void raw_mode(void)
{
    struct termio tbuf;
    ioctl(0, TCGETA, &tbuf);
    tbuf.c_cc[4] = 1;
    tbuf.c_cc[5] = 0;
    tbuf.c_iflag = 0;
    tbuf.c_iflag |= IXON;
    tbuf.c_iflag |= IXANY;
    tbuf.c_oflag = 0;
    tbuf.c_oflag &= ~OPOST;
    tbuf.c_lflag &= ~(ICANON | ISIG | ECHO);
    tbuf.c_cflag &= ~PARENB;
    tbuf.c_cflag &= ~CSIZE;
    tbuf.c_cflag |= CS8;
    ioctl(0, TCSETAF, &tbuf);

	return;
}

/* 프로그램 종료 루틴 */
int host_close (void)
{
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

void print_header(const char *head_title)
{
	printf(ESC_CLEAR);
    printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	int center = (80-strlen(strip_ansi_codes(head_title)))/2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, head_title);
    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
    printf("\033[4;1H");
}

// 입력 받기 (X 는 종료)
std::string input(const char *msg)
{
	char cmd[64];
	printf(ESC_ENG);
	printf("%s", msg);
	line_input(cmd, 20);
	if ( !strcasecmp(cmd, "x") ) host_close();
	return trim(cmd);
}

void wait_enter(void)
{
	printf("\r\n [Enter] 를 누르세요.");
	press_enter();
}

void today(int *y, int *m, int *d)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	*y = tm->tm_year + 1900;
	*m = tm->tm_mon + 1;
	*d = tm->tm_mday;
}

void print_date_info(int sy, int sm, int sd)
{
	int ly, lm, ld;
	bool leap;
	solar_to_lunar(sy, sm, sd, &ly, &lm, &ld, &leap);

	printf("\r\n   양  력 : %d년 %d월 %d일 (%s요일)", sy, sm, sd, weekdays[day_of_week(sy, sm, sd)]);
	printf("\r\n   음  력 : %d년 %s%d월 %d일", ly, leap ? "윤" : "", lm, ld);
	printf("\r\n   간  지 : %s", gapja_string(ly, lm, ld, leap).c_str());
	printf("\r\n   띠     : %s띠", ddi_string(ly).c_str());
	if ( is_holiday(sy, sm, sd) ) {
		std::vector<holiday> list = holidays_of_year(sy);
		for (unsigned int i=0; i<list.size(); i++) {
			if ( list[i].m == sm && list[i].d == sd ) {
				printf("\r\n   공휴일 : %s", list[i].name.c_str());
				break;
			}
		}
	}
	printf("\r\n");
}

void show_today(void)
{
	int y, m, d;
	today(&y, &m, &d);

	print_header("오늘의 만세력");
	print_date_info(y, m, d);

	// 다가오는 명절/공휴일
	printf("\r\n   %s", repeat("─", 36).c_str());
	printf("\r\n   다가오는 공휴일\r\n");

	std::vector<holiday> list = holidays_of_year(y);
	if ( y < MAX_YEAR ) {
		std::vector<holiday> next = holidays_of_year(y + 1);
		list.insert(list.end(), next.begin(), next.end());
	}

	struct tm t0;
	memset(&t0, 0, sizeof(t0));
	t0.tm_year = y - 1900; t0.tm_mon = m - 1; t0.tm_mday = d; t0.tm_hour = 12;
	time_t now = mktime(&t0);

	int shown = 0;
	for (unsigned int i=0; i<list.size() && shown < 8; i++) {
		struct tm t;
		memset(&t, 0, sizeof(t));
		t.tm_year = list[i].y - 1900; t.tm_mon = list[i].m - 1; t.tm_mday = list[i].d; t.tm_hour = 12;
		time_t when = mktime(&t);
		long dday = (long)((when - now) / 86400);
		if ( dday < 0 ) continue;

		char dd[16];
		if ( dday == 0 ) snprintf(dd, sizeof(dd), "D-DAY");
		else snprintf(dd, sizeof(dd), "D-%ld", dday);

		printf("\r\n   %04d-%02d-%02d (%s)  %-14s %6s", list[i].y, list[i].m, list[i].d,
				weekdays[day_of_week(list[i].y, list[i].m, list[i].d)], list[i].name.c_str(), dd);
		shown++;
	}
	printf("\r\n");

	wait_enter();
}

void convert_solar(void)
{
	print_header("양력 → 음력 변환");
	printf("\r\n   %d년 ~ %d년 사이의 날짜를 입력하세요. (예: 1995-3-14)\r\n", MIN_YEAR, MAX_YEAR);

	std::string s = input("\r\n   양력 날짜 >> ");
	if ( s.empty() || !strcasecmp(s.c_str(), "p") ) return;

	int y = 0, m = 0, d = 0;
	if ( sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || !valid_solar(y, m, d) ) {
		printf("\r\n   잘못된 날짜입니다.");
		wait_enter();
		return;
	}

	print_date_info(y, m, d);
	wait_enter();
}

void convert_lunar(void)
{
	print_header("음력 → 양력 변환");
	printf("\r\n   %d년 ~ %d년 사이의 음력 날짜를 입력하세요. (예: 1995-2-14)\r\n", MIN_YEAR, MAX_YEAR);

	std::string s = input("\r\n   음력 날짜 >> ");
	if ( s.empty() || !strcasecmp(s.c_str(), "p") ) return;

	int y = 0, m = 0, d = 0;
	if ( sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || y < MIN_YEAR || y > MAX_YEAR ) {
		printf("\r\n   잘못된 날짜입니다.");
		wait_enter();
		return;
	}

	bool leap = false;
	if ( leap_month(y) == m ) {
		std::string a = input("\r\n   그 해는 윤달이 있습니다. 윤달입니까? (y/N) >> ");
		leap = !strcasecmp(a.c_str(), "y");
	}

	if ( !valid_lunar(y, m, d, leap) ) {
		printf("\r\n   잘못된 날짜입니다. (그 달은 %d일까지 있습니다)",
				(m >= 1 && m <= 12) ? lunar_month_days(y, m, leap) : 0);
		wait_enter();
		return;
	}

	int sy, sm, sd;
	lunar_to_solar(y, m, d, leap, &sy, &sm, &sd);
	print_date_info(sy, sm, sd);
	wait_enter();
}

// 한 달 달력 (양력 + 음력)
void show_calendar(void)
{
	int y, m, d;
	today(&y, &m, &d);
	int cur_y = y, cur_m = m, cur_d = d;

	while (1) {
		char buf[64];
		snprintf(buf, sizeof(buf), "%d년 %d월 달력", y, m);
		print_header(buf);

		for (int i=0; i<7; i++) {
			// 요일, 양력 날짜, 음력 날짜 모두 칸의 6 번째 글자에 오른쪽 끝을 맞춘다 (한 칸 11 자)
			printf("%6s     ", weekdays[i]);
		}
		printf("\r\n%s\r\n", repeat("─", 39).c_str());

		int first = day_of_week(y, m, 1);
		int days = solar_month_days(y, m);
		int cell = 0;

		std::string line1, line2;
		for (int i=0; i<first; i++) {
			line1 += "           ";
			line2 += "           ";
			cell++;
		}

		for (int day=1; day<=days; day++) {
			int ly, lm, ld;
			bool leap;
			solar_to_lunar(y, m, day, &ly, &lm, &ld, &leap);

			char c1[32], c2[32];
			const char *mark = " ";
			if ( y == cur_y && m == cur_m && day == cur_d ) mark = "<";
			else if ( is_holiday(y, m, day) ) mark = "*";
			snprintf(c1, sizeof(c1), "%6d%s    ", day, mark);

			// 음력 1일이나 이 달의 첫날은 월도 표시
			char ldate[16];
			if ( ld == 1 || day == 1 ) snprintf(ldate, sizeof(ldate), "%s%d.%d", leap ? "윤" : "", lm, ld);
			else snprintf(ldate, sizeof(ldate), "%d", ld);
			snprintf(c2, sizeof(c2), "%6s     ", ldate);

			line1 += c1;
			line2 += c2;
			cell++;

			if ( cell % 7 == 0 || day == days ) {
				printf("%s\r\n", line1.c_str());
				printf("\033[=8F%s\033[=15F\r\n", line2.c_str());
				line1 = "";
				line2 = "";
			}
		}

		printf("%s\r\n", repeat("─", 39).c_str());
		printf(" * 공휴일  < 오늘  (아래 줄은 음력)\r\n");

		std::string cmd = input("이전달(B) 다음달(N) 년-월(예: 2027-1) 상위메뉴(P) >> ");
		if ( !strcasecmp(cmd.c_str(), "p") ) break;

		int ny = y, nm = m;
		if ( !strcasecmp(cmd.c_str(), "b") ) {
			nm--;
			if ( nm < 1 ) { nm = 12; ny--; }
		} else if ( !strcasecmp(cmd.c_str(), "n") || cmd.empty() ) {
			nm++;
			if ( nm > 12 ) { nm = 1; ny++; }
		} else {
			int a, b;
			if ( sscanf(cmd.c_str(), "%d-%d", &a, &b) == 2 ) {
				ny = a;
				nm = b;
			}
		}

		if ( ny >= MIN_YEAR && ny <= MAX_YEAR && nm >= 1 && nm <= 12 ) {
			y = ny;
			m = nm;
		}
	}
}

int main(int argc, char **argv)
{
	// bin/lunar --today [YYYY-MM-DD] : 오늘(또는 그 날)의 음력 날짜와 공휴일/명절 이름
	//   (txt 파일의 [lunar_date], [holiday])  "음력 8월 25일<TAB>추석"
	if ( argc > 1 && !strcmp(argv[1], "--today") ) {
		init_lunar();
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		int y = tm->tm_year + 1900, m = tm->tm_mon + 1, d = tm->tm_mday;
		if ( argc > 2 ) sscanf(argv[2], "%d-%d-%d", &y, &m, &d);
		if ( !valid_solar(y, m, d) ) return 1;
		int ly, lm, ld;
		bool leap;
		solar_to_lunar(y, m, d, &ly, &lm, &ld, &leap);
		std::string name;
		std::vector<holiday> list = holidays_of_year(y);
		for ( unsigned int i = 0; i < list.size(); i++ ) {
			if ( list[i].y == y && list[i].m == m && list[i].d == d ) {
				name = list[i].name;
				break;
			}
		}
		printf("음력 %s%d월 %d일\t%s\n", leap ? "윤" : "", lm, ld, name.c_str());
		return 0;
	}

	if ( argc > 1 ) {
		snprintf(host_name, sizeof(host_name), "%s", argv[1]);
	}

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);

    ioctl(0,TCGETA, &sys_term);
	raw_mode();

    umask(0111);

	init_lunar();

	while (1) {
		print_header(title);

		int y, m, d;
		today(&y, &m, &d);
		print_date_info(y, m, d);

		printf("\r\n%s\r\n", repeat("━", 40).c_str());
		printf("\r\n    1. 오늘의 만세력 / 다가오는 공휴일");
		printf("\r\n    2. 양력 → 음력 변환");
		printf("\r\n    3. 음력 → 양력 변환");
		printf("\r\n    4. 이달의 달력 (음력 함께 보기)");
		printf("\r\n\r\n%s\r\n", repeat("━", 40).c_str());

		std::string cmd = input("이동(번호) 상위메뉴(P) 종료(X)\r\n선택 >> ");
		if ( !strcasecmp(cmd.c_str(), "p") ) break;

		if ( cmd == "1" ) show_today();
		else if ( cmd == "2" ) convert_solar();
		else if ( cmd == "3" ) convert_lunar();
		else if ( cmd == "4" ) show_calendar();
	}

	ioctl(0, TCSETAF, &sys_term);
	return 0;
}
