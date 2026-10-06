#include "main.h"
#include <sys/time.h>

// ------------------------------------------------------------------
// 숫자야구 : 컴퓨터가 고른 서로 다른 숫자를 맞힌다
//   bin/baseball <호스트이름> <아이디> <tty>
//   자리와 숫자가 맞으면 스트라이크, 숫자만 맞으면 볼, 하나도 없으면 아웃.
//   1) 세 자리 (10 번 안에)  2) 네 자리 (12 번 안에)
//   3) 오늘의 문제: 하루 동안 모두 같은 네 자리 숫자, 한 번만 도전, 오늘 순위
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;

#define B_WHITE		"\033[=15F"
#define B_YELLOW	"\033[=14F"
#define B_RED		"\033[=12F"
#define B_GREEN		"\033[=10F"
#define B_CYAN		"\033[=11F"
#define B_GRAY		"\033[=7F"
#define B_MAGENTA	"\033[=13F"

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

int host_close(void)
{
	printf(B_WHITE);
	fflush(stdout);
	database::close();
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

static void print_header(const char *head_title)
{
	printf(ESC_CLEAR);
    printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	int center = (80 - strlen(strip_ansi_codes(head_title))) / 2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, head_title);
    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
    printf("\033[4;1H");
}

static long now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

static void wait_enter(void)
{
	printf("\r\n " B_GRAY "[Enter] 를 누르세요." B_WHITE);
	press_enter();
}

// ------------------------------------------------------------------
// DB
// ------------------------------------------------------------------
static void create_tables(void)
{
	// MODE: 3 세 자리, 4 네 자리
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_baseball ( "
			"USER_ID VARCHAR(50) NOT NULL, "
			"MODE INT NOT NULL, "
			"NAME VARCHAR(50) NOT NULL, "
			"BEST INT NOT NULL, "				/* 가장 적게 맞힌 횟수 */
			"BEST_MS INT NOT NULL, "
			"PLAYS INT NOT NULL, "
			"WINS INT NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"PRIMARY KEY (USER_ID, MODE) )");
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_baseball_daily ( "
			"DAY DATE NOT NULL, "
			"USER_ID VARCHAR(50) NOT NULL, "
			"NAME VARCHAR(50) NOT NULL, "
			"TRIES INT NOT NULL, "
			"MS INT NOT NULL, "
			"SOLVED INT NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"PRIMARY KEY (DAY, USER_ID) )");
}

static void save_score(int mode, bool won, int tries, long ms)
{
	char q[1024];
	std::string id = database::escape(user_id.c_str());
	std::string name = database::escape(user_nick.c_str());
	int best = won ? tries : 99;
	// 적게 맞힐수록, 같으면 빨리 맞힐수록 좋은 기록
	snprintf(q, sizeof(q), "INSERT INTO game_baseball (USER_ID, MODE, NAME, BEST, BEST_MS, PLAYS, WINS, DATE_TIME) "
			"VALUES ('%s', %d, '%s', %d, %ld, 1, %d, NOW()) ON DUPLICATE KEY UPDATE NAME='%s', PLAYS=PLAYS+1, WINS=WINS+%d, "
			"DATE_TIME=IF(%d < BEST OR (%d = BEST AND %ld < BEST_MS), NOW(), DATE_TIME), "
			"BEST_MS=IF(%d < BEST OR (%d = BEST AND %ld < BEST_MS), %ld, BEST_MS), "
			"BEST=LEAST(BEST, %d)",
			id.c_str(), mode, name.c_str(), best, ms, won ? 1 : 0, name.c_str(), won ? 1 : 0,
			best, best, ms, best, best, ms, ms, best);
	mysql_query(mysql, q);
}

// ------------------------------------------------------------------
// 게임
// ------------------------------------------------------------------
static std::string make_secret(int n, unsigned long seed, bool seeded)
{
	std::string digits = "0123456789", s;
	unsigned long r = seed;
	for ( int i = 0; i < n; i++ ) {
		int k;
		if ( seeded ) {
			r = r * 1103515245UL + 12345UL;
			k = (int)((r >> 8) % digits.size());
		} else {
			k = rand() % digits.size();
		}
		s += digits[k];
		digits.erase(k, 1);
	}
	return s;
}

static std::string today(void)
{
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	char b[16];
	snprintf(b, sizeof(b), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	return b;
}

static unsigned long today_seed(void)
{
	std::string d = today();
	unsigned long h = 5381;
	for ( unsigned int i = 0; i < d.size(); i++ ) h = h * 33 + (unsigned char)d[i];
	return h ^ 0x9e3779b9UL;
}

struct guess_t { std::string g; int s, b; };

static void judge(const std::string &secret, const std::string &g, int *s, int *b)
{
	*s = *b = 0;
	for ( unsigned int i = 0; i < g.size(); i++ ) {
		if ( g[i] == secret[i] ) (*s)++;
		else if ( secret.find(g[i]) != std::string::npos ) (*b)++;
	}
}

static std::string spaced(const std::string &g)
{
	std::string r;
	for ( unsigned int i = 0; i < g.size(); i++ ) { if ( i ) r += ' '; r += g[i]; }
	return r;
}

// 한 판. mode 3/4, daily 면 오늘의 문제. 맞혔으면 true
static bool play(int n, int limit, bool daily, int *tries_out, long *ms_out)
{
	std::string secret = daily ? make_secret(n, today_seed(), true) : make_secret(n, 0, false);
	std::vector<guess_t> history;
	std::string message;
	long start = now_ms();
	bool won = false;

	while ( 1 ) {
		char title[128];
		snprintf(title, sizeof(title), B_CYAN "숫자야구" B_WHITE " - %s", daily ? "오늘의 문제 (네 자리)" : (n == 3 ? "세 자리" : "네 자리"));
		print_header(title);
		printf("\r\n  " B_GRAY "서로 다른 숫자 %d 개를 맞히세요. (기회 %d 번, 0 도 쓸 수 있음)" B_WHITE "\r\n", n, limit);
		printf("  " B_GRAY "자리까지 맞으면 " B_YELLOW "S" B_GRAY "(스트라이크), 숫자만 맞으면 " B_GREEN "B" B_GRAY
				"(볼), 하나도 없으면 " B_RED "아웃" B_WHITE "\r\n\r\n");

		// 기록: 두 단 (한 단에 8 줄)
		int rows = 8;
		for ( int r = 0; r < rows; r++ ) {
			for ( int col = 0; col < 2; col++ ) {
				unsigned int k = col * rows + r;
				if ( k >= history.size() ) { printf("%38s", ""); continue; }
				const guess_t &h = history[k];
				std::string res;
				char buf[64];
				if ( h.s == 0 && h.b == 0 ) snprintf(buf, sizeof(buf), B_RED "아웃 " B_WHITE);
				else snprintf(buf, sizeof(buf), B_YELLOW "%dS" B_WHITE " " B_GREEN "%dB" B_WHITE, h.s, h.b);
				printf("  %2d.  " B_CYAN "%-8s" B_WHITE "  →  %s", k + 1, spaced(h.g).c_str(), buf);
				printf("%*s", 12, "");
			}
			printf("\r\n");
		}
		printf("\r\n");
		if ( !message.empty() ) printf("  %s\r\n", message.c_str());
		message.clear();

		if ( won || (int)history.size() >= limit ) {
			long ms = now_ms() - start;
			*tries_out = history.size();
			*ms_out = ms;
			if ( won ) {
				printf("  " B_YELLOW "★ 홈런! %d 번 만에 맞혔습니다. (%ld.%ld 초)" B_WHITE "\r\n", (int)history.size(), ms / 1000, (ms % 1000) / 100);
			} else {
				printf("  " B_RED "삼진 아웃! 답은 %s 였습니다." B_WHITE "\r\n", spaced(secret).c_str());
			}
			return won;
		}

		char cmd[32];
		printf(ESC_ENG);
		printf("  %d 번째 (숫자 %d 개, 그만: Q) >> ", (int)history.size() + 1, n);
		line_input(cmd, 10);
		std::string c;
		for ( char *p = cmd; *p; p++ ) if ( *p != ' ' ) c += *p;
		if ( !strcasecmp(c.c_str(), "q") ) {
			if ( daily ) {
				// 오늘의 문제는 그만두면 실패로 남긴다 (다시 도전 못 함)
				*tries_out = history.size();
				*ms_out = now_ms() - start;
				printf("\r\n  " B_RED "오늘의 문제를 그만두었습니다. 답은 %s 였습니다." B_WHITE "\r\n", spaced(secret).c_str());
				return false;
			}
			*tries_out = -1;
			return false;
		}
		if ( (int)c.size() != n || !is_number(c) ) {
			message = std::string(B_RED) + "숫자 " + TO_STRING(n) + " 개를 넣으세요. (예: " + (n == 3 ? "1 2 3" : "1 2 3 4") + ")" + B_WHITE;
			continue;
		}
		bool dup = false;
		for ( int i = 0; i < n; i++ ) for ( int j = i + 1; j < n; j++ ) if ( c[i] == c[j] ) dup = true;
		if ( dup ) { message = B_RED "서로 다른 숫자여야 합니다." B_WHITE; continue; }

		guess_t h;
		h.g = c;
		judge(secret, c, &h.s, &h.b);
		history.push_back(h);
		if ( h.s == n ) won = true;
	}
}

static void show_rank(void)
{
	print_header(B_CYAN "숫자야구 명예의 전당" B_WHITE);
	for ( int mode = 3; mode <= 4; mode++ ) {
		char q[256];
		snprintf(q, sizeof(q), "SELECT * FROM game_baseball WHERE MODE=%d AND BEST < 99 ORDER BY BEST, BEST_MS LIMIT 6", mode);
		std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(q);
		printf("\r\n  " B_YELLOW "%s" B_WHITE "\r\n", mode == 3 ? "◆ 세 자리" : "◆ 네 자리");
		printf("  " B_GRAY "%4s  %-16s %6s %8s %10s  %s" B_WHITE "\r\n", "순위", "이름", "횟수", "시간", "맞힘/판", "날짜");
		if ( rows.size() == 0 ) printf("  " B_GRAY "    아직 기록이 없습니다." B_WHITE "\r\n");
		for ( unsigned int i = 0; i < rows.size(); i++ ) {
			bool me = rows[i]["USER_ID"] == user_id;
			std::string d = rows[i]["DATE_TIME"].size() >= 10 ? rows[i]["DATE_TIME"].substr(0, 10) : "";
			long ms = atol(rows[i]["BEST_MS"].c_str());
			char t[16], wp[24];
			snprintf(t, sizeof(t), "%ld.%ld초", ms / 1000, (ms % 1000) / 100);
			snprintf(wp, sizeof(wp), "%s/%s", rows[i]["WINS"].c_str(), rows[i]["PLAYS"].c_str());
			printf("  %s%4d  %-16s %4s번 %8s %10s  %s" B_WHITE "\r\n", me ? B_YELLOW : B_WHITE, i + 1,
					string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(),
					rows[i]["BEST"].c_str(), t, wp, d.c_str());
		}
	}
	wait_enter();
}

static void show_daily_rank(void)
{
	print_header(B_CYAN "숫자야구 - 오늘의 문제 순위" B_WHITE);
	std::string q = "SELECT * FROM game_baseball_daily WHERE DAY='" + today() + "' ORDER BY SOLVED DESC, TRIES, MS LIMIT 15";
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)q.c_str());
	printf("\r\n  " B_GRAY "%s, 모두 같은 숫자에 한 번씩 도전합니다." B_WHITE "\r\n\r\n", today().c_str());
	printf("  " B_GRAY "%4s  %-16s %6s %8s  %s" B_WHITE "\r\n", "순위", "이름", "횟수", "시간", "결과");
	if ( rows.size() == 0 ) printf("  " B_GRAY "    아직 아무도 도전하지 않았습니다." B_WHITE "\r\n");
	for ( unsigned int i = 0; i < rows.size(); i++ ) {
		bool me = rows[i]["USER_ID"] == user_id;
		bool solved = atoi(rows[i]["SOLVED"].c_str()) != 0;
		long ms = atol(rows[i]["MS"].c_str());
		char t[16];
		snprintf(t, sizeof(t), "%ld.%ld초", ms / 1000, (ms % 1000) / 100);
		printf("  %s%4s  %-16s %4s번 %8s  %s" B_WHITE "\r\n", me ? B_YELLOW : B_WHITE,
				solved ? TO_STRING(i + 1).c_str() : "-",
				string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(),
				rows[i]["TRIES"].c_str(), t, solved ? "맞힘" : B_GRAY "실패");
	}
	wait_enter();
}

static void daily(void)
{
	bool ok;
	std::string q = "SELECT TRIES FROM game_baseball_daily WHERE DAY='" + today() + "' AND USER_ID='" +
		database::escape(user_id.c_str()) + "'";
	std::string done = database::fetch((char*)q.c_str(), &ok);
	if ( ok && !done.empty() ) {
		printf("\r\n  " B_YELLOW "오늘의 문제는 이미 도전했습니다. 내일 새 문제가 나옵니다." B_WHITE "\r\n");
		wait_enter();
		show_daily_rank();
		return;
	}
	int tries; long ms;
	bool won = play(4, 12, true, &tries, &ms);
	char ins[512];
	snprintf(ins, sizeof(ins), "INSERT IGNORE INTO game_baseball_daily (DAY, USER_ID, NAME, TRIES, MS, SOLVED, DATE_TIME) "
			"VALUES ('%s', '%s', '%s', %d, %ld, %d, NOW())", today().c_str(),
			database::escape(user_id.c_str()).c_str(), database::escape(user_nick.c_str()).c_str(), tries, ms, won ? 1 : 0);
	mysql_query(mysql, ins);
	wait_enter();
	show_daily_rank();
}

static void title(void)
{
	while ( 1 ) {
		print_header(B_CYAN "숫자야구" B_WHITE);
		printf("\r\n");
		printf("        " B_YELLOW "⊙" B_WHITE "  컴퓨터가 고른 서로 다른 숫자를 맞혀 보세요.\r\n\r\n");
		printf("        " B_GRAY "예) 답이 3 7 1 일 때 1 7 5 를 넣으면" B_WHITE "\r\n");
		printf("        " B_GRAY "    7 은 자리까지 맞아 " B_YELLOW "1S" B_GRAY ", 1 은 숫자만 맞아 " B_GREEN "1B" B_GRAY " → " B_YELLOW "1S" B_WHITE " " B_GREEN "1B" B_WHITE "\r\n\r\n");
		printf("        1. 세 자리 (기회 10 번)\r\n");
		printf("        2. 네 자리 (기회 12 번)\r\n");
		printf("        3. " B_MAGENTA "오늘의 문제" B_WHITE " (모두 같은 네 자리, 하루 한 번, 오늘 순위)\r\n");
		printf("        4. 명예의 전당\r\n");
		printf("        5. 오늘의 문제 순위\r\n\r\n");

		char cmd[16];
		printf(ESC_ENG);
		printf("  선택 (끝내기: Q) >> ");
		line_input(cmd, 3);
		std::string c = trim(cmd);
		if ( c.empty() ) continue;
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		int n = atoi(c.c_str());
		if ( n == 1 || n == 2 ) {
			int digits = n == 1 ? 3 : 4, tries;
			long ms;
			bool won = play(digits, n == 1 ? 10 : 12, false, &tries, &ms);
			if ( tries >= 0 ) {
				save_score(digits, won, tries, ms);
				wait_enter();
			}
		} else if ( n == 3 ) {
			daily();
		} else if ( n == 4 ) {
			show_rank();
		} else if ( n == 5 ) {
			show_daily_rank();
		}
	}
}

int main(int argc, char **argv)
{
	if ( argc < 3 ) {
		printf("usage: %s <host_name> <user_id> [tty]\n", argv[0]);
		return 1;
	}
	snprintf(host_name, sizeof(host_name), "%s", argv[1]);
	user_id = argv[2];
	snprintf(tty, sizeof(tty), "%s", argc > 3 ? argv[3] : "");

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);

	read_settings("hanulso.cfg");

    ioctl(0, TCGETA, &sys_term);
	raw_mode();

	srand(time(NULL) ^ getpid());

	if ( database::open() == false )
		exit(1);
	create_tables();

	bool exist;
	std::map<std::string, std::string> user = database::user_info((char*)user_id.c_str(), &exist);
	user_nick = display_text(user["NICK_NAME"]);
	if ( user_nick.empty() ) user_nick = user_id;

	title();

	host_close();
	return 0;
}
