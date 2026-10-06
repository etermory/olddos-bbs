#include "main.h"

// ------------------------------------------------------------------
// 로또 6/45 : 회차별 당첨번호와 당첨금, 번호 추천, 내 번호 맞춰 보기
//   bin/lotto <호스트이름> <아이디> <tty>
// 자료: smok95.github.io/lotto (동행복권 발표를 옮겨 둔 공개 자료, JSON)
//   (동행복권 조회 주소는 보안 페이지를 돌려줘서 바로 받을 수 없음)
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "로또 6/45";

char tty[10];

char host_name[256];

#define LOTTO_URL	"https://smok95.github.io/lotto/results/"

#define L_WHITE		"\033[=15F"
#define L_YELLOW	"\033[=14F"
#define L_RED		"\033[=12F"
#define L_GREEN		"\033[=10F"
#define L_CYAN		"\033[=11F"
#define L_GRAY		"\033[=7F"

struct draw {
	int no;
	int nums[6];
	int bonus;
	std::string date;		// YYYY-MM-DD
	long long prize[5], winners[5];
	long long sales;
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

static std::string comma(long long v)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%lld", v);
	std::string s = buf, r;
	int n = s.size();
	for ( int i = 0; i < n; i++ ) {
		r += s[i];
		if ( (n - i - 1) % 3 == 0 && i != n - 1 ) r += ',';
	}
	return r;
}

// JSON 에서 "key": 다음의 숫자 (from 부터 찾고, 찾은 곳 다음을 *next 에)
static long long json_num(const std::string &j, const char *key, std::string::size_type from = 0, std::string::size_type *next = NULL)
{
	std::string k = std::string("\"") + key + "\":";
	std::string::size_type p = j.find(k, from);
	if ( p == std::string::npos ) { if ( next ) *next = std::string::npos; return -1; }
	if ( next ) *next = p + k.size();
	return atoll(j.c_str() + p + k.size());
}

// 회차 받기 (no <= 0 이면 가장 최근)
static bool get_draw(int no, draw &d)
{
	char url[256];
	if ( no > 0 ) snprintf(url, sizeof(url), LOTTO_URL "%d.json", no);
	else snprintf(url, sizeof(url), LOTTO_URL "latest.json");
	std::string j;
	if ( !download_text(url, j) || j.find("\"draw_no\"") == std::string::npos ) return false;

	d.no = (int)json_num(j, "draw_no");
	std::string::size_type p = j.find("\"numbers\":[");
	if ( p == std::string::npos ) return false;
	p += 11;
	for ( int i = 0; i < 6; i++ ) {
		d.nums[i] = atoi(j.c_str() + p);
		p = j.find_first_of(",]", p);
		if ( p == std::string::npos ) return false;
		p++;
	}
	d.bonus = (int)json_num(j, "bonus_no");
	p = j.find("\"date\":\"");
	d.date = (p != std::string::npos) ? j.substr(p + 8, 10) : "";
	std::string::size_type at = j.find("\"divisions\"");
	for ( int i = 0; i < 5; i++ ) {
		d.prize[i] = json_num(j, "prize", at, &at);
		d.winners[i] = json_num(j, "winners", at, &at);
	}
	d.sales = json_num(j, "total_sales_amount");
	return d.no > 0;
}

// 공 색 (실제 로또 공처럼: 1~10 노랑, 11~20 파랑, 21~30 빨강, 31~40 회색, 41~45 초록)
// 파랑은 바탕과 같아서 하늘색으로
static std::string ball(int n)
{
	const char *c = n <= 10 ? L_YELLOW : n <= 20 ? L_CYAN : n <= 30 ? L_RED : n <= 40 ? L_GRAY : L_GREEN;
	char buf[64];
	snprintf(buf, sizeof(buf), "%s\033[7m %2d \033[0m" L_WHITE, c, n);
	return buf;
}

static std::string weekday_of(const std::string &date)
{
	struct tm t;
	memset(&t, 0, sizeof(t));
	if ( sscanf(date.c_str(), "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) != 3 ) return "";
	t.tm_year -= 1900; t.tm_mon -= 1; t.tm_hour = 12;
	mktime(&t);
	static const char *w[] = { "일", "월", "화", "수", "목", "금", "토" };
	return w[t.tm_wday];
}

static void show_draw(const draw &d)
{
	printf("\r\n  " L_YELLOW "제 %d 회" L_WHITE "   %s (%s) 추첨\r\n\r\n", d.no, d.date.c_str(), weekday_of(d.date).c_str());
	printf("     ");
	for ( int i = 0; i < 6; i++ ) printf("%s ", ball(d.nums[i]).c_str());
	printf("  +  %s " L_GRAY "보너스" L_WHITE "\r\n\r\n", ball(d.bonus).c_str());

	static const char *rule[] = { "6개 일치", "5개 + 보너스", "5개 일치", "4개 일치", "3개 일치" };
	printf("  " L_GRAY "%-4s  %-14s %14s %20s" L_WHITE "\r\n", "등수", "맞힌 수", "당첨자", "1명당 당첨금");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %s%d등" L_WHITE "   %-14s %12s명 %18s원\r\n", i == 0 ? L_YELLOW : "", i + 1, rule[i],
				d.winners[i] >= 0 ? comma(d.winners[i]).c_str() : "-", d.prize[i] >= 0 ? comma(d.prize[i]).c_str() : "-");
	}
	if ( d.sales > 0 ) printf("  " L_GRAY "총 판매액 %s원" L_WHITE "\r\n", comma(d.sales).c_str());
}

// 번호 추천 (재미로) : 서로 다른 6 개를 고르고 차례대로
static void pick(int out[6])
{
	int pool[45];
	for ( int i = 0; i < 45; i++ ) pool[i] = i + 1;
	for ( int i = 0; i < 6; i++ ) {
		int k = i + rand() % (45 - i);
		std::swap(pool[i], pool[k]);
		out[i] = pool[i];
	}
	std::sort(out, out + 6);
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
	srand(time(NULL) ^ getpid());

	print_header(title);
	printf("자료를 받아오는 중입니다...");
	fflush(stdout);

	draw d;
	if ( !get_draw(0, d) ) {
		printf("\r\n\r\n로또 당첨 정보를 받아오지 못했습니다.\r\n");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		host_close();
	}
	int latest = d.no;
	std::vector<std::string> result;

	while ( 1 ) {
		print_header(title);
		show_draw(d);
		printf("  " L_GRAY "자료: smok95.github.io/lotto (동행복권 발표를 옮긴 공개 자료, 참고용)" L_WHITE "\r\n");
		for ( unsigned int i = 0; i < result.size(); i++ ) printf("\r\n%s", result[i].c_str());
		if ( !result.empty() ) printf("\r\n");

		char cmd[64];
		printf(ESC_ENG);
		printf("\r\n회차번호  이전(B) 다음(N) 최근(L)  번호추천(R)  맞춰보기(M 1 2 3 4 5 6)\r\n");
		printf("상위메뉴(P) 종료(X) >> ");
		line_input(cmd, 40);
		result.clear();

		std::vector<std::string> args = split_string(trim(cmd), ' ');
		if ( args.size() == 0 ) continue;
		const char *c = args[0].c_str();
		if ( !strcasecmp(c, "p") || !strcasecmp(c, "x") || !strcasecmp(c, "q") ) break;

		int want = 0;
		if ( !strcasecmp(c, "b") ) want = d.no - 1;
		else if ( !strcasecmp(c, "n") ) want = d.no + 1;
		else if ( !strcasecmp(c, "l") ) want = latest;
		else if ( is_number((char*)c) ) want = atoi(c);

		if ( want != 0 ) {
			if ( want < 1 || want > latest ) {
				result.push_back("  " L_RED "1 ~ " + std::string(comma(latest)) + " 회 사이로 입력하세요." L_WHITE);
				continue;
			}
			draw nd;
			if ( get_draw(want, nd) ) d = nd;
			else result.push_back("  " L_RED "그 회차 정보를 받아오지 못했습니다." L_WHITE);
		} else if ( !strcasecmp(c, "r") ) {
			// 재미로 다섯 게임 (한 줄에 두 게임씩, 24 줄 화면에 들어가게)
			result.push_back("  " L_GRAY "추천 번호 (재미로)" L_WHITE);
			std::string s;
			for ( int g = 0; g < 5; g++ ) {
				int n[6];
				pick(n);
				s += (g % 2 == 0) ? "  " : "    ";
				s += std::string(1, 'A' + g) + "  ";
				for ( int i = 0; i < 6; i++ ) s += ball(n[i]) + " ";
				if ( g % 2 == 1 || g == 4 ) {
					result.push_back(s);
					s.clear();
				}
			}
		} else if ( !strcasecmp(c, "m") ) {
			// 내 번호 맞춰 보기 (지금 보는 회차)
			std::vector<int> mine;
			for ( unsigned int i = 1; i < args.size(); i++ ) {
				int n = atoi(args[i].c_str());
				if ( n >= 1 && n <= 45 && std::find(mine.begin(), mine.end(), n) == mine.end() ) mine.push_back(n);
			}
			if ( mine.size() != 6 ) {
				result.push_back("  " L_RED "서로 다른 1~45 숫자 여섯 개를 넣으세요. 예) M 3 11 19 27 35 42" L_WHITE);
				continue;
			}
			std::sort(mine.begin(), mine.end());
			int hit = 0;
			bool bonus = false;
			std::string s = "  내 번호  ";
			for ( int i = 0; i < 6; i++ ) {
				bool h = std::find(d.nums, d.nums + 6, mine[i]) != d.nums + 6;
				if ( h ) hit++;
				if ( mine[i] == d.bonus ) bonus = true;
				char b[16];
				snprintf(b, sizeof(b), "%2d", mine[i]);
				s += h ? ball(mine[i]) + " " : std::string(" ") + b + "  ";
			}
			result.push_back(s);
			int rank = hit == 6 ? 1 : (hit == 5 && bonus) ? 2 : hit == 5 ? 3 : hit == 4 ? 4 : hit == 3 ? 5 : 0;
			char r[128];
			if ( rank ) snprintf(r, sizeof(r), "  " L_YELLOW "%d개 맞음 - %d등!" L_WHITE, hit, rank);
			else snprintf(r, sizeof(r), "  %d개 맞음 - 아쉽게도 낙첨입니다.", hit);
			result.push_back(r);
		}
	}

	host_close();
	return 0;
}
