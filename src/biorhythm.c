// 바이오리듬
//   biorhythm YYYY MM DD          짧게 (로그인 화면, bio 명령): 오늘의 막대 4 줄과 한 줄 풀이
//   biorhythm YYYY MM DD full     자세히 (생활정보 메뉴): 오늘 + 앞뒤 2 주 그래프 + 위험일
// 신체 23 일, 감성 28 일, 지성 33 일, 지각 38 일 주기. 태어난 날을 0 일째로 센다.
// 처음 판: 바이오리듬 계산 프로그램 V1.0 by TeIn (2015)

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define PI 3.1415926535897932384626433832795

#define C_WHITE  "\033[=15F"
#define C_GRAY   "\033[=7F"
#define C_DARK   "\033[=8F"
#define C_YELLOW "\033[=14F"
#define C_RED    "\033[=12F"

struct rhythm {
	const char *name;
	int days;
	const char *color;
	const char *good, *bad;		// 좋은 날, 나쁜 날 풀이
};

static const struct rhythm R[4] = {
	{ "신체", 23, "\033[=12F", "몸이 가벼운 날. 운동이나 바깥 일 하기 좋아요.", "몸이 무거운 날. 무리하지 말고 푹 쉬세요." },
	{ "감성", 28, "\033[=10F", "기분이 좋은 날. 사람 만나고 이야기하기 좋아요.", "예민해지기 쉬운 날. 말 한마디 조심하세요." },
	{ "지성", 33, "\033[=11F", "머리가 맑은 날. 공부나 프로그래밍하기 좋아요.", "집중이 잘 안 되는 날. 큰 결정은 미뤄 두세요." },
	{ "지각", 38, "\033[=13F", "눈치와 직감이 빠른 날. 감을 믿어 보세요.", "직감이 흐린 날. 한 번 더 확인하세요." },
};

// 서기 1 년 1 월 1 일부터 센 날 수 (그레고리력)
static long day_number(int y, int m, int d)
{
	static const int before[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	long yy = y - 1;
	long n = yy * 365 + yy / 4 - yy / 100 + yy / 400 + before[m - 1] + d;
	if ( m > 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ) n++;
	return n;
}

static double value(int k, long t)
{
	return sin(2 * PI * (double)t / R[k].days);
}

// 위험일: 리듬이 0 을 지나는 날 (오늘과 내일 사이에 부호가 바뀜)
static int critical(int k, long t)
{
	double a = value(k, t), b = value(k, t + 1);
	if ( t % R[k].days == 0 ) return 1;
	return (a > 0 && b <= 0) || (a < 0 && b >= 0);
}

static const char *state(int k, long t)
{
	double v = value(k, t);
	if ( critical(k, t) ) return C_RED "위험일" C_WHITE;
	if ( v >= 0.6 ) return "아주 좋음";
	if ( v >= 0.15 ) return "좋음";
	if ( v > -0.15 ) return "보통";
	if ( v > -0.6 ) return "낮음";
	return "바닥";
}

// -100 ~ +100 을 20 칸 막대로 (가운데가 0)
static void bar(int k, double v)
{
	int n = (int)floor(fabs(v) * 10 + 0.5), i;
	if ( n > 10 ) n = 10;
	printf(C_DARK);
	for ( i = 0; i < 10; i++ ) {
		if ( v < 0 && i >= 10 - n ) printf("%s■", R[k].color);
		else printf(C_DARK "□");
	}
	printf(C_GRAY "│");
	for ( i = 0; i < 10; i++ ) {
		if ( v > 0 && i < n ) printf("%s■", R[k].color);
		else printf(C_DARK "□");
	}
	printf(C_WHITE);
}

static void today_lines(long t)
{
	int k;
	printf(" " C_GRAY "     -100" "                 0" "                  +100" C_WHITE "\r\n");
	for ( k = 0; k < 4; k++ ) {
		double v = value(k, t);
		const char *arrow = value(k, t + 1) > v ? C_YELLOW "▲" C_WHITE : C_GRAY "▼" C_WHITE;
		printf(" %s%s" C_WHITE " ", R[k].color, R[k].name);
		bar(k, v);
		printf(" %4d%% %s %s\r\n", (int)floor(v * 100 + (v < 0 ? -0.5 : 0.5)), arrow, state(k, t));
	}
}

// 오늘의 풀이 (최대 lines 줄): 위험일, 가장 좋은 리듬, 가장 나쁜 리듬 순
static void advice(long t, int lines)
{
	int k, best = 0, worst = 0, crit = -1, shown = 0;
	for ( k = 0; k < 4; k++ ) {
		if ( value(k, t) > value(best, t) ) best = k;
		if ( value(k, t) < value(worst, t) ) worst = k;
		if ( crit < 0 && critical(k, t) ) crit = k;
	}
	if ( crit >= 0 && shown < lines ) {
		printf(" " C_RED "※" C_WHITE " 오늘은 %s%s" C_WHITE " 위험일: 리듬이 바뀌는 날이라 실수하기 쉬워요.\r\n", R[crit].color, R[crit].name);
		shown++;
	}
	if ( value(best, t) >= 0.15 && best != crit && shown < lines ) {
		printf(" " C_YELLOW "☞" C_WHITE " %s%s" C_WHITE ": %s\r\n", R[best].color, R[best].name, R[best].good);
		shown++;
	}
	if ( value(worst, t) <= -0.15 && worst != crit && shown < lines ) {
		printf(" " C_GRAY "☞" C_WHITE " %s%s" C_WHITE ": %s\r\n", R[worst].color, R[worst].name, R[worst].bad);
		shown++;
	}
	if ( shown == 0 && lines > 0 ) printf(" " C_GRAY "☞" C_WHITE " 크게 좋지도 나쁘지도 않은 무난한 날입니다.\r\n");
}

// y-m-d 에서 n 일 뒤(앞)의 날짜. '일' 을 돌려주고 mon 에 '월'
static int shift_date(int y, int m, int d, int n, int *mon)
{
	struct tm tmv;
	memset(&tmv, 0, sizeof(tmv));
	tmv.tm_year = y - 1900; tmv.tm_mon = m - 1; tmv.tm_mday = d + n; tmv.tm_hour = 12;
	mktime(&tmv);
	if ( mon ) *mon = tmv.tm_mon + 1;
	return tmv.tm_mday;
}

// 앞뒤 2 주 그래프: 2 일 전 ~ 12 일 뒤 (15 일), 세로 7 칸
static void chart(long t, int y, int m, int d)
{
	const int FROM = -2, DAYS = 15, ROWS = 7;
	signed char cell[7][15];
	int r, i, k;
	memset(cell, -1, sizeof(cell));
	for ( i = 0; i < DAYS; i++ ) {
		for ( k = 3; k >= 0; k-- ) {	// 겹치면 앞의 리듬(신체)이 보인다
			int row = (int)floor((1 - value(k, t + FROM + i)) * (ROWS - 1) / 2 + 0.5);
			cell[row][i] = (signed char)k;
		}
	}
	for ( r = 0; r < ROWS; r++ ) {
		const char *label = r == 0 ? "+100" : r == ROWS / 2 ? "   0" : r == ROWS - 1 ? "-100" : "    ";
		printf(" " C_GRAY "%s ┤" C_WHITE, label);
		for ( i = 0; i < DAYS; i++ ) {
			int c = cell[r][i];
			if ( c >= 0 ) printf("%s %s ", R[c].color, i == -FROM ? "◆" : "●");
			else if ( r == ROWS / 2 ) printf(C_DARK "──");
			else if ( i == -FROM ) printf(C_DARK " ： ");
			else printf("    ");
		}
		printf(C_WHITE "\r\n");
	}
	// 날짜 줄
	printf(" " C_GRAY "       ");
	for ( i = 0; i < DAYS; i++ ) {
		int dd = shift_date(y, m, d, FROM + i, NULL);
		if ( i == -FROM ) printf(C_YELLOW " %2d " C_GRAY, dd);
		else printf(" %2d ", dd);
	}
	printf(C_WHITE "\r\n");
}

// 앞으로 2 주 안의 위험일
static void criticals(long t, int y, int m, int d)
{
	int k, i, any = 0;
	printf(" " C_GRAY "앞으로 2 주 위험일:" C_WHITE);
	for ( k = 0; k < 4; k++ ) {
		for ( i = 1; i <= 14; i++ ) {
			int mon, dd;
			if ( !critical(k, t + i) ) continue;
			dd = shift_date(y, m, d, i, &mon);
			printf(" %s%s" C_WHITE " %d/%d", R[k].color, R[k].name, mon, dd);
			any = 1;
			break;	// 리듬마다 가장 가까운 날 하나
		}
	}
	if ( !any ) printf(" 없음");
	printf("\r\n");
}

int main(int argc, char **argv)
{
	int uy, um, ud, ty, tm_, td, full, k;
	long t;
	time_t now;
	struct tm *lt;

	if ( argc < 4 ) {
		printf("usage: %s YYYY MM DD [full]\n", argv[0]);
		return 1;
	}
	uy = atoi(argv[1]); um = atoi(argv[2]); ud = atoi(argv[3]);
	full = (argc > 4 && !strcmp(argv[4], "full"));

	now = time(NULL);
	lt = localtime(&now);
	ty = lt->tm_year + 1900; tm_ = lt->tm_mon + 1; td = lt->tm_mday;

	if ( uy < 1900 || um < 1 || um > 12 || ud < 1 || ud > 31 || day_number(uy, um, ud) > day_number(ty, tm_, td) ) {
		printf(" 생일이 바르게 등록되어 있지 않아 바이오리듬을 볼 수 없습니다.\r\n");
		printf(" " C_GRAY "(pe 명령으로 회원 정보의 생일을 고쳐 주세요)" C_WHITE "\r\n");
		return 0;
	}
	t = day_number(ty, tm_, td) - day_number(uy, um, ud);	// 태어난 날이 0 일째

	if ( !full ) {
		printf(" " C_YELLOW "오늘의 바이오리듬" C_WHITE "  " C_GRAY "%d년 %d월 %d일 / 태어난 지 %ld 일째" C_WHITE "\r\n", ty, tm_, td, t);
		today_lines(t);
		advice(t, 1);
		return 0;
	}

	printf(" %d년 %d월 %d일생, 오늘은 태어난 지 " C_YELLOW "%ld" C_WHITE " 일째입니다. " C_GRAY "(%d년 %d월 %d일)" C_WHITE "\r\n\r\n",
		uy, um, ud, t, ty, tm_, td);
	today_lines(t);
	printf("\r\n");
	chart(t, ty, tm_, td);
	printf(" " C_GRAY "      ");
	for ( k = 0; k < 4; k++ ) printf("%s●" C_GRAY " %s(%d일)  ", R[k].color, R[k].name, R[k].days);
	printf(C_YELLOW "◆" C_GRAY " 오늘" C_WHITE "\r\n");
	criticals(t, ty, tm_, td);
	advice(t, 2);
	return 0;
}
