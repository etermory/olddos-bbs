#include "main.h"
#include <math.h>

// ------------------------------------------------------------------
// 해와 달 : 도시별 해 뜨고 지는 시각, 오늘의 달 모양
//   bin/sun <호스트이름> <아이디> <tty>
// 해: open-meteo (키 없이), 달: 평균 삭망월로 계산 (보름/그믐 날짜는 하루쯤 어긋날 수 있음)
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "해와 달";

char tty[10];

char host_name[256];

#define S_W		"\033[=15F"
#define S_Y		"\033[=14F"
#define S_C		"\033[=11F"
#define S_G		"\033[=7F"
#define S_R		"\033[=12F"

struct city { const char *name; double lat, lon; };
static const city cities[] = {
	{ "서울", 37.57, 126.98 }, { "인천", 37.46, 126.71 }, { "수원", 37.26, 127.03 }, { "춘천", 37.88, 127.73 },
	{ "강릉", 37.75, 128.88 }, { "청주", 36.64, 127.49 }, { "대전", 36.35, 127.38 }, { "전주", 35.82, 127.15 },
	{ "광주", 35.16, 126.85 }, { "대구", 35.87, 128.60 }, { "포항", 36.02, 129.34 }, { "울산", 35.54, 129.31 },
	{ "부산", 35.18, 129.08 }, { "창원", 35.23, 128.68 }, { "제주", 33.50, 126.53 }, { "독도", 37.24, 131.87 },
};
static const int city_count = sizeof(cities) / sizeof(cities[0]);

#define SYNODIC		29.530588853		// 평균 삭망월 (일)
#define NEW_MOON_JD	2451550.26			// 2000-01-06 18:14 UTC 의 삭

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

static void at(int row, int col) { printf("\033[%d;%dH", row, col); }

// ------------------------------------------------------------------
// 해 (open-meteo)
// ------------------------------------------------------------------
struct sun_day { std::string date, rise, set; long daylight; };

static bool get_sun(const city &c, std::vector<sun_day> &days)
{
	char url[512];
	snprintf(url, sizeof(url), "http://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f"
			"&daily=sunrise,sunset,daylight_duration&timezone=Asia%%2FSeoul&forecast_days=2", c.lat, c.lon);
	std::string j;
	if ( !download_text(url, j) ) return false;
	// "time":["2026-10-06","2026-10-07"], "sunrise":["2026-10-06T06:31",...] ...
	std::vector<std::string> keys;
	keys.push_back("\"time\":["); keys.push_back("\"sunrise\":["); keys.push_back("\"sunset\":["); keys.push_back("\"daylight_duration\":[");
	std::vector<std::vector<std::string> > cols;
	std::string::size_type daily = j.find("\"daily\":");
	if ( daily == std::string::npos ) return false;
	for ( unsigned int k = 0; k < keys.size(); k++ ) {
		std::string::size_type p = j.find(keys[k], daily);
		if ( p == std::string::npos ) return false;
		p += keys[k].size();
		std::string::size_type e = j.find(']', p);
		std::vector<std::string> v = split_string(j.substr(p, e - p), ',');
		for ( unsigned int i = 0; i < v.size(); i++ ) {
			std::string s = v[i];
			if ( !s.empty() && s[0] == '"' ) s = s.substr(1, s.size() - 2);
			v[i] = s;
		}
		cols.push_back(v);
	}
	for ( unsigned int i = 0; i < cols[0].size() && i < cols[1].size() && i < cols[2].size() && i < cols[3].size(); i++ ) {
		sun_day d;
		d.date = cols[0][i];
		d.rise = cols[1][i].size() >= 16 ? cols[1][i].substr(11, 5) : "-";
		d.set = cols[2][i].size() >= 16 ? cols[2][i].substr(11, 5) : "-";
		d.daylight = atol(cols[3][i].c_str());
		days.push_back(d);
	}
	return !days.empty();
}

// ------------------------------------------------------------------
// 달 (계산)
// ------------------------------------------------------------------
static double moon_age(time_t t)
{
	double jd = t / 86400.0 + 2440587.5;
	double age = fmod(jd - NEW_MOON_JD, SYNODIC);
	return age < 0 ? age + SYNODIC : age;
}

static const char *phase_name(double age)
{
	double e = SYNODIC / 8;		// 여덟 단계
	if ( age < e / 2 || age >= SYNODIC - e / 2 ) return "삭 (보이지 않는 달)";
	if ( age < e * 1.5 ) return "초승달";
	if ( age < e * 2.5 ) return "상현달";
	if ( age < e * 3.5 ) return "차오르는 달";
	if ( age < e * 4.5 ) return "보름달";
	if ( age < e * 5.5 ) return "기우는 달";
	if ( age < e * 6.5 ) return "하현달";
	return "그믐달";
}

static std::string when_str(time_t t)
{
	static const char *w[] = { "일", "월", "화", "수", "목", "금", "토" };
	struct tm *tm = localtime(&t);
	char buf[64];
	snprintf(buf, sizeof(buf), "%d월 %d일 (%s) %s %d시쯤", tm->tm_mon + 1, tm->tm_mday, w[tm->tm_wday],
			tm->tm_hour < 12 ? "오전" : "오후", tm->tm_hour % 12 == 0 ? 12 : tm->tm_hour % 12);
	return buf;
}

// 달 그림 (반지름 5, 한 칸이 두 글자 폭). 밝은 쪽은 노란 ●, 어두운 쪽은 회색 ○
static void draw_moon(double age, int top, int left)
{
	const int R = 5;
	double k = cos(2 * M_PI * age / SYNODIC);		// 삭 1, 보름 -1
	bool waxing = age < SYNODIC / 2;					// 차오를 때는 오른쪽이 밝다 (북반구)
	for ( int y = -R; y <= R; y++ ) {
		at(top + y + R, left);
		double w = sqrt((double)(R * R - y * y) + 0.5);
		for ( int x = -R; x <= R; x++ ) {
			if ( x * x + y * y > R * R + R / 2 ) { printf("  "); continue; }
			bool lit = waxing ? (x >= k * w) : (x <= -k * w);
			printf("%s", lit ? S_Y "●" : S_G "○");
		}
	}
	printf(S_W);
}

static std::string lunar_today(void)
{
	bool ok;
	std::vector<std::string> l = exec_command((char*)(std::string(getenv("HANULSO") ? getenv("HANULSO") : ".") + "/bin/lunar --today").c_str(), &ok);
	if ( l.empty() ) return "";
	std::string s = l[0];
	std::string::size_type tab = s.find('\t');
	return trim(s.substr(0, tab));
}

static std::string hm(long sec)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%ld시간 %02ld분", sec / 3600, sec % 3600 / 60);
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

	int ci = 0;			// 서울
	std::string message;
	std::string lunar = lunar_today();

	while ( 1 ) {
		print_header(title);
		printf("자료를 받아오는 중입니다...");
		fflush(stdout);
		std::vector<sun_day> days;
		bool ok = get_sun(cities[ci], days);

		print_header(title);
		printf("\r\n  " S_Y "◆ 해" S_W "  " S_C "%s" S_W "\r\n", cities[ci].name);
		if ( !ok ) {
			printf("    " S_R "해 뜨는 시각을 받아오지 못했습니다." S_W "\r\n");
		}
		for ( unsigned int i = 0; i < days.size() && i < 2; i++ ) {
			printf("    %s  해 뜸 " S_Y "%s" S_W "   해 짐 " S_Y "%s" S_W "   " S_G "낮 %s" S_W "\r\n",
					i == 0 ? "오늘" : "내일", days[i].rise.c_str(), days[i].set.c_str(), hm(days[i].daylight).c_str());
		}

		time_t now = time(NULL);
		double age = moon_age(now);
		double lit = (1 - cos(2 * M_PI * age / SYNODIC)) / 2 * 100;
		time_t full = now + (time_t)(((age < SYNODIC / 2) ? SYNODIC / 2 - age : SYNODIC * 1.5 - age) * 86400);
		time_t newm = now + (time_t)((SYNODIC - age) * 86400);
		printf("\r\n  " S_Y "◆ 달" S_W "  %s\r\n", lunar.c_str());
		printf("    " S_C "%s" S_W "   " S_G "달 나이 %.1f일, 밝은 부분 %.0f%%" S_W "\r\n", phase_name(age), age, lit);
		printf("    다음 보름달  %s\r\n", when_str(full).c_str());
		printf("    다음 삭(그믐) %s\r\n", when_str(newm).c_str());
		printf("    " S_G "(평균 주기로 계산, 하루쯤 어긋날 수 있음)" S_W "\r\n");

		// 달 그림 (오른쪽 5~15 줄)
		draw_moon(age, 5, 56);

		// 도시 목록 (달 그림 아래)
		at(17, 1);
		printf("  " S_G "도시" S_W);
		for ( int i = 0; i < city_count; i++ ) {
			if ( i == 8 ) printf("\r\n      ");
			printf(" %s%2d.%s" S_W, i == ci ? S_Y : "", i + 1, cities[i].name);
		}
		printf("\r\n  " S_G "해: open-meteo.com" S_W "\r\n");
		if ( !message.empty() ) printf("  %s\r\n", message.c_str());

		char cmd[64];
		printf(ESC_ENG);
		at(21, 1);
		printf("\r\n도시(번호/이름) 상위메뉴(P) 종료(X) >> ");
		line_input(cmd, 12);
		message.clear();

		std::string c = trim(cmd);
		if ( c.empty() ) continue;
		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") || !strcasecmp(c.c_str(), "q") ) break;
		int n = atoi(c.c_str());
		int found = -1;
		if ( n >= 1 && n <= city_count ) found = n - 1;
		for ( int i = 0; i < city_count && found < 0; i++ ) if ( c == cities[i].name ) found = i;
		if ( found < 0 ) message = S_R "그런 도시는 없습니다. 번호나 이름을 넣으세요." S_W;
		else ci = found;
	}

	host_close();
	return 0;
}
