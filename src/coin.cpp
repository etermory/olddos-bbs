#include "main.h"

// ------------------------------------------------------------------
// 코인 시세 (업비트 원화 마켓, 24 시간 거래대금이 큰 순서로)
//   bin/coin <호스트이름> <아이디> <tty>
// 자료: 업비트 공개 API (키 없이), 투자 참고용 아님
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "코인 시세";

char tty[10];

char host_name[256];

#define C_W		"\033[=15F"
#define C_Y		"\033[=14F"
#define C_R		"\033[=12F"
#define C_C		"\033[=11F"
#define C_G		"\033[=7F"

#define SHOW	15

struct coin {
	std::string market, name;
	double price, change_rate, high, low, open, prev, volume_krw;
	std::string change;		// RISE / FALL / EVEN
};

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

// 천 단위 쉼표 (소수 자리수 decimals)
static std::string comma(double v, int decimals)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%.*f", decimals, v < 0 ? -v : v);
	std::string s = buf;
	std::size_t dot = s.find('.');
	std::string ip = (dot == std::string::npos) ? s : s.substr(0, dot);
	std::string fp = (dot == std::string::npos) ? "" : s.substr(dot);
	std::string r;
	int n = ip.size();
	for ( int i = 0; i < n; i++ ) {
		r += ip[i];
		if ( (n - i - 1) % 3 == 0 && i != n - 1 ) r += ',';
	}
	return (v < 0 ? "-" : "") + r + fp;
}

// 값에 맞는 소수 자리 (비싼 것은 원 단위, 싼 것은 소수까지)
static std::string price_str(double v)
{
	if ( v >= 100 ) return comma(v, 0);
	if ( v >= 1 ) return comma(v, 2);
	return comma(v, 4);
}

static double jnum(const std::string &j, const char *key)
{
	std::string k = std::string("\"") + key + "\":";
	std::string::size_type p = j.find(k);
	return p == std::string::npos ? 0 : atof(j.c_str() + p + k.size());
}

static std::string jstr(const std::string &j, const char *key)
{
	std::string k = std::string("\"") + key + "\":\"";
	std::string::size_type p = j.find(k);
	if ( p == std::string::npos ) return "";
	p += k.size();
	std::string::size_type e = j.find('"', p);
	return e == std::string::npos ? "" : j.substr(p, e - p);
}

// JSON 배열 [{...},{...}] 을 객체 문자열들로
static std::vector<std::string> objects(const std::string &j)
{
	std::vector<std::string> out;
	std::string::size_type p = 0;
	while ( (p = j.find('{', p)) != std::string::npos ) {
		std::string::size_type e = j.find('}', p);
		if ( e == std::string::npos ) break;
		out.push_back(j.substr(p, e - p + 1));
		p = e + 1;
	}
	return out;
}

static bool load(std::vector<coin> &list)
{
	list.clear();
	// 원화 마켓과 한글 이름
	std::string all;
	if ( !download_text("https://api.upbit.com/v1/market/all?isDetails=false", all) ) return false;
	std::map<std::string, std::string> names;
	std::string markets;
	std::vector<std::string> obj = objects(all);
	for ( unsigned int i = 0; i < obj.size(); i++ ) {
		std::string m = jstr(obj[i], "market");
		if ( m.compare(0, 4, "KRW-") != 0 ) continue;
		names[m] = utf8_to_cp949(jstr(obj[i], "korean_name"));
		if ( !markets.empty() ) markets += ",";
		markets += m;
	}
	if ( markets.empty() ) return false;

	std::string t;
	if ( !download_text("https://api.upbit.com/v1/ticker?markets=" + markets, t) ) return false;
	obj = objects(t);
	for ( unsigned int i = 0; i < obj.size(); i++ ) {
		coin c;
		c.market = jstr(obj[i], "market");
		if ( c.market.empty() ) continue;
		c.name = names[c.market];
		c.price = jnum(obj[i], "trade_price");
		c.change_rate = jnum(obj[i], "signed_change_rate") * 100;
		c.high = jnum(obj[i], "high_price");
		c.low = jnum(obj[i], "low_price");
		c.open = jnum(obj[i], "opening_price");
		c.prev = jnum(obj[i], "prev_closing_price");
		c.volume_krw = jnum(obj[i], "acc_trade_price_24h");
		c.change = jstr(obj[i], "change");
		list.push_back(c);
	}
	// 24 시간 거래대금이 큰 순서
	for ( unsigned int i = 1; i < list.size(); i++ )
		for ( unsigned int k = i; k > 0 && list[k].volume_krw > list[k - 1].volume_krw; k-- ) std::swap(list[k], list[k - 1]);
	return !list.empty();
}

static std::string change_str(const coin &c)
{
	char buf[64];
	if ( c.change == "RISE" ) snprintf(buf, sizeof(buf), C_R "▲%6.2f%%" C_W, c.change_rate);
	else if ( c.change == "FALL" ) snprintf(buf, sizeof(buf), C_C "▼%6.2f%%" C_W, -c.change_rate);
	else snprintf(buf, sizeof(buf), "  %6.2f%%", 0.0);
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

	std::vector<coin> list;
	std::string detail;
	bool reload = true;
	char when[32] = "";

	while ( 1 ) {
		if ( reload ) {
			print_header(title);
			printf("자료를 받아오는 중입니다...");
			fflush(stdout);
			if ( !load(list) ) {
				printf("\r\n\r\n시세를 받아오지 못했습니다.\r\n");
				printf("\r\n[Enter] 를 누르세요.");
				press_enter();
				host_close();
			}
			time_t t = time(NULL);
			strftime(when, sizeof(when), "%m/%d %H:%M:%S", localtime(&t));
			reload = false;
		}

		print_header(title);
		// 4 + 이름 14 + 현재가 16 + 전일대비 9 + 거래대금 12 = 한 줄 약 64 칸
		printf("  " C_G "%3s %-14s %16s %10s %13s  %-8s" C_W "\r\n", "", "이름", "현재가(원)", "전일대비", "거래대금(24h)", "코드");
		for ( unsigned int i = 0; i < list.size() && i < SHOW; i++ ) {
			const coin &c = list[i];
			char vol[32];
			snprintf(vol, sizeof(vol), "%s억", comma(c.volume_krw / 100000000.0, 0).c_str());
			printf("  %3d %-14s %16s %s %13s  " C_G "%-8s" C_W "\r\n", i + 1,
					string_truncate(c.name, 14, "").c_str(), price_str(c.price).c_str(), change_str(c).c_str(),
					vol, c.market.substr(4).c_str());
		}
		printf("  " C_G "업비트 원화 마켓 %s 기준, 거래대금 순 (투자 참고용 아님)" C_W "\r\n", when);
		// (24 줄 화면: 목록 다음에 바로, 빈 줄 없이)
		if ( !detail.empty() ) printf("%s\r\n", detail.c_str());

		char cmd[64];
		printf(ESC_ENG);
		printf("\r\n새로 보기(Enter) 코드나 이름(예: BTC) 상위메뉴(P) 종료(X) >> ");
		line_input(cmd, 12);
		detail.clear();

		std::string c = trim(cmd);
		if ( c.empty() ) { reload = true; continue; }
		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") || !strcasecmp(c.c_str(), "q") ) break;

		// 코드(BTC) 나 이름(비트코인)으로 찾아 자세히
		const coin *found = NULL;
		for ( unsigned int i = 0; i < list.size(); i++ ) {
			if ( !strcasecmp(list[i].market.substr(4).c_str(), c.c_str()) || list[i].name == c ) { found = &list[i]; break; }
		}
		if ( found == NULL ) {
			detail = "  " C_R "'" + string_truncate(display_text(c), 20, "") + "' 을(를) 찾지 못했습니다. (원화 마켓 코드나 이름)" C_W;
			continue;
		}
		char buf[512];
		// 두 줄, 80 칸 안에
		snprintf(buf, sizeof(buf), "  " C_Y "%s (%s)" C_W "  현재 %s원 %s  " C_G "전일 %s" C_W "\r\n"
				"     시가 %s   고가 %s   저가 %s",
				string_truncate(found->name, 14, "").c_str(), found->market.substr(4).c_str(), price_str(found->price).c_str(),
				change_str(*found).c_str(), price_str(found->prev).c_str(), price_str(found->open).c_str(),
				price_str(found->high).c_str(), price_str(found->low).c_str());
		detail = buf;
	}

	host_close();
	return 0;
}
