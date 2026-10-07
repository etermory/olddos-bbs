#include "main.h"
#include "lunarcalc.h"

// 만세력 / 음력 변환
// 음력 계산은 lunarcalc.cpp

struct termio sys_term;

char title[1024] = "만세력 / 음력";

char tty[10];

char host_name[256];

static const char *weekdays[] = { "일", "월", "화", "수", "목", "금", "토" };

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
