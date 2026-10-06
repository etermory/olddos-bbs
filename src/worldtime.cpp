#include "main.h"

// ------------------------------------------------------------------
// 세계 시각 : 주요 도시의 지금 시각, 서울과의 시차, 서머타임
//   bin/worldtime <호스트이름> <아이디> <tty>
// 서버의 시간대 자료(tzdata)를 쓴다. "15:00" 처럼 치면 서울이 그 시각일 때 각 도시의 시각.
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "세계 시각";

char tty[10];

char host_name[256];

#define T_W		"\033[=15F"
#define T_Y		"\033[=14F"
#define T_C		"\033[=11F"
#define T_G		"\033[=7F"
#define T_R		"\033[=12F"
#define T_M		"\033[=13F"

struct zone { const char *name, *tz; };
static const zone zones[] = {
	{ "서울", "Asia/Seoul" },          { "도쿄", "Asia/Tokyo" },          { "베이징", "Asia/Shanghai" },
	{ "홍콩", "Asia/Hong_Kong" },      { "싱가포르", "Asia/Singapore" },  { "방콕", "Asia/Bangkok" },
	{ "하노이", "Asia/Ho_Chi_Minh" },  { "뉴델리", "Asia/Kolkata" },      { "두바이", "Asia/Dubai" },
	{ "모스크바", "Europe/Moscow" },   { "이스탄불", "Europe/Istanbul" },
	{ "런던", "Europe/London" },       { "파리", "Europe/Paris" },        { "베를린", "Europe/Berlin" },
	{ "뉴욕", "America/New_York" },    { "시카고", "America/Chicago" },   { "덴버", "America/Denver" },
	{ "로스앤젤레스", "America/Los_Angeles" }, { "상파울루", "America/Sao_Paulo" },
	{ "시드니", "Australia/Sydney" },  { "오클랜드", "Pacific/Auckland" }, { "호놀룰루", "Pacific/Honolulu" },
};
static const int zone_count = sizeof(zones) / sizeof(zones[0]);

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

// 그 시간대의 시각 (TZ 를 바꿔 localtime 을 부르고 되돌린다)
static struct tm in_zone(time_t t, const char *tz, long *gmtoff)
{
	static std::string orig = getenv("TZ") ? getenv("TZ") : "";
	setenv("TZ", tz, 1);
	tzset();
	struct tm r;
	localtime_r(&t, &r);
	*gmtoff = r.tm_gmtoff;
	if ( orig.empty() ) unsetenv("TZ"); else setenv("TZ", orig.c_str(), 1);
	tzset();
	return r;
}

static const char *wday[] = { "일", "월", "화", "수", "목", "금", "토" };

// 한 칸 (36 칸): 이름 12 + 시각 5 + 어제/내일 4 + 시차 7 + 서머 5
static std::string cell(const zone &z, time_t t, const struct tm &seoul, long seoul_off)
{
	long off;
	struct tm m = in_zone(t, z.tz, &off);
	long diff = off - seoul_off;		// 초
	char d[16];
	if ( diff == 0 ) snprintf(d, sizeof(d), "   -   ");
	else if ( diff % 3600 == 0 ) snprintf(d, sizeof(d), "%+3ld시간", diff / 3600);
	else snprintf(d, sizeof(d), "%+3ld:%02ld ", diff / 3600, labs(diff % 3600) / 60);
	// 서울 날짜와 다르면 어제/내일
	int dd = (m.tm_year - seoul.tm_year) * 400 + m.tm_yday - seoul.tm_yday;
	const char *day = dd < 0 ? T_G "어제" T_W : dd > 0 ? T_C "내일" T_W : "    ";
	bool night = m.tm_hour < 6 || m.tm_hour >= 19;
	char buf[256];
	snprintf(buf, sizeof(buf), "%-12s %s%02d:%02d" T_W " %s " T_G "%-7s" T_W "%s" T_W,
			z.name, night ? T_G : T_Y, m.tm_hour, m.tm_min, day, d, m.tm_isdst > 0 ? T_M " 서머" : "     ");
	return buf;
}

int main(int argc, char **argv)
{
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

	time_t when = 0;		// 0 이면 지금, 아니면 서울이 그 시각일 때
	std::string message;

	while ( 1 ) {
		time_t t = when ? when : time(NULL);
		long seoul_off;
		struct tm seoul = in_zone(t, "Asia/Seoul", &seoul_off);

		print_header(title);
		if ( when ) {
			printf("\r\n  " T_Y "서울이 %d월 %d일 (%s) %02d:%02d 일 때" T_W "\r\n", seoul.tm_mon + 1, seoul.tm_mday,
					wday[seoul.tm_wday], seoul.tm_hour, seoul.tm_min);
		} else {
			printf("\r\n  " T_Y "지금 서울 %d월 %d일 (%s) %02d:%02d" T_W "\r\n", seoul.tm_mon + 1, seoul.tm_mday,
					wday[seoul.tm_wday], seoul.tm_hour, seoul.tm_min);
		}
		printf("\r\n");

		// 두 단 (2 + 36 + 4 + 36 = 78 칸)
		int rows = (zone_count + 1) / 2;
		for ( int r = 0; r < rows; r++ ) {
			printf("  %s", cell(zones[r], t, seoul, seoul_off).c_str());
			if ( r + rows < zone_count ) printf("    %s", cell(zones[r + rows], t, seoul, seoul_off).c_str());
			printf("\r\n");
		}
		printf("\r\n  " T_G "시차는 서울 기준.  노란 시각은 낮(6~19시), 회색은 밤.  서머 = 서머타임 중" T_W "\r\n");
		if ( !message.empty() ) printf("  %s\r\n", message.c_str());

		char cmd[64];
		printf(ESC_ENG);
		printf("\r\n새로 보기(Enter)  서울이 그 시각일 때(예: 15:00)  상위메뉴(P) 종료(X) >> ");
		line_input(cmd, 8);
		message.clear();

		std::string c = trim(cmd);
		if ( c.empty() ) { when = 0; continue; }
		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") || !strcasecmp(c.c_str(), "q") ) break;
		int h, mi = 0;
		if ( (sscanf(c.c_str(), "%d:%d", &h, &mi) >= 1) && h >= 0 && h <= 23 && mi >= 0 && mi <= 59 ) {
			// 오늘 서울 날짜의 그 시각
			time_t now = time(NULL);
			long off;
			struct tm s = in_zone(now, "Asia/Seoul", &off);
			s.tm_hour = h; s.tm_min = mi; s.tm_sec = 0;
			// 서울은 서머타임이 없으므로 UTC 로 계산
			when = (time_t)(timegm(&s) - off);
		} else {
			message = T_R "시각은 15:00 처럼 넣으세요." T_W;
		}
	}

	host_close();
	return 0;
}
