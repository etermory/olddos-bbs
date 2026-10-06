#include "main.h"
#include <sys/time.h>

// ------------------------------------------------------------------
// 오목
//   bin/omok <호스트이름> <아이디> <tty>
// 사람끼리 (대국방을 만들고 다른 회원이 들어와서 둔다, 구경/대화/초대) 또는 컴퓨터와 둔다.
// 규칙: 15x15, 흑이 먼저. 정확히 다섯이면 이김 (여섯 이상은 아님).
//       쌍삼(열린 삼이 두 개 생기는 수)은 흑백 모두 금지. 한 수에 60 초.
// 사람끼리 둘 때는 두 사람의 프로그램이 DB 의 대국(omok_game)을 0.7 초마다 보고 맞춘다.
// ------------------------------------------------------------------

struct termio sys_term;

char tty[10];

static std::string user_id;
static std::string user_nick;

#define O_WHITE		"\033[=15F"
#define O_BLACK		"\033[=0F"
#define O_YELLOW	"\033[=14F"
#define O_RED		"\033[=12F"
#define O_GREEN		"\033[=10F"
#define O_CYAN		"\033[=11F"
#define O_MAGENTA	"\033[=13F"
#define O_GRAY		"\033[=7F"
#define O_BROWN		"\033[=6F"

#define N			15
#define MOVE_SEC	60		// 한 수 시간
#define GONE_SEC	30		// 이만큼 소식이 없으면 나간 것으로

#define BOARD_TOP	5		// 판의 맨 윗줄 (화면 줄)
#define BOARD_LEFT	5		// 판의 A 줄 (화면 칸)
#define PANEL		39		// 오른쪽 안내 칸
#define HELP_ROW	21
#define INPUT_ROW	22

enum { NONE = 0, STONE_B = 1, STONE_W = 2 };	// 빈 칸, 흑, 백

static int board[N][N];
static int last_x = -1, last_y = -1;
static std::string moves;		// 둔 수들: 한 수에 두 글자 ('a'+x, 'a'+y)
static int cur_x = 7, cur_y = 7;	// 커서

// 지금 하는 대국 (사람끼리). host_close 에서 기다리던 방을 닫으려고
static int current_game = 0;
static bool waiting_room = false;

// ------------------------------------------------------------------
// 화면 / 입력
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
}

int host_close(void)
{
	// 기다리던 방은 닫는다 (두던 판은 상대가 30 초 뒤에 이긴 것으로 처리)
	if ( current_game > 0 && waiting_room ) {
		char q[256];
		snprintf(q, sizeof(q), "UPDATE omok_game SET STATUS=2, WINNER=0, REASON='방을 닫음' WHERE NO=%d AND STATUS=0", current_game);
		mysql_query(mysql, q);
	}
	printf(O_WHITE);
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

static std::string ask(const char *msg)
{
	char buf[64];
	printf(ESC_ENG);
	printf("%s", msg);
	line_input(buf, 20);
	return trim(buf);
}

static void wait_enter(void)
{
	printf("\r\n " O_GRAY "[Enter] 를 누르세요." O_WHITE);
	press_enter();
}

// 남아 있는 입력을 버린다
static void flush_input(void)
{
	fd_set fds;
	struct timeval tv;
	char buf[256];
	while ( 1 ) {
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		if ( select(1, &fds, NULL, NULL, &tv) <= 0 ) break;
		if ( read(0, buf, sizeof(buf)) <= 0 ) break;
	}
}

static std::string nick_of(const std::string &id)
{
	if ( id.empty() ) return "";
	bool exist;
	std::map<std::string, std::string> u = database::user_info((char*)id.c_str(), &exist);
	std::string n = display_text(u["NICK_NAME"]);
	return n.empty() ? display_text(id) : n;
}

// ------------------------------------------------------------------
// DB
// ------------------------------------------------------------------
static void create_tables(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS omok_game ( "
			"NO INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
			"BLACK_ID VARCHAR(50) NOT NULL, "
			"WHITE_ID VARCHAR(50) NOT NULL DEFAULT '', "
			"STATUS INT NOT NULL DEFAULT 0, "			// 0 기다림, 1 두는 중, 2 끝
			"MOVES VARCHAR(500) NOT NULL DEFAULT '', "
			"WINNER INT NOT NULL DEFAULT 0, "			// 1 흑, 2 백, 3 비김
			"REASON VARCHAR(50) NOT NULL DEFAULT '', "
			"CREATED DATETIME NOT NULL, "
			"UPDATED DATETIME NOT NULL, "				// 마지막 수 (시간 재기)
			"SEEN_B DATETIME NOT NULL, "
			"SEEN_W DATETIME NOT NULL, "
			"KEY IDX_STATUS (STATUS) )");
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS omok_chat ( "
			"NO INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
			"GAME_NO INT NOT NULL, "
			"USER_ID VARCHAR(50) NOT NULL, "
			"TEXT VARCHAR(255) NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"KEY IDX_GAME (GAME_NO) )");
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS omok_stat ( "
			"USER_ID VARCHAR(50) NOT NULL PRIMARY KEY, "
			"NAME VARCHAR(50) NOT NULL, "
			"WIN INT NOT NULL DEFAULT 0, LOSE INT NOT NULL DEFAULT 0, DRAW INT NOT NULL DEFAULT 0, "
			"AI_WIN INT NOT NULL DEFAULT 0, AI_LOSE INT NOT NULL DEFAULT 0 )");
	// 오래된 대국과 대화는 지운다
	mysql_query(mysql, "DELETE FROM omok_chat WHERE DATE_TIME < NOW() - INTERVAL 30 DAY");
	mysql_query(mysql, "DELETE FROM omok_game WHERE STATUS = 2 AND UPDATED < NOW() - INTERVAL 30 DAY");
	// 주인이 사라진 기다리는 방 / 둘 다 사라진 판은 끝낸다
	mysql_query(mysql, "UPDATE omok_game SET STATUS=2, REASON='방을 닫음' WHERE STATUS=0 AND SEEN_B < NOW() - INTERVAL 1 MINUTE");
	mysql_query(mysql, "UPDATE omok_game SET STATUS=2, REASON='둘 다 나감' WHERE STATUS=1 "
			"AND SEEN_B < NOW() - INTERVAL 5 MINUTE AND SEEN_W < NOW() - INTERVAL 5 MINUTE");
}

// 전적 (column: WIN/LOSE/DRAW/AI_WIN/AI_LOSE)
static void add_stat(const std::string &id, const char *column)
{
	if ( id.empty() ) return;
	std::string eid = database::escape(id.c_str());
	std::string name = database::escape(nick_of(id).c_str());
	std::string q = std::string("INSERT INTO omok_stat (USER_ID, NAME, ") + column + ") VALUES ('" + eid + "', '" + name +
		"', 1) ON DUPLICATE KEY UPDATE NAME='" + name + "', " + column + "=" + column + "+1";
	mysql_query(mysql, q.c_str());
}

// ------------------------------------------------------------------
// 판과 규칙
// ------------------------------------------------------------------
static bool inside(int x, int y) { return x >= 0 && x < N && y >= 0 && y < N; }

static int stone(int x, int y) { return inside(x, y) ? board[y][x] : -1; }

static void reset_board(void)
{
	memset(board, 0, sizeof(board));
	last_x = last_y = -1;
	moves.clear();
}

// 둔 수들(moves)로 판을 다시 놓는다
static void apply_moves(const std::string &m)
{
	reset_board();
	for ( unsigned int i = 0; i + 1 < m.size(); i += 2 ) {
		int x = m[i] - 'a', y = m[i + 1] - 'a';
		if ( !inside(x, y) ) continue;
		board[y][x] = (i / 2) % 2 == 0 ? STONE_B : STONE_W;
		last_x = x;
		last_y = y;
	}
	moves = m;
}

static int to_move(void)
{
	return (moves.size() / 2) % 2 == 0 ? STONE_B : STONE_W;
}

// (x,y) 에서 (dx,dy) 쪽으로 같은 색이 몇 개 이어지는지 ((x,y) 는 빼고)
static int run(int x, int y, int dx, int dy, int c)
{
	int n = 0;
	for ( x += dx, y += dy; stone(x, y) == c; x += dx, y += dy ) n++;
	return n;
}

static const int DX[4] = { 1, 0, 1, 1 };
static const int DY[4] = { 0, 1, 1, -1 };

// (x,y) 에 c 가 놓인 상태에서 정확히 다섯이 생겼는지
static bool five(int x, int y, int c)
{
	for ( int d = 0; d < 4; d++ ) {
		if ( run(x, y, DX[d], DY[d], c) + run(x, y, -DX[d], -DY[d], c) + 1 == 5 ) return true;
	}
	return false;
}

// (x,y) 를 포함해 (dx,dy) 줄에 '열린 넷' (양쪽 끝이 비어 있고, 어느 쪽을 채워도 정확히 다섯)
static bool open_four(int x, int y, int dx, int dy, int c)
{
	int a = run(x, y, -dx, -dy, c), b = run(x, y, dx, dy, c);
	if ( a + b + 1 != 4 ) return false;
	int x1 = x - (a + 1) * dx, y1 = y - (a + 1) * dy;
	int x2 = x + (b + 1) * dx, y2 = y + (b + 1) * dy;
	if ( stone(x1, y1) != NONE || stone(x2, y2) != NONE ) return false;
	return stone(x1 - dx, y1 - dy) != c && stone(x2 + dx, y2 + dy) != c;
}

// (x,y) 에 c 가 놓인 상태에서 (dx,dy) 줄이 '열린 삼'인지: 그 줄에 한 수 더 두면 열린 넷이 되는가
static bool open_three(int x, int y, int dx, int dy, int c)
{
	// 이미 넷 이상이면 삼이 아니다 (4-3 은 둘 수 있다)
	if ( run(x, y, -dx, -dy, c) + run(x, y, dx, dy, c) + 1 >= 4 ) return false;
	for ( int s = -4; s <= 4; s++ ) {
		if ( s == 0 ) continue;
		int ex = x + s * dx, ey = y + s * dy;
		if ( stone(ex, ey) != NONE ) continue;
		board[ey][ex] = c;
		// 더 둔 돌이 (x,y) 와 이어진 넷 안에 있어야 한다
		int a = run(x, y, -dx, -dy, c), b = run(x, y, dx, dy, c);
		bool ok = s >= -a && s <= b && open_four(x, y, dx, dy, c);
		board[ey][ex] = NONE;
		if ( ok ) return true;
	}
	return false;
}

// 쌍삼 금지 (다섯이 되는 수는 괜찮다)
static bool forbidden(int x, int y, int c)
{
	if ( stone(x, y) != NONE ) return false;
	board[y][x] = c;
	bool bad = false;
	if ( !five(x, y, c) ) {
		int threes = 0;
		for ( int d = 0; d < 4; d++ ) {
			if ( open_three(x, y, DX[d], DY[d], c) ) threes++;
		}
		bad = threes >= 2;
	}
	board[y][x] = NONE;
	return bad;
}

static std::string coord(int x, int y)
{
	char buf[8];
	snprintf(buf, sizeof(buf), "%c%d", 'A' + x, N - y);
	return buf;
}

// ------------------------------------------------------------------
// 컴퓨터
// ------------------------------------------------------------------
// 다섯 칸 창 방식: (x,y) 를 지나는 다섯 칸 창마다, 상대 돌이 없으면 내 돌 수에 따라 점수
static long window_score(int x, int y, int c, const long *w)
{
	long total = 0;
	for ( int d = 0; d < 4; d++ ) {
		for ( int k = 0; k < 5; k++ ) {
			int sx = x - k * DX[d], sy = y - k * DY[d];
			int own = 0;
			bool blocked = false;
			for ( int i = 0; i < 5; i++ ) {
				int s = stone(sx + i * DX[d], sy + i * DY[d]);
				if ( s == -1 || (s != NONE && s != c) ) { blocked = true; break; }
				if ( s == c ) own++;
			}
			if ( !blocked ) total += w[own];
		}
	}
	return total;
}

struct cand { long score; int x, y; };

// level 1 쉬움, 2 보통, 3 어려움
static bool ai_move(int c, int level, int *mx, int *my)
{
	static const long attack[] = { 1, 20, 400, 6000, 2000000 };
	static const long defend[] = { 1, 15, 300, 4500, 500000 };
	int opp = (c == STONE_B) ? STONE_W : STONE_B;

	if ( moves.empty() ) { *mx = 7; *my = 7; return true; }

	std::vector<cand> list;
	for ( int y = 0; y < N; y++ ) {
		for ( int x = 0; x < N; x++ ) {
			if ( board[y][x] != NONE ) continue;
			// 돌 가까이 (두 칸 안) 만 본다
			bool near = false;
			for ( int yy = y - 2; yy <= y + 2 && !near; yy++ )
				for ( int xx = x - 2; xx <= x + 2 && !near; xx++ )
					if ( stone(xx, yy) > 0 ) near = true;
			if ( !near || forbidden(x, y, c) ) continue;

			// 바로 이기는 수
			board[y][x] = c;
			bool win = five(x, y, c);
			board[y][x] = NONE;
			cand cd;
			cd.x = x; cd.y = y;
			if ( win ) { *mx = x; *my = y; return true; }
			long def = window_score(x, y, opp, defend);
			if ( level == 1 ) def /= 3;			// 쉬움: 잘 막지 않는다
			cd.score = window_score(x, y, c, attack) + def;
			list.push_back(cd);
		}
	}
	if ( list.empty() ) return false;

	// 점수 순
	for ( unsigned int i = 1; i < list.size(); i++ )
		for ( unsigned int k = i; k > 0 && list[k].score > list[k - 1].score; k-- ) std::swap(list[k], list[k - 1]);

	// 상대가 바로 이기는 자리는 반드시 막는다 (쉬움도)
	for ( unsigned int i = 0; i < list.size(); i++ ) {
		board[list[i].y][list[i].x] = opp;
		bool lose = five(list[i].x, list[i].y, opp);
		board[list[i].y][list[i].x] = NONE;
		if ( lose ) { *mx = list[i].x; *my = list[i].y; return true; }
	}

	int pick = 0;
	if ( level == 1 ) pick = rand() % (list.size() < 6 ? list.size() : 6);
	else if ( level == 2 && rand() % 4 == 0 ) pick = rand() % (list.size() < 3 ? list.size() : 3);
	*mx = list[pick].x;
	*my = list[pick].y;
	return true;
}

// ------------------------------------------------------------------
// 판 그리기
// ------------------------------------------------------------------
static int cell_row(int y) { return BOARD_TOP + y; }
static int cell_col(int x) { return BOARD_LEFT + x * 2; }

static void draw_cell(int x, int y)
{
	at(cell_row(y), cell_col(x));
	int s = board[y][x];
	if ( s == NONE ) {
		const char *g;
		if ( y == 0 ) g = (x == 0) ? "┌" : (x == N - 1) ? "┐" : "┬";
		else if ( y == N - 1 ) g = (x == 0) ? "└" : (x == N - 1) ? "┘" : "┴";
		else g = (x == 0) ? "├" : (x == N - 1) ? "┤" : "┼";
		printf(O_BROWN "%s", g);
	} else {
		bool last = (x == last_x && y == last_y);
		printf("%s%s", s == STONE_B ? O_BLACK : O_WHITE, last ? "◎" : "●");
	}
	printf(O_WHITE);
}

static void draw_board(void)
{
	at(BOARD_TOP - 1, BOARD_LEFT);
	printf(O_GRAY);
	for ( int x = 0; x < N; x++ ) printf("%c ", 'A' + x);
	for ( int y = 0; y < N; y++ ) {
		at(cell_row(y), 2);
		printf(O_GRAY "%2d" O_WHITE, N - y);
		for ( int x = 0; x < N; x++ ) draw_cell(x, y);
	}
	printf(O_WHITE);
}

// 오른쪽 안내 칸의 한 줄 (지우고 쓴다)
static void panel(int row, const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	at(row, PANEL);
	printf("\033[K%s" O_WHITE, buf);
}

static void place_cursor(void)
{
	at(cell_row(cur_y), cell_col(cur_x));
	fflush(stdout);
}

// ------------------------------------------------------------------
// 키 입력: 방향키, 좌표(H8), 말하기
// ------------------------------------------------------------------
enum { K_NONE, K_PLACE, K_QUIT, K_CHAT, K_UNDO, K_INVITE, K_MOVED };

struct keyreader {
	int esc;			// 0, 1: ESC, 2: ESC[
	int han_lead;
	std::string coord;	// 치고 있는 좌표
	bool chatting;
	std::string chat;	// 치고 있는 말
	bool line_mode;		// 초대할 아이디처럼 한 줄 받기 (chat 을 같이 씀)
};

static void draw_input(keyreader &k)
{
	at(INPUT_ROW, 1);
	if ( k.chatting ) {
		printf("\033[K " O_CYAN "%s >>" O_WHITE " %s", k.line_mode ? "초대할 아이디" : "말하기", k.chat.c_str());
		fflush(stdout);
		return;
	}
	printf("\033[K");
	if ( !k.coord.empty() ) printf(" " O_YELLOW "좌표 >>" O_WHITE " %s", k.coord.c_str());
	place_cursor();
}

// 바이트 하나. 무엇을 해야 하는지 돌려준다
static int key(keyreader &k, unsigned char c)
{
	if ( k.esc == 1 ) {
		k.esc = (c == '[') ? 2 : 0;
		// 말하는 중에 ESC 만 누르면 그만 말하기
		if ( k.esc == 0 && k.chatting ) {
			k.chatting = false;
			k.line_mode = false;
			k.chat.clear();
			printf(ESC_ENG);
			draw_input(k);
		}
		return K_NONE;
	}
	if ( k.esc == 2 ) {
		if ( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~' ) {
			k.esc = 0;
			if ( k.chatting ) return K_NONE;
			if ( c == 'A' && cur_y > 0 ) cur_y--;
			else if ( c == 'B' && cur_y < N - 1 ) cur_y++;
			else if ( c == 'C' && cur_x < N - 1 ) cur_x++;
			else if ( c == 'D' && cur_x > 0 ) cur_x--;
			return K_MOVED;
		}
		return K_NONE;
	}
	if ( c == 0x1b ) { k.esc = 1; return K_NONE; }

	if ( k.chatting ) {
		if ( k.han_lead >= 0 ) {
			if ( c >= 0xA1 && k.chat.size() + 2 <= 60 ) { k.chat += (char)k.han_lead; k.chat += (char)c; }
			k.han_lead = -1;
			draw_input(k);
			return K_NONE;
		}
		if ( c >= 0xA1 ) { k.han_lead = c; return K_NONE; }
		if ( c == '\r' ) { k.chatting = false; printf(ESC_ENG); return K_CHAT; }
		if ( c == '\b' || c == 0x7f ) {
			if ( !k.chat.empty() ) {
				unsigned int i = 0, last = 1;
				while ( i < k.chat.size() ) { last = ((unsigned char)k.chat[i] >= 0xA1 && i + 1 < k.chat.size()) ? 2 : 1; i += last; }
				k.chat.erase(k.chat.size() - last);
			}
			draw_input(k);
			return K_NONE;
		}
		if ( c >= 0x20 && c < 0x7f && k.chat.size() < 60 ) { k.chat += (char)c; draw_input(k); }
		return K_NONE;
	}

	char lc = tolower(c);
	if ( lc >= 'a' && lc <= 'a' + N - 1 ) {
		k.coord = std::string(1, toupper(c));
		draw_input(k);
		return K_NONE;
	}
	if ( c >= '0' && c <= '9' && !k.coord.empty() && k.coord.size() < 3 ) {
		k.coord += (char)c;
		draw_input(k);
		return K_NONE;
	}
	if ( c == '\b' || c == 0x7f ) {
		if ( !k.coord.empty() ) k.coord.erase(k.coord.size() - 1);
		draw_input(k);
		return K_NONE;
	}
	if ( c == '\r' || c == ' ' ) {
		if ( !k.coord.empty() ) {
			// 좌표로 커서를 옮기고 둔다
			int x = k.coord[0] - 'A', n = atoi(k.coord.c_str() + 1);
			k.coord.clear();
			draw_input(k);
			if ( n < 1 || n > N ) return K_NONE;
			cur_x = x;
			cur_y = N - n;
		}
		return K_PLACE;
	}
	if ( lc == 'q' ) return K_QUIT;
	if ( lc == 't' ) { k.chatting = true; k.line_mode = false; k.chat.clear(); k.han_lead = -1; printf(ESC_HAN); draw_input(k); return K_NONE; }
	if ( lc == 'u' ) return K_UNDO;
	if ( lc == 'v' ) return K_INVITE;		// (A~O 는 좌표라서 초대는 V)
	return K_NONE;
}

// 입력을 기다린다 (timeout_ms 동안). 키가 오면 처리해서 결과, 아니면 K_NONE
static int wait_key(keyreader &k, int timeout_ms)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	struct timeval tv;
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	if ( select(1, &fds, NULL, NULL, &tv) <= 0 ) return K_NONE;
	unsigned char buf[64];
	int n = read(0, buf, sizeof(buf));
	if ( n <= 0 ) host_close();
	int result = K_NONE;
	for ( int i = 0; i < n; i++ ) {
		int r = key(k, buf[i]);
		if ( r != K_NONE ) {
			result = r;
			if ( r != K_MOVED ) break;		// 두기/말하기 등은 한 번에 하나
		}
	}
	if ( result == K_MOVED ) { draw_input(k); result = K_NONE; }
	return result;
}

// ------------------------------------------------------------------
// 컴퓨터와 두기
// ------------------------------------------------------------------
static const char *level_name[] = { "", "쉬움", "보통", "어려움" };

static void play_ai(int level, int me)
{
	int ai = (me == STONE_B) ? STONE_W : STONE_B;
	reset_board();
	cur_x = cur_y = 7;
	keyreader k;
	k.esc = 0; k.han_lead = -1; k.chatting = false; k.line_mode = false;

	print_header(O_CYAN "오 목" O_WHITE " - 컴퓨터와 두기");
	draw_board();
	panel(5, "%s●" O_WHITE " 흑  %s", O_BLACK, me == STONE_B ? user_nick.c_str() : "컴퓨터");
	panel(6, "%s●" O_WHITE " 백  %s", O_WHITE, me == STONE_W ? user_nick.c_str() : "컴퓨터");
	panel(7, O_GRAY "컴퓨터: %s" O_WHITE, level_name[level]);
	at(HELP_ROW, 1);
	printf(O_GRAY " 방향키 이동  Space/Enter 두기  H8 처럼 좌표+Enter  U 무르기  Q 그만두기" O_WHITE);
	printf(ESC_ENG);

	std::string message;
	int winner = 0;
	while ( 1 ) {
		int turn = to_move();
		panel(9, "%s 차례 (%d 수째)", turn == STONE_B ? "흑" : "백", (int)moves.size() / 2 + 1);
		panel(10, last_x >= 0 ? "마지막 수: %s" : "", last_x >= 0 ? coord(last_x, last_y).c_str() : "");
		panel(12, "%s", message.c_str());
		place_cursor();

		if ( moves.size() / 2 >= N * N ) { winner = 3; break; }

		if ( turn == ai ) {
			int x, y;
			panel(9, "컴퓨터가 생각하는 중...");
			place_cursor();
			usleep(300000);
			if ( !ai_move(ai, level, &x, &y) ) { winner = 3; break; }
			board[y][x] = ai;
			moves += (char)('a' + x);
			moves += (char)('a' + y);
			int px = last_x, py = last_y;
			last_x = x; last_y = y;
			if ( px >= 0 ) draw_cell(px, py);
			draw_cell(x, y);
			if ( five(x, y, ai) ) { winner = ai; break; }
			continue;
		}

		int r = wait_key(k, 1000);
		if ( r == K_QUIT ) {
			at(INPUT_ROW, 1);
			printf("\033[K " O_YELLOW "그만두시겠습니까? (y/N) " O_WHITE);
			fflush(stdout);
			unsigned char c = 0;
			while ( read(0, &c, 1) == 1 && c == 0 ) ;
			if ( tolower(c) == 'y' ) { winner = ai; message = "기권"; break; }
			draw_input(k);
		} else if ( r == K_UNDO ) {
			// 컴퓨터 수와 내 수를 하나씩 무른다 (컴퓨터가 흑이면 첫 수는 남긴다)
			unsigned int keep = (me == STONE_W) ? 2 : 0;
			if ( moves.size() >= keep + 4 ) {
				apply_moves(moves.substr(0, moves.size() - 4));
				draw_board();
				message = "한 수 물렀습니다.";
			}
		} else if ( r == K_PLACE ) {
			if ( board[cur_y][cur_x] != NONE ) {
				message = O_RED "이미 돌이 있습니다." O_WHITE;
			} else if ( forbidden(cur_x, cur_y, me) ) {
				message = O_RED "쌍삼이라 둘 수 없습니다." O_WHITE;
			} else {
				message = "";
				board[cur_y][cur_x] = me;
				moves += (char)('a' + cur_x);
				moves += (char)('a' + cur_y);
				int px = last_x, py = last_y;
				last_x = cur_x; last_y = cur_y;
				if ( px >= 0 ) draw_cell(px, py);
				draw_cell(cur_x, cur_y);
				if ( five(cur_x, cur_y, me) ) { winner = me; break; }
			}
		}
	}

	place_cursor();
	if ( winner == me ) add_stat(user_id, "AI_WIN");
	else if ( winner == ai ) add_stat(user_id, "AI_LOSE");
	panel(9, "");
	panel(12, "%s", winner == me ? O_YELLOW "이겼습니다!" : winner == ai ? O_RED "졌습니다." : "비겼습니다.");
	if ( !message.empty() ) panel(13, "%s", message.c_str());
	at(INPUT_ROW, 1);
	printf("\033[K");
	flush_input();
	wait_enter();
}

// ------------------------------------------------------------------
// 사람끼리
// ------------------------------------------------------------------
struct game_state {
	int status, winner;
	std::string moves, reason, black, white;
	int since_move, seen_b, seen_w;		// 초
	bool ok;
};

static game_state load_game(int no)
{
	game_state g;
	g.ok = false;
	char q[512];
	snprintf(q, sizeof(q), "SELECT STATUS, WINNER, MOVES, REASON, BLACK_ID, WHITE_ID, "
			"TIMESTAMPDIFF(SECOND, UPDATED, NOW()) AS T, TIMESTAMPDIFF(SECOND, SEEN_B, NOW()) AS SB, "
			"TIMESTAMPDIFF(SECOND, SEEN_W, NOW()) AS SW FROM omok_game WHERE NO=%d", no);
	std::vector<std::map<std::string, std::string> > r = database::fetch_rows(q);
	if ( r.size() == 0 ) return g;
	g.ok = true;
	g.status = atoi(r[0]["STATUS"].c_str());
	g.winner = atoi(r[0]["WINNER"].c_str());
	g.moves = r[0]["MOVES"];
	g.reason = r[0]["REASON"];
	g.black = r[0]["BLACK_ID"];
	g.white = r[0]["WHITE_ID"];
	g.since_move = atoi(r[0]["T"].c_str());
	g.seen_b = atoi(r[0]["SB"].c_str());
	g.seen_w = atoi(r[0]["SW"].c_str());
	return g;
}

// 판을 끝낸다 (먼저 끝낸 쪽만 성공하고, 그쪽이 두 사람 전적을 남긴다)
static bool end_game(int no, int winner, const char *reason, const game_state &g)
{
	char q[512];
	snprintf(q, sizeof(q), "UPDATE omok_game SET STATUS=2, WINNER=%d, REASON='%s', UPDATED=NOW() WHERE NO=%d AND STATUS=1",
			winner, database::escape(reason).c_str(), no);
	if ( mysql_query(mysql, q) != 0 || mysql_affected_rows(mysql) != 1 ) return false;
	if ( winner == 3 ) {
		add_stat(g.black, "DRAW");
		add_stat(g.white, "DRAW");
	} else {
		add_stat(winner == STONE_B ? g.black : g.white, "WIN");
		add_stat(winner == STONE_B ? g.white : g.black, "LOSE");
	}
	return true;
}

static void add_chat(int no, const std::string &text)
{
	char head[32];
	snprintf(head, sizeof(head), "%d", no);
	std::string q = "INSERT INTO omok_chat (GAME_NO, USER_ID, TEXT, DATE_TIME) VALUES (" + std::string(head) + ", '" +
		database::escape(user_id.c_str()) + "', '" + database::escape(text.c_str()) + "', NOW())";
	mysql_query(mysql, q.c_str());
}

// 대화 (오른쪽 아래 몇 줄)
#define CHAT_TOP	14
#define CHAT_LINES	5
static std::vector<std::string> chat_lines;
static int chat_last = 0;

static void draw_chat(void)
{
	panel(CHAT_TOP - 1, O_GRAY "── 대화 (T 말하기) ────────" O_WHITE);	// 40 칸
	for ( unsigned int i = 0; i < CHAT_LINES; i++ ) {
		panel(CHAT_TOP + i, "%s", i < chat_lines.size() ? chat_lines[i].c_str() : "");
	}
}

// 새 대화를 읽어 오고, 있으면 다시 그린다
static void poll_chat(int no)
{
	char q[256];
	snprintf(q, sizeof(q), "SELECT c.NO, c.USER_ID, c.TEXT, m.NICK_NAME FROM omok_chat c LEFT JOIN member m ON m.USER_ID = c.USER_ID "
			"WHERE c.GAME_NO=%d AND c.NO > %d ORDER BY c.NO LIMIT 20", no, chat_last);
	std::vector<std::map<std::string, std::string> > r = database::fetch_rows(q);
	if ( r.size() == 0 ) return;
	for ( unsigned int i = 0; i < r.size(); i++ ) {
		chat_last = atoi(r[i]["NO"].c_str());
		std::string n = display_text(r[i]["NICK_NAME"]);
		if ( n.empty() ) n = display_text(r[i]["USER_ID"]);
		std::string line = string_truncate(n, 10, "") + ": " + display_text(r[i]["TEXT"]);
		chat_lines.push_back(string_truncate(line, 40, ""));
	}
	while ( chat_lines.size() > CHAT_LINES ) chat_lines.erase(chat_lines.begin());
	draw_chat();
}

// 접속 중인 회원에게 초대 전보 (main 의 전보와 같은 telegram 테이블, SIGUSR1)
static std::string invite(const std::string &who, int no)
{
	bool exist;
	std::string to;
	database::user_info((char*)who.c_str(), &exist);
	if ( exist ) to = who;
	else {
		std::map<std::string, std::string> u = database::user_info_by_nick_name((char*)who.c_str(), &exist);
		if ( exist ) to = u["USER_ID"];
	}
	if ( to.empty() ) return "'" + string_truncate(display_text(who), 16, "") + "' 회원이 없습니다.";
	if ( to == user_id ) return "자기 자신은 초대할 수 없습니다.";

	char pattern[1024];
	snprintf(pattern, sizeof(pattern), "%s/tmp/*.tty", getenv("HANULSO"));
	std::vector<std::string> files = find_files(pattern);
	std::vector<int> pids;
	for ( unsigned int i = 0; i < files.size(); i++ ) {
		std::string id;
		if ( !read_tty_file(files[i], id) || id != to ) continue;
		char sid[256] = "";
		int pid = 0;
		sscanf(trim(read_file(files[i].c_str())).c_str(), "%255s %d", sid, &pid);
		if ( is_bbs_process(pid) ) pids.push_back(pid);
	}
	if ( pids.empty() ) return nick_of(to) + " 님은 지금 접속해 있지 않습니다.";

	char text[256];
	snprintf(text, sizeof(text), "오목 한 판 두실래요? GO OMOK 에서 %d번 방으로 오세요.", no);
	std::string q = "INSERT INTO telegram (FROM_USER_ID, TO_USER_ID, TEXT, DATE_TIME) VALUES ('" +
		database::escape(user_id.c_str()) + "', '" + database::escape(to.c_str()) + "', '" + database::escape(text) + "', NOW())";
	if ( mysql_query(mysql, q.c_str()) != 0 ) return "초대를 보내지 못했습니다.";
	for ( unsigned int i = 0; i < pids.size(); i++ ) kill(pids[i], SIGUSR1);
	return nick_of(to) + " 님께 초대를 보냈습니다.";
}

// 대국 화면 (두는 사람이면 me 가 STONE_B/STONE_W, 구경이면 NONE)
static void play_online(int no, int me)
{
	current_game = no;
	waiting_room = false;
	chat_lines.clear();
	chat_last = 0;
	cur_x = cur_y = 7;
	keyreader k;
	k.esc = 0; k.han_lead = -1; k.chatting = false; k.line_mode = false;

	game_state g = load_game(no);
	if ( !g.ok ) return;
	apply_moves(g.moves);
	std::string bn = nick_of(g.black), wn = nick_of(g.white);

	char head[128];
	snprintf(head, sizeof(head), O_CYAN "오 목" O_WHITE " - %d번 방%s", no, me == NONE ? " (구경)" : "");
	print_header(head);
	draw_board();
	panel(5, "%s●" O_WHITE " 흑  %s%s", O_BLACK, bn.c_str(), me == STONE_B ? O_YELLOW " (나)" : "");
	panel(6, "%s●" O_WHITE " 백  %s%s", O_WHITE, wn.c_str(), me == STONE_W ? O_YELLOW " (나)" : "");
	at(HELP_ROW, 1);
	if ( me == NONE ) printf(O_GRAY " 구경 중입니다.  T 말하기  Q 나가기" O_WHITE);
	else printf(O_GRAY " 방향키 이동  Space/Enter 두기  H8 처럼 좌표+Enter  T 말하기  Q 기권" O_WHITE);
	printf(ESC_ENG);
	draw_chat();
	poll_chat(no);

	std::string message;
	long last_poll = 0, last_seen = 0;
	while ( 1 ) {
		long now = now_ms();
		if ( now - last_poll >= 700 ) {
			last_poll = now;
			g = load_game(no);
			if ( !g.ok ) break;
			if ( g.moves != moves ) {
				int px = last_x, py = last_y;
				bool one_more = g.moves.size() == moves.size() + 2 && g.moves.compare(0, moves.size(), moves) == 0;
				apply_moves(g.moves);
				if ( one_more ) {
					if ( px >= 0 ) draw_cell(px, py);
					draw_cell(last_x, last_y);
				} else {
					draw_board();
				}
				if ( me != NONE && to_move() == me ) printf("\007");	// 상대가 두었다
			}
			if ( g.status == 2 ) break;

			int turn = to_move();
			int left = MOVE_SEC - g.since_move;
			if ( left < 0 ) left = 0;
			panel(8, "%s 차례%s" O_GRAY "  (%d 수째)", turn == STONE_B ? "흑" : "백",
					turn == me ? O_YELLOW " - 내 차례" : "", (int)moves.size() / 2 + 1);
			panel(9, "남은 시간 %s%d초" O_WHITE, left <= 10 ? O_RED : "", left);
			panel(10, last_x >= 0 ? "마지막 수: %s" : "", last_x >= 0 ? coord(last_x, last_y).c_str() : "");
			panel(11, "%s", message.c_str());
			poll_chat(no);
			draw_input(k);

			if ( me != NONE ) {
				// 내가 아직 여기 있다고 알린다 (3 초마다)
				if ( now - last_seen >= 3000 ) {
					last_seen = now;
					char q[128];
					snprintf(q, sizeof(q), "UPDATE omok_game SET %s=NOW() WHERE NO=%d", me == STONE_B ? "SEEN_B" : "SEEN_W", no);
					mysql_query(mysql, q);
				}
				int opp = (me == STONE_B) ? STONE_W : STONE_B;
				int opp_seen = (me == STONE_B) ? g.seen_w : g.seen_b;
				if ( opp_seen > GONE_SEC ) end_game(no, me, "상대가 나감", g);
				else if ( turn == me && g.since_move > MOVE_SEC ) end_game(no, opp, "시간 초과", g);
				else if ( turn == opp && g.since_move > MOVE_SEC + 5 ) end_game(no, me, "시간 초과", g);
			}
		}

		int r = wait_key(k, 200);
		if ( r == K_CHAT ) {
			std::string text = trim(display_text(k.chat));
			k.chat.clear();
			if ( !text.empty() ) add_chat(no, text);
			draw_input(k);
			last_poll = 0;
		} else if ( r == K_QUIT ) {
			if ( me == NONE ) break;
			at(INPUT_ROW, 1);
			printf("\033[K " O_YELLOW "기권하시겠습니까? (y/N) " O_WHITE);
			fflush(stdout);
			unsigned char c = 0;
			while ( read(0, &c, 1) == 1 && c == 0 ) ;
			if ( tolower(c) == 'y' ) {
				g = load_game(no);
				end_game(no, me == STONE_B ? STONE_W : STONE_B, "기권", g);
				last_poll = 0;
			}
			draw_input(k);
		} else if ( r == K_PLACE && me != NONE ) {
			if ( to_move() != me ) {
				message = O_GRAY "상대 차례입니다." O_WHITE;
			} else if ( board[cur_y][cur_x] != NONE ) {
				message = O_RED "이미 돌이 있습니다." O_WHITE;
			} else if ( forbidden(cur_x, cur_y, me) ) {
				message = O_RED "쌍삼이라 둘 수 없습니다." O_WHITE;
			} else {
				message = "";
				char q[256];
				snprintf(q, sizeof(q), "UPDATE omok_game SET MOVES=CONCAT(MOVES, '%c%c'), UPDATED=NOW() "
						"WHERE NO=%d AND STATUS=1 AND CHAR_LENGTH(MOVES)=%d",
						'a' + cur_x, 'a' + cur_y, no, (int)moves.size());
				if ( mysql_query(mysql, q) == 0 && mysql_affected_rows(mysql) == 1 ) {
					std::string m = moves + (char)('a' + cur_x) + (char)('a' + cur_y);
					int px = last_x, py = last_y;
					apply_moves(m);
					if ( px >= 0 ) draw_cell(px, py);
					draw_cell(last_x, last_y);
					g = load_game(no);
					if ( five(last_x, last_y, me) ) end_game(no, me, "오목", g);
					else if ( moves.size() / 2 >= N * N ) end_game(no, 3, "판이 꽉 참", g);
				}
			}
			last_poll = 0;
		}
	}

	// 결과
	current_game = 0;
	g = load_game(no);
	if ( g.ok && g.status == 2 ) {
		apply_moves(g.moves);
		draw_board();
		const char *res;
		if ( g.winner == 3 ) res = "비겼습니다.";
		else if ( me == NONE ) res = g.winner == STONE_B ? "흑이 이겼습니다." : "백이 이겼습니다.";
		else res = g.winner == me ? O_YELLOW "이겼습니다!" : O_RED "졌습니다.";
		panel(8, "%s", res);
		panel(9, O_GRAY "%s" O_WHITE, display_text(g.reason).c_str());
		panel(10, "");
		panel(11, "");
		at(INPUT_ROW, 1);
		printf("\033[K");
		flush_input();
		wait_enter();
	}
}

// 방을 만들고 상대를 기다린다
static void make_room(void)
{
	mysql_query(mysql, ("INSERT INTO omok_game (BLACK_ID, CREATED, UPDATED, SEEN_B, SEEN_W) VALUES ('" +
				database::escape(user_id.c_str()) + "', NOW(), NOW(), NOW(), NOW())").c_str());
	int no = (int)mysql_insert_id(mysql);
	if ( no <= 0 ) return;
	current_game = no;
	waiting_room = true;
	reset_board();

	keyreader k;
	k.esc = 0; k.han_lead = -1; k.chatting = false; k.line_mode = false;
	char head[128];
	snprintf(head, sizeof(head), O_CYAN "오 목" O_WHITE " - %d번 방", no);
	print_header(head);
	draw_board();
	panel(5, "%s●" O_WHITE " 흑  %s" O_YELLOW " (나)", O_BLACK, user_nick.c_str());
	panel(6, "%s●" O_WHITE " 백  " O_GRAY "(기다리는 중)", O_WHITE);
	panel(8, "상대를 기다리고 있습니다...");
	panel(9, O_GRAY "다른 회원이 GO OMOK 에서 %d번 방에" O_WHITE, no);
	panel(10, O_GRAY "들어오면 시작합니다." O_WHITE);
	at(HELP_ROW, 1);
	printf(O_GRAY " V 접속 중인 회원 초대하기 (아이디/닉네임)  Q 방 닫기" O_WHITE);
	printf(ESC_ENG);

	long last = 0;
	int dots = 0;
	while ( 1 ) {
		long now = now_ms();
		if ( now - last >= 1000 ) {
			last = now;
			char q[128];
			snprintf(q, sizeof(q), "UPDATE omok_game SET SEEN_B=NOW() WHERE NO=%d", no);
			mysql_query(mysql, q);
			game_state g = load_game(no);
			if ( !g.ok || g.status == 2 ) break;
			if ( g.status == 1 ) {
				printf("\007");
				play_online(no, STONE_B);
				break;
			}
			panel(8, "상대를 기다리고 있습니다%s", std::string(dots++ % 4, '.').c_str());
			if ( !k.chatting ) place_cursor();
		}
		int r = wait_key(k, 300);
		if ( r == K_QUIT ) {
			char q[256];
			snprintf(q, sizeof(q), "UPDATE omok_game SET STATUS=2, REASON='방을 닫음' WHERE NO=%d AND STATUS=0", no);
			mysql_query(mysql, q);
			if ( mysql_affected_rows(mysql) == 1 ) break;	// 그 사이에 누가 들어왔으면 계속
		} else if ( r == K_INVITE && !k.chatting ) {
			k.chatting = true;
			k.line_mode = true;
			k.chat.clear();
			k.han_lead = -1;
			draw_input(k);
		} else if ( r == K_CHAT && k.line_mode ) {
			k.line_mode = false;
			std::string who = trim(k.chat);
			k.chat.clear();
			if ( !who.empty() ) panel(12, "%s", invite(who, no).c_str());
			draw_input(k);
		}
	}
	current_game = 0;
	waiting_room = false;
}

// 다른 사람의 방에 들어간다
static bool join_room(int no)
{
	char q[512];
	snprintf(q, sizeof(q), "UPDATE omok_game SET WHITE_ID='%s', STATUS=1, UPDATED=NOW(), SEEN_W=NOW() "
			"WHERE NO=%d AND STATUS=0 AND BLACK_ID<>'%s' AND SEEN_B > NOW() - INTERVAL 30 SECOND",
			database::escape(user_id.c_str()).c_str(), no, database::escape(user_id.c_str()).c_str());
	if ( mysql_query(mysql, q) != 0 || mysql_affected_rows(mysql) != 1 ) return false;
	play_online(no, STONE_W);
	return true;
}

// ------------------------------------------------------------------
// 순위
// ------------------------------------------------------------------
static void show_rank(void)
{
	print_header(O_CYAN "오목 순위" O_WHITE);
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)
			"SELECT * FROM omok_stat WHERE WIN + LOSE + DRAW > 0 ORDER BY WIN DESC, LOSE, DRAW DESC LIMIT 12");
	printf("\r\n  " O_GRAY "%4s  %-16s %5s %5s %5s %7s   %s" O_WHITE "\r\n", "순위", "이름", "승", "패", "무", "승률", "컴퓨터 상대 (승/패)");
	if ( rows.size() == 0 ) printf("  " O_GRAY "    아직 사람끼리 둔 기록이 없습니다." O_WHITE "\r\n");
	for ( unsigned int i = 0; i < rows.size(); i++ ) {
		int w = atoi(rows[i]["WIN"].c_str()), l = atoi(rows[i]["LOSE"].c_str()), d = atoi(rows[i]["DRAW"].c_str());
		char rate[16];
		snprintf(rate, sizeof(rate), "%d%%", (w + l + d) > 0 ? w * 100 / (w + l + d) : 0);
		bool me = rows[i]["USER_ID"] == user_id;
		printf("  %s%4d  %-16s %5d %5d %5d %7s   %s/%s" O_WHITE "\r\n", me ? O_YELLOW : O_WHITE, i + 1,
				string_truncate(display_text(rows[i]["NAME"]), 16, "").c_str(), w, l, d, rate,
				rows[i]["AI_WIN"].c_str(), rows[i]["AI_LOSE"].c_str());
	}
	std::vector<std::map<std::string, std::string> > mine = database::fetch_rows((char*)
			("SELECT * FROM omok_stat WHERE USER_ID='" + database::escape(user_id.c_str()) + "'").c_str());
	if ( mine.size() > 0 ) {
		printf("\r\n  " O_CYAN "내 전적" O_WHITE "  사람 %s승 %s패 %s무   컴퓨터 %s승 %s패\r\n",
				mine[0]["WIN"].c_str(), mine[0]["LOSE"].c_str(), mine[0]["DRAW"].c_str(),
				mine[0]["AI_WIN"].c_str(), mine[0]["AI_LOSE"].c_str());
	}
	wait_enter();
}

// ------------------------------------------------------------------
// 대기실
// ------------------------------------------------------------------
static void lobby(void)
{
	while ( 1 ) {
		print_header(O_CYAN "오 목" O_WHITE);
		printf("\r\n" O_BROWN
				"       ┌┬┬┬┬┬┐   " O_WHITE "다섯 개를 먼저 나란히 놓으면 이깁니다." O_BROWN "\r\n"
				"       ├" O_WHITE "●" O_BROWN "┼┼┼┼┤   " O_GRAY "15x15, 흑이 먼저, 쌍삼 금지, 여섯 이상은 이기지 못함" O_BROWN "\r\n"
				"       ├┼" O_BLACK "●" O_WHITE "●" O_BROWN "┼┼┤   " O_GRAY "한 수에 %d 초" O_BROWN "\r\n"
				"       ├┼┼" O_BLACK "●" O_BROWN "┼┼┤\r\n"
				"       └┴┴┴" O_BLACK "●" O_BROWN "┴┘\r\n" O_WHITE, MOVE_SEC);

		// 기다리는 방 / 두는 판 (소식이 끊긴 것은 빼고)
		std::vector<std::map<std::string, std::string> > rooms = database::fetch_rows((char*)
				"SELECT NO, BLACK_ID, WHITE_ID, STATUS, CHAR_LENGTH(MOVES) DIV 2 AS N, "
				"TIMESTAMPDIFF(MINUTE, CREATED, NOW()) AS M FROM omok_game "
				"WHERE (STATUS = 0 AND SEEN_B > NOW() - INTERVAL 30 SECOND) "
				"OR (STATUS = 1 AND (SEEN_B > NOW() - INTERVAL 1 MINUTE OR SEEN_W > NOW() - INTERVAL 1 MINUTE)) "
				"ORDER BY STATUS, NO LIMIT 8");
		printf("\r\n  " O_YELLOW "◆ 대국방" O_WHITE "\r\n");
		if ( rooms.size() == 0 ) printf("  " O_GRAY "    지금 열린 방이 없습니다. N 으로 방을 만들어 보세요." O_WHITE "\r\n");
		for ( unsigned int i = 0; i < rooms.size(); i++ ) {
			std::map<std::string, std::string> &r = rooms[i];
			if ( r["STATUS"] == "0" ) {
				printf("  %3s번  " O_CYAN "%-14s" O_WHITE " 상대를 기다리는 중 " O_GRAY "(%s분째)" O_WHITE "\r\n",
						r["NO"].c_str(), string_truncate(nick_of(r["BLACK_ID"]), 14, "").c_str(), r["M"].c_str());
			} else {
				printf("  %3s번  %s●" O_WHITE " %-12s vs %s●" O_WHITE " %-12s " O_GRAY "%s수째 (구경)" O_WHITE "\r\n",
						r["NO"].c_str(), O_BLACK, string_truncate(nick_of(r["BLACK_ID"]), 12, "").c_str(),
						O_WHITE, string_truncate(nick_of(r["WHITE_ID"]), 12, "").c_str(), r["N"].c_str());
			}
		}

		printf("\r\n  " O_YELLOW "[C]" O_WHITE " 컴퓨터와 두기  " O_YELLOW "[N]" O_WHITE " 방 만들기  "
				O_YELLOW "[번호]" O_WHITE " 들어가기/구경  " O_YELLOW "[R]" O_WHITE " 순위  " O_YELLOW "[Q]" O_WHITE " 나가기\r\n");
		std::string c = ask("\r\n 선택 (Enter: 새로 보기) >> ");

		if ( c.empty() ) continue;
		if ( !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") ) return;
		if ( !strcasecmp(c.c_str(), "r") ) { show_rank(); continue; }
		if ( !strcasecmp(c.c_str(), "n") ) { make_room(); continue; }
		if ( !strcasecmp(c.c_str(), "c") ) {
			std::string lv = ask("\r\n 컴퓨터 실력  [1] 쉬움  [2] 보통  [3] 어려움 >> ");
			int level = atoi(lv.c_str());
			if ( level < 1 || level > 3 ) continue;
			std::string col = ask("\r\n 내 돌  [1] 흑 (먼저)  [2] 백 >> ");
			int me = atoi(col.c_str()) == 2 ? STONE_W : STONE_B;
			play_ai(level, me);
			continue;
		}
		int no = atoi(c.c_str());
		if ( no > 0 ) {
			game_state g = load_game(no);
			if ( !g.ok || g.status == 2 ) {
				printf("\r\n  없는 방이거나 끝난 판입니다.");
				wait_enter();
			} else if ( g.status == 0 ) {
				if ( g.black == user_id ) continue;
				if ( !join_room(no) ) {
					printf("\r\n  들어갈 수 없습니다. (이미 시작했거나 방이 닫혔습니다)");
					wait_enter();
				}
			} else if ( g.black == user_id || g.white == user_id ) {
				play_online(no, g.black == user_id ? STONE_B : STONE_W);	// 끊겼다 다시 들어온 내 판
			} else {
				play_online(no, NONE);
			}
		}
	}
}

int main(int argc, char **argv)
{
	if ( argc < 3 ) {
		printf("usage: %s <host_name> <user_id> [tty]\n", argv[0]);
		return 1;
	}
	user_id = argv[2];
	snprintf(tty, sizeof(tty), "%s", argc > 3 ? argv[3] : "");

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);		// BBS 로 보낸 전보 신호가 잘못 와도 죽지 않게
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

	user_nick = nick_of(user_id);
	lobby();

	host_close();
	return 0;
}
