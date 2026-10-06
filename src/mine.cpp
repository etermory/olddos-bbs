#include "main.h"
#include <sys/time.h>

// ------------------------------------------------------------------
// 지뢰찾기
//   bin/mine <호스트이름> <아이디> <tty>
//   방향키(또는 H J K L)로 움직이고 Space/Enter 로 열기, F 로 깃발.
//   숫자 칸에서 Space 를 누르면 둘레 깃발 수가 맞을 때 나머지를 한꺼번에 연다.
//   첫 칸은 늘 안전하다.  초급 9x9 (10), 중급 16x16 (40), 고급 16x30 (99)
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;

#define M_WHITE		"\033[=15F"
#define M_YELLOW	"\033[=14F"
#define M_RED		"\033[=12F"
#define M_GREEN		"\033[=10F"
#define M_CYAN		"\033[=11F"
#define M_GRAY		"\033[=7F"
#define M_BLUE		"\033[=9F"
#define M_MAGENTA	"\033[=13F"
#define M_BROWN		"\033[=6F"
#define M_DGREEN	"\033[=2F"

#define MAXW	30
#define MAXH	16

struct level_t { const char *name; int w, h, mines; };
static const level_t levels[] = {
	{ "초급", 9, 9, 10 },
	{ "중급", 16, 16, 40 },
	{ "고급", 30, 16, 99 },
};

static int W, H, MINES, LEVEL;
static bool mine[MAXH][MAXW];
static int near_n[MAXH][MAXW];
static int state[MAXH][MAXW];		// 0 닫힘, 1 열림, 2 깃발
static int cx, cy;
static bool placed;				// 지뢰를 깔았는지 (첫 칸을 연 뒤)
static int opened, flags;
static long start_ms;
static int top_row, left_col;

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
	printf("\033[0m" M_WHITE);
	fflush(stdout);
	database::close();
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

static void at(int row, int col)
{
	printf("\033[%d;%dH", row, col);
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
	printf("\r\n " M_GRAY "[Enter] 를 누르세요." M_WHITE);
	press_enter();
}

// 키 하나를 timeout_ms 동안 기다린다. 없으면 -1
static int read_byte(int timeout_ms)
{
	fflush(stdout);
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	struct timeval tv;
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	if ( select(1, &fds, NULL, NULL, &tv) <= 0 ) return -1;
	unsigned char c;
	if ( read(0, &c, 1) != 1 ) host_close();
	return c;
}

// ------------------------------------------------------------------
// DB
// ------------------------------------------------------------------
static void create_tables(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS game_mine ( "
			"USER_ID VARCHAR(50) NOT NULL, "
			"LEVEL INT NOT NULL, "
			"NAME VARCHAR(50) NOT NULL, "
			"BEST_MS INT NOT NULL, "			/* 0 이면 아직 깨지 못함 */
			"PLAYS INT NOT NULL, "
			"WINS INT NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"PRIMARY KEY (USER_ID, LEVEL) )");
}

// 이긴 판이면 몇 등인지 돌려준다
static int save_score(bool won, long ms)
{
	char q[1024];
	std::string id = database::escape(user_id.c_str());
	std::string name = database::escape(user_nick.c_str());
	long best = won ? ms : 0;
	snprintf(q, sizeof(q), "INSERT INTO game_mine (USER_ID, LEVEL, NAME, BEST_MS, PLAYS, WINS, DATE_TIME) "
			"VALUES ('%s', %d, '%s', %ld, 1, %d, NOW()) ON DUPLICATE KEY UPDATE NAME='%s', PLAYS=PLAYS+1, WINS=WINS+%d, "
			"DATE_TIME=IF(%ld > 0 AND (BEST_MS = 0 OR %ld < BEST_MS), NOW(), DATE_TIME), "
			"BEST_MS=IF(%ld > 0 AND (BEST_MS = 0 OR %ld < BEST_MS), %ld, BEST_MS)",
			id.c_str(), LEVEL, name.c_str(), best, won ? 1 : 0, name.c_str(), won ? 1 : 0,
			best, best, best, best, best);
	mysql_query(mysql, q);
	if ( !won ) return 0;
	bool ok;
	snprintf(q, sizeof(q), "SELECT COUNT(*) FROM game_mine WHERE LEVEL=%d AND BEST_MS > 0 AND BEST_MS < %ld", LEVEL, ms);
	return atoi(database::fetch(q, &ok).c_str()) + 1;
}

static void show_rank(void)
{
	print_header(M_CYAN "지뢰찾기 명예의 전당" M_WHITE);
	for ( int lv = 0; lv < 3; lv++ ) {
		char q[256];
		snprintf(q, sizeof(q), "SELECT * FROM game_mine WHERE LEVEL=%d AND BEST_MS > 0 ORDER BY BEST_MS LIMIT 5", lv);
		std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(q);
		printf("  " M_YELLOW "◆ %s" M_GRAY " (%dx%d, 지뢰 %d)" M_WHITE "\r\n", levels[lv].name, levels[lv].w, levels[lv].h, levels[lv].mines);
		if ( rows.size() == 0 ) printf("  " M_GRAY "    아직 깬 사람이 없습니다." M_WHITE "\r\n");
		for ( unsigned int i = 0; i < rows.size(); i++ ) {
			bool me = rows[i]["USER_ID"] == user_id;
			std::string d = rows[i]["DATE_TIME"].size() >= 10 ? rows[i]["DATE_TIME"].substr(0, 10) : "";
			long ms = atol(rows[i]["BEST_MS"].c_str());
			char t[16], wp[24];
			snprintf(t, sizeof(t), "%ld.%ld초", ms / 1000, (ms % 1000) / 100);
			snprintf(wp, sizeof(wp), "%s/%s", rows[i]["WINS"].c_str(), rows[i]["PLAYS"].c_str());
			printf("  %s%4d  %-16s %9s  %9s판  %s" M_WHITE "\r\n", me ? M_YELLOW : M_WHITE, i + 1,
					string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(), t, wp, d.c_str());
		}
	}
	wait_enter();
}

// ------------------------------------------------------------------
// 판
// ------------------------------------------------------------------
static bool inside(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

static void place_mines(int sx, int sy)
{
	int n = 0;
	while ( n < MINES ) {
		int x = rand() % W, y = rand() % H;
		if ( mine[y][x] ) continue;
		if ( abs(x - sx) <= 1 && abs(y - sy) <= 1 ) continue;	// 첫 칸과 그 둘레는 비운다
		mine[y][x] = true;
		n++;
	}
	for ( int y = 0; y < H; y++ ) for ( int x = 0; x < W; x++ ) {
		int k = 0;
		for ( int dy = -1; dy <= 1; dy++ ) for ( int dx = -1; dx <= 1; dx++ )
			if ( (dx || dy) && inside(x + dx, y + dy) && mine[y + dy][x + dx] ) k++;
		near_n[y][x] = k;
	}
	placed = true;
	start_ms = now_ms();
}

// 칸 하나 그리기 (두 칸 너비). reveal_all: 졌을 때 지뢰를 보인다
static void draw_cell(int x, int y, bool reveal_all)
{
	static const char *num_color[] = { "", M_CYAN, M_GREEN, M_RED, M_BLUE, M_BROWN, M_MAGENTA, M_YELLOW, M_GRAY };
	at(top_row + y, left_col + x * 2);
	bool cur = (x == cx && y == cy);
	if ( cur ) printf("\033[7m");
	if ( state[y][x] == 2 ) {
		if ( reveal_all && !mine[y][x] ) printf(M_RED "×");
		else printf(M_RED "▶");
	} else if ( state[y][x] == 0 ) {
		if ( reveal_all && mine[y][x] ) printf(M_RED "◆");
		else printf(M_GRAY "□");
	} else if ( mine[y][x] ) {
		printf(M_RED "※");				// 밟은 지뢰
	} else if ( near_n[y][x] == 0 ) {
		printf(M_WHITE "  ");
	} else {
		printf("%s %d", num_color[near_n[y][x]], near_n[y][x]);
	}
	if ( cur ) printf("\033[0m");
	printf(M_WHITE);
}

static void draw_status(const char *msg)
{
	long sec = placed ? (now_ms() - start_ms) / 1000 : 0;
	at(top_row + H + 1, 3);
	printf("\033[K" M_RED "지뢰 %3d" M_WHITE "   " M_YELLOW "시간 %3ld초" M_WHITE "   %s", MINES - flags, sec, msg);
}

static void draw_board(bool reveal_all)
{
	// 테두리
	at(top_row - 1, left_col - 2);
	printf(M_GRAY "┌%s┐" M_WHITE, repeat("─", W).c_str());
	for ( int y = 0; y < H; y++ ) {
		at(top_row + y, left_col - 2); printf(M_GRAY "│" M_WHITE);
		for ( int x = 0; x < W; x++ ) draw_cell(x, y, reveal_all);
		at(top_row + y, left_col + W * 2); printf(M_GRAY "│" M_WHITE);
	}
	at(top_row + H, left_col - 2);
	printf(M_GRAY "└%s┘" M_WHITE, repeat("─", W).c_str());
}

static void park(void)
{
	at(top_row + cy, left_col + cx * 2);
}

// 칸 열기 (0 이면 둘레로 퍼진다). 지뢰를 밟으면 false
static bool open_cell(int x, int y)
{
	if ( !inside(x, y) || state[y][x] != 0 ) return true;
	std::vector<std::pair<int, int> > stack;
	stack.push_back(std::make_pair(x, y));
	while ( !stack.empty() ) {
		int px = stack.back().first, py = stack.back().second;
		stack.pop_back();
		if ( state[py][px] != 0 ) continue;
		state[py][px] = 1;
		if ( mine[py][px] ) return false;
		opened++;
		draw_cell(px, py, false);
		if ( near_n[py][px] == 0 ) {
			for ( int dy = -1; dy <= 1; dy++ ) for ( int dx = -1; dx <= 1; dx++ )
				if ( (dx || dy) && inside(px + dx, py + dy) && state[py + dy][px + dx] == 0 )
					stack.push_back(std::make_pair(px + dx, py + dy));
		}
	}
	return true;
}

// 열린 숫자 칸: 둘레 깃발 수가 숫자와 같으면 나머지를 연다
static bool chord(int x, int y)
{
	int f = 0;
	for ( int dy = -1; dy <= 1; dy++ ) for ( int dx = -1; dx <= 1; dx++ )
		if ( (dx || dy) && inside(x + dx, y + dy) && state[y + dy][x + dx] == 2 ) f++;
	if ( f != near_n[y][x] ) return true;
	bool ok = true;
	for ( int dy = -1; dy <= 1; dy++ ) for ( int dx = -1; dx <= 1; dx++ )
		if ( (dx || dy) && inside(x + dx, y + dy) && state[y + dy][x + dx] == 0 )
			if ( !open_cell(x + dx, y + dy) ) ok = false;
	return ok;
}

// 한 판. 끝나면 true 면 이김
static void play(int lv)
{
	LEVEL = lv;
	W = levels[lv].w; H = levels[lv].h; MINES = levels[lv].mines;
	memset(mine, 0, sizeof(mine));
	memset(state, 0, sizeof(state));
	memset(near_n, 0, sizeof(near_n));
	placed = false;
	opened = flags = 0;
	cx = W / 2; cy = H / 2;
	top_row = 5;
	left_col = (80 - W * 2) / 2 + 1;

	char title[64];
	snprintf(title, sizeof(title), M_CYAN "지뢰찾기" M_WHITE " - %s (%dx%d, 지뢰 %d)", levels[lv].name, W, H, MINES);
	print_header(title);
	draw_board(false);
	at(top_row + H + 2, 3);
	printf(M_GRAY "방향키 움직이기  Space 열기  F 깃발  숫자에서 Space: 둘레 열기  Q 그만" M_WHITE);
	draw_status("");
	park();

	int esc = 0;
	long last_sec = -1;
	bool lost = false, won = false;
	while ( !lost && !won ) {
		int c = read_byte(500);
		if ( c < 0 ) {
			long sec = placed ? (now_ms() - start_ms) / 1000 : 0;
			if ( sec != last_sec ) { last_sec = sec; draw_status(""); park(); }
			continue;
		}
		int ox = cx, oy = cy;
		if ( esc == 1 ) { esc = (c == '[' || c == 'O') ? 2 : 0; continue; }
		if ( esc == 2 ) {
			esc = 0;
			if ( c == 'A' ) cy--; else if ( c == 'B' ) cy++; else if ( c == 'C' ) cx++; else if ( c == 'D' ) cx--;
		} else if ( c == 0x1b ) {
			esc = 1;
			continue;
		} else {
			int lc = tolower(c);
			if ( lc == 'k' || lc == 'w' ) cy--;
			else if ( lc == 'j' || lc == 's' ) cy++;
			else if ( lc == 'l' || lc == 'd' ) cx++;
			else if ( lc == 'h' || lc == 'a' ) cx--;
			else if ( lc == 'q' ) {
				at(top_row + H + 3, 3);
				printf("\033[K그만둘까요? (y/N) ");
				if ( yesno(NO) == YES ) {
					if ( placed ) save_score(false, 0);		// 시작한 판은 진 판으로
					return;
				}
				at(top_row + H + 3, 3); printf("\033[K");
			} else if ( lc == 'f' || c == '/' ) {
				if ( state[cy][cx] == 0 ) { state[cy][cx] = 2; flags++; }
				else if ( state[cy][cx] == 2 ) { state[cy][cx] = 0; flags--; }
				draw_cell(cx, cy, false);
				draw_status("");
			} else if ( c == ' ' || c == '\r' ) {
				if ( !placed ) place_mines(cx, cy);
				if ( state[cy][cx] == 0 ) lost = !open_cell(cx, cy);
				else if ( state[cy][cx] == 1 && near_n[cy][cx] > 0 ) lost = !chord(cx, cy);
				draw_cell(cx, cy, false);
				if ( !lost && opened == W * H - MINES ) won = true;
				draw_status("");
			}
		}
		if ( cx < 0 ) cx = 0; if ( cx >= W ) cx = W - 1;
		if ( cy < 0 ) cy = 0; if ( cy >= H ) cy = H - 1;
		if ( ox != cx || oy != cy ) {
			int nx = cx, ny = cy;
			cx = ox; cy = oy;		// 옛 자리를 반전 없이 다시
			cx = -1;
			draw_cell(ox, oy, false);
			cx = nx; cy = ny;
			draw_cell(cx, cy, false);
		}
		park();
	}

	long ms = now_ms() - start_ms;
	cx = -1;
	draw_board(true);
	at(top_row + H + 2, 3);
	printf("\033[K");
	if ( won ) {
		int rank = save_score(true, ms);
		printf(M_YELLOW "★ 지뢰를 모두 찾았습니다! %ld.%ld 초 (%s %d 등)" M_WHITE, ms / 1000, (ms % 1000) / 100, levels[lv].name, rank);
	} else {
		save_score(false, 0);
		printf(M_RED "펑! 지뢰를 밟았습니다." M_WHITE);
	}
	wait_enter();
}

static void title(void)
{
	while ( 1 ) {
		print_header(M_CYAN "지뢰찾기" M_WHITE);
		printf("\r\n");
		printf("        " M_GRAY "□" M_WHITE " 칸을 열어 지뢰를 피하세요. 숫자는 둘레 여덟 칸의 지뢰 수입니다.\r\n");
		printf("        지뢰라고 생각되는 칸에는 F 로 " M_RED "▶" M_WHITE " 깃발을 꽂으세요.\r\n");
		printf("        " M_GRAY "첫 칸은 늘 안전합니다." M_WHITE "\r\n\r\n");
		for ( int i = 0; i < 3; i++ ) {
			printf("        %d. %s  " M_GRAY "(%dx%d, 지뢰 %d)" M_WHITE "\r\n", i + 1, levels[i].name, levels[i].w, levels[i].h, levels[i].mines);
		}
		printf("        4. 명예의 전당\r\n\r\n");

		char cmd[16];
		printf(ESC_ENG);
		printf("  선택 (끝내기: Q) >> ");
		line_input(cmd, 3);
		std::string c = trim(cmd);
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		int n = atoi(c.c_str());
		if ( n >= 1 && n <= 3 ) play(n - 1);
		else if ( n == 4 ) show_rank();
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
