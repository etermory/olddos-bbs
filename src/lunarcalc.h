#ifndef LUNARCALC_H
#define LUNARCALC_H

// 음력 / 간지 계산 (lunarcalc.cpp)

#define LUNAR_BASE_YEAR		1000
#define LUNAR_YEAR_COUNT	1051
#define SOLAR_LUNAR_DAY_DIFF	43

// 메뉴에서 받는 연도 범위
#define MIN_YEAR	1900
#define MAX_YEAR	2049

extern const char *cheongan[10];
extern const char *jiji[12];
extern const char *ddi[12];

void init_lunar(void);			// 맨 처음 한 번 부른다
int leap_month(int year);		// 윤달 (없으면 0)
int lunar_month_days(int year, int month, bool leap);
int solar_month_days(int year, int month);
long lunar_abs_days(int year, int month, int day, bool leap);	// 간지 계산용 날 번호
void solar_to_lunar(int sy, int sm, int sd, int *ly, int *lm, int *ld, bool *leap);
void lunar_to_solar(int ly, int lm, int ld, bool leap, int *sy, int *sm, int *sd);
std::string gapja_string(int ly, int lm, int ld, bool leap);
std::string ddi_string(int ly);
int day_of_week(int y, int m, int d);	// 0: 일요일
bool valid_solar(int y, int m, int d);
bool valid_lunar(int y, int m, int d, bool leap);

#endif
