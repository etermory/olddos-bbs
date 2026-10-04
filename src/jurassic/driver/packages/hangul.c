/*
 * hangul.c : 한글 조사 efun (HanLP 머드 라이브러리 호환)
 *
 *   han(단어, 조사)     단어 + 받침에 맞는 조사   han("칼", "을") -> "칼을", han("검", "를") -> "검을"
 *   han_last(단어, 조사) 받침에 맞는 조사만
 *   han_iga(단어)       단어 + 이/가
 *   han_obj(단어)       단어 + 을/를
 *   han_desc(단어)      단어 + 은/는
 *   han_and(단어)       단어 + 과/와
 *   han_tool(단어)      단어 + 으로/로 (ㄹ 받침은 로)
 *   han_i(단어)         받침이 있으면 단어 + "이" ("~이라는" 의 앞부분)
 *   han_count(수)       세는 말 ("한 ", "두 ", "스무 ", ...)
 *   first_char(이름)    이름 첫 글자의 초성 ("ㄱ" ...) / 영문은 소문자 한 글자
 *
 * 문자열은 EUC-KR(완성형). 받침은 마지막 글자로 판단하며 ESC[..m, %^..%^ 색 코드,
 * 공백과 문장 부호는 건너뛴다. 숫자는 읽는 소리, 영문은 l,m,n,r 로 끝나면 받침이 있는 것으로 본다.
 */

#ifdef LATTICE
#include "/lpc_incl.h"
#else
#include "../lpc_incl.h"
#include "../efun_protos.h"
#endif

#include "hangul_tab.h"

#define JONG_RIEUL	8

/* 완성형 한글 한 글자의 표 값 (없으면 -1) */
static int han_entry(unsigned char lead, unsigned char trail)
{
	if ( lead < 0xB0 || lead > 0xC8 || trail < 0xA1 || trail > 0xFE ) return -1;
	return hangul_tab[(lead - 0xB0) * 94 + (trail - 0xA1)];
}

/* 마지막 글자의 받침 번호 (0: 받침 없음) */
static int last_jong(const char *str)
{
	const unsigned char *s = (const unsigned char *)str;
	int jong = 0;
	int i = 0;

	while ( s[i] ) {
		unsigned char c = s[i];

		/* ESC[ ... 영문자 */
		if ( c == 0x1b ) {
			i++;
			if ( s[i] == '[' ) {
				i++;
				while ( s[i] && !((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z')) ) i++;
				if ( s[i] ) i++;
			}
			continue;
		}
		/* %^RED%^ 같은 색 코드 */
		if ( c == '%' && s[i+1] == '^' ) {
			int j = i + 2;
			while ( s[j] && !(s[j] == '%' && s[j+1] == '^') ) j++;
			if ( s[j] ) { i = j + 2; continue; }
		}

		if ( c >= 0x80 ) {
			if ( s[i+1] ) {
				int e = han_entry(c, s[i+1]);
				jong = (e >= 0) ? (e & 31) : 0;
				i += 2;
			} else {
				i++;
			}
			continue;
		}

		if ( c >= '0' && c <= '9' ) {
			/* 영 일 이 삼 사 오 육 칠 팔 구 */
			static const int digit_jong[10] = { 21, 8, 0, 16, 0, 0, 1, 8, 8, 0 };
			jong = digit_jong[c - '0'];
		} else if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ) {
			switch ( c | 0x20 ) {
				case 'l': case 'r': jong = JONG_RIEUL; break;
				case 'm': jong = 16; break;
				case 'n': jong = 4; break;
				default: jong = 0;
			}
		}
		/* 공백, 문장 부호는 바로 앞 글자를 그대로 둔다 */
		i++;
	}
	return jong;
}

/* 받침에 맞는 조사 고르기. 짝이 없는 조사는 그대로 */
static const char *josa_pairs[][2] = {
	{ "을", "를" }, { "은", "는" }, { "이", "가" }, { "과", "와" },
	{ "으로", "로" }, { "이라", "라" }, { "이다", "다" }, { "이나", "나" },
	{ "이랑", "랑" }, { "아", "야" }, { "이여", "여" }, { "이며", "며" },
	{ "이고", "고" }, { "이든", "든" }, { "이면", "면" },
	{ 0, 0 }
};

static const char *choose_josa(const char *word, const char *josa)
{
	int jong = last_jong(word);
	int k;

	for ( k = 0; josa_pairs[k][0]; k++ ) {
		if ( !strcmp(josa, josa_pairs[k][0]) || !strcmp(josa, josa_pairs[k][1]) ) {
			/* 으로/로 는 ㄹ 받침이면 로 */
			if ( k == 4 ) return (jong && jong != JONG_RIEUL) ? josa_pairs[k][0] : josa_pairs[k][1];
			return jong ? josa_pairs[k][0] : josa_pairs[k][1];
		}
	}
	return josa;
}

/* a + b 를 새 문자열로 */
static char *join2(const char *a, const char *b)
{
	int la = strlen(a), lb = strlen(b);
	char *r = new_string(la + lb, "hangul");
	memcpy(r, a, la);
	memcpy(r + la, b, lb);
	r[la + lb] = 0;
	return r;
}

/* 스택의 한 문자열 인자에 조사를 붙여 바꾼다 */
static void attach_josa(const char *josa)
{
	char *r = join2(sp->u.string, choose_josa(sp->u.string, josa));
	free_string_svalue(sp);
	put_malloced_string(r);
}

#ifdef F_HAN
void f_han PROT((void))
{
	char *r = join2((sp - 1)->u.string, choose_josa((sp - 1)->u.string, sp->u.string));
	free_string_svalue(sp--);
	free_string_svalue(sp);
	put_malloced_string(r);
}
#endif

#ifdef F_HAN_LAST
void f_han_last PROT((void))
{
	char *r = join2("", choose_josa((sp - 1)->u.string, sp->u.string));
	free_string_svalue(sp--);
	free_string_svalue(sp);
	put_malloced_string(r);
}
#endif

#ifdef F_HAN_IGA
void f_han_iga PROT((void)) { attach_josa("이"); }
#endif

#ifdef F_HAN_OBJ
void f_han_obj PROT((void)) { attach_josa("을"); }
#endif

#ifdef F_HAN_DESC
void f_han_desc PROT((void)) { attach_josa("은"); }
#endif

#ifdef F_HAN_AND
void f_han_and PROT((void)) { attach_josa("과"); }
#endif

#ifdef F_HAN_TOOL
void f_han_tool PROT((void)) { attach_josa("으로"); }
#endif

#ifdef F_HAN_I
void f_han_i PROT((void))
{
	char *r = join2(sp->u.string, last_jong(sp->u.string) ? "이" : "");
	free_string_svalue(sp);
	put_malloced_string(r);
}
#endif

#ifdef F_HAN_COUNT
void f_han_count PROT((void))
{
	static const char *ones[] = { "", "한", "두", "세", "네", "다섯", "여섯", "일곱", "여덟", "아홉" };
	static const char *tens[] = { "", "열", "스물", "서른", "마흔", "쉰", "예순", "일흔", "여든", "아흔" };
	int n = sp->u.number;
	char buf[64];

	if ( n <= 0 || n >= 100 ) {
		sprintf(buf, "%d ", n);
	} else if ( n == 20 ) {
		strcpy(buf, "스무 ");
	} else {
		sprintf(buf, "%s%s ", tens[n / 10], ones[n % 10]);
	}
	put_malloced_string(string_copy(buf, "han_count"));
}
#endif

#ifdef F_FIRST_CHAR
void f_first_char PROT((void))
{
	static const char *cho[] = {
		"ㄱ", "ㄲ", "ㄴ", "ㄷ", "ㄸ", "ㄹ", "ㅁ", "ㅂ", "ㅃ", "ㅅ",
		"ㅆ", "ㅇ", "ㅈ", "ㅉ", "ㅊ", "ㅋ", "ㅌ", "ㅍ", "ㅎ"
	};
	const unsigned char *s = (const unsigned char *)sp->u.string;
	char buf[8];

	if ( s[0] >= 0x80 && s[1] ) {
		int e = han_entry(s[0], s[1]);
		if ( e >= 0 ) strcpy(buf, cho[e >> 5]);
		else strcpy(buf, "etc");
	} else if ( s[0] ) {
		buf[0] = (s[0] >= 'A' && s[0] <= 'Z') ? s[0] + 32 : s[0];
		buf[1] = 0;
	} else {
		strcpy(buf, "etc");
	}
	free_string_svalue(sp);
	put_malloced_string(string_copy(buf, "first_char"));
}
#endif
