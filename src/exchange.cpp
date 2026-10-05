#include "main.h"

// 환율 정보 (자료: ExchangeRate-API, 키 없이 쓰는 무료 주소, 하루 한 번 갱신)

struct termio sys_term;

char title[1024] = "환율 정보";

char tty[10];

char host_name[256];

struct currency {
	const char *code;
	const char *name;
	int unit;		// 표시 단위 (엔/동 등은 100 단위)
};

static const currency currencies[] = {
	{ "USD", "미국 달러", 1 },
	{ "JPY", "일본 엔", 100 },
	{ "EUR", "유로", 1 },
	{ "CNY", "중국 위안", 1 },
	{ "GBP", "영국 파운드", 1 },
	{ "HKD", "홍콩 달러", 1 },
	{ "TWD", "대만 달러", 1 },
	{ "SGD", "싱가포르", 1 },
	{ "THB", "태국 바트", 1 },
	{ "VND", "베트남 동", 100 },
	{ "PHP", "필리핀 페소", 1 },
	{ "IDR", "인도네시아", 100 },
	{ "MYR", "말레이시아", 1 },
	{ "INR", "인도 루피", 1 },
	{ "AUD", "호주 달러", 1 },
	{ "NZD", "뉴질랜드", 1 },
	{ "CAD", "캐나다 달러", 1 },
	{ "CHF", "스위스 프랑", 1 },
	{ "SEK", "스웨덴", 1 },
	{ "RUB", "러시아 루블", 1 },
	{ "MNT", "몽골 투그릭", 100 },
	{ "AED", "아랍에미리트", 1 },
};

static const int currency_count = sizeof(currencies) / sizeof(currencies[0]);

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

// JSON 에서 "KEY":숫자 값을 찾는다
bool json_number(const std::string &json, const char *key, double *value)
{
	std::string k = std::string("\"") + key + "\":";
	std::size_t p = json.find(k);
	if ( p == std::string::npos ) return false;
	*value = atof(json.c_str() + p + k.size());
	return true;
}

// JSON 에서 "KEY":"문자열" 값을 찾는다
std::string json_string(const std::string &json, const char *key)
{
	std::string k = std::string("\"") + key + "\":\"";
	std::size_t p = json.find(k);
	if ( p == std::string::npos ) return "";
	p += k.size();
	std::size_t e = json.find('"', p);
	if ( e == std::string::npos ) return "";
	return json.substr(p, e - p);
}

// 1 KRW 기준 환율을 받아 온다
bool get_rates(std::string &json)
{
	// 임시 파일 없이 받는다 (받는 중에 끊겨도 tmp 에 남지 않게)
	bool ok = download_text("http://open.er-api.com/v6/latest/KRW", json);

	return ok && json.find("\"result\":\"success\"") != std::string::npos;
}

// 1 단위 외화의 원화 값
double krw_per(const std::string &json, const char *code)
{
	double rate;
	if ( !json_number(json, code, &rate) || rate <= 0 ) return 0;
	return 1.0 / rate;
}

// 천 단위 쉼표
std::string comma(double v, int decimals)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%.*f", decimals, v < 0 ? -v : v);
	std::string s = buf;
	std::size_t dot = s.find('.');
	std::string ip = (dot == std::string::npos) ? s : s.substr(0, dot);
	std::string fp = (dot == std::string::npos) ? "" : s.substr(dot);
	std::string r;
	int n = ip.size();
	for (int i=0; i<n; i++) {
		r += ip[i];
		if ( (n - i - 1) % 3 == 0 && i != n - 1 ) r += ',';
	}
	return (v < 0 ? "-" : "") + r + fp;
}

const currency *find_currency(const std::string &code)
{
	for (int i=0; i<currency_count; i++) {
		if ( !strcasecmp(code.c_str(), currencies[i].code) ) return &currencies[i];
	}
	return NULL;
}

int main(int argc, char **argv)
{
	// bin/exchange --usd : 1 달러가 몇 원인지 "1,380.50" (txt 파일의 [usd_rate])
	if ( argc > 1 && !strcmp(argv[1], "--usd") ) {
		std::string json;
		double v = get_rates(json) ? krw_per(json, "USD") : 0;
		if ( v <= 0 ) return 1;
		printf("%s\n", comma(v, 2).c_str());
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

	print_header(title);
	printf("자료를 받아오는 중입니다...");
	fflush(stdout);

	std::string json;
	if ( !get_rates(json) ) {
		printf("\r\n\r\n환율 정보를 받아오지 못했습니다.\r\n");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		host_close();
	}

	std::string updated = json_string(json, "time_last_update_utc");
	std::string result;

	while (1) {
		print_header(title);

		// 두 단으로 출력
		int rows = (currency_count + 1) / 2;
		for (int r=0; r<rows; r++) {
			for (int c=0; c<2; c++) {
				int i = c * rows + r;
				if ( i >= currency_count ) break;
				const currency &cu = currencies[i];
				std::string unit = (cu.unit == 100) ? "(100)" : "";
				double v = krw_per(json, cu.code) * cu.unit;
				// 한 칸 33 자: 코드(3) 이름(12) 단위(5) 금액(10)원 -> 두 칸이면 76 자
				printf("%s%-3s %-12s%5s%10s원", (c == 0) ? "  " : "    ",
						cu.code, string_truncate(cu.name, 12, "").c_str(), unit.c_str(), comma(v, 2).c_str());
			}
			printf("\r\n");
		}

		printf("%s\r\n", repeat("━", 40).c_str());
		printf(" 기준: %s (하루 1회 갱신, 참고용)\r\n", updated.c_str());
		printf(" 자료: Rates By Exchange Rate API (www.exchangerate-api.com)\r\n");
		if ( !result.empty() ) {
			printf("\r\n %s\r\n", result.c_str());
		}

		char cmd[64];
		printf(ESC_ENG);
		printf("\r\n환산(예: 100 USD, 50000 KRW JPY) 상위메뉴(P) 종료(X)\r\n선택 >> ");
		line_input(cmd, 30);

		std::vector<std::string> args = split_string(trim(cmd), ' ');
		if ( args.size() == 0 ) continue;
		if ( !strcasecmp(args[0].c_str(), "p") || !strcasecmp(args[0].c_str(), "x") ) break;

		double amount = atof(args[0].c_str());
		if ( args.size() < 2 || amount <= 0 ) {
			result = "입력 예: 100 USD (달러를 원화로), 50000 KRW JPY (원화를 엔화로)";
			continue;
		}

		char buf[256];
		if ( !strcasecmp(args[1].c_str(), "KRW") ) {
			// 원화 -> 외화
			const currency *to = (args.size() > 2) ? find_currency(args[2]) : find_currency("USD");
			if ( to == NULL ) {
				result = "지원하지 않는 통화입니다.";
				continue;
			}
			double per = krw_per(json, to->code);
			if ( per <= 0 ) {
				result = "환율 정보가 없습니다.";
				continue;
			}
			snprintf(buf, sizeof(buf), "%s원 = %s %s", comma(amount, 0).c_str(),
					comma(amount / per, 2).c_str(), to->code);
		} else {
			// 외화 -> 원화
			const currency *from = find_currency(args[1]);
			if ( from == NULL ) {
				result = "지원하지 않는 통화입니다.";
				continue;
			}
			double per = krw_per(json, from->code);
			snprintf(buf, sizeof(buf), "%s %s = %s원", comma(amount, 2).c_str(), from->code,
					comma(amount * per, 0).c_str());
		}
		result = buf;
	}

	ioctl(0, TCSETAF, &sys_term);
	return 0;
}
