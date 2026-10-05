#include "main.h"
#include <stdarg.h>
#include <algorithm>

// ------------------------------------------------------------------
// 오늘의 통계 (ST)
//   오늘 다녀간 회원(출석 기준), 지금/오늘 최고/역대 최고 동시 접속,
//   최근 7 일 다녀간 회원 막대그래프, 회원 수, 오늘 새 글, 많이 읽힌 글
// 최고 동시 접속은 로그인할 때마다 stat_online 에 남긴다.
// ------------------------------------------------------------------

#define S_WHITE		"\033[=15F"
#define S_YELLOW	"\033[=14F"
#define S_CYAN		"\033[=11F"
#define S_GREEN		"\033[=10F"
#define S_MAGENTA	"\033[=13F"
#define S_GRAY		"\033[=7F"

static void create_stat_table(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS stat_online ( "
			"DAY DATE NOT NULL PRIMARY KEY, "
			"MAX_ONLINE INT NOT NULL, "
			"MAX_TIME DATETIME NOT NULL )");
}

// 지금 접속해 있는 회원 수 (tmp/<tty>.tty)
static int online_count(void)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "%s/tmp/*.tty", getenv("HANULSO"));
	std::vector<std::string> files = find_files(buf);
	int n = 0;
	for ( unsigned int i = 0; i < files.size(); i++ ) {
		std::string id;
		if ( read_tty_file(files[i], id) ) n++;
	}
	return n;
}

// 로그인할 때: 오늘 최고 동시 접속을 갱신
void stats_record_online(void)
{
	create_stat_table();
	char q[512];
	int n = online_count();
	snprintf(q, sizeof(q), "INSERT INTO stat_online (DAY, MAX_ONLINE, MAX_TIME) VALUES (CURDATE(), %d, NOW()) "
			"ON DUPLICATE KEY UPDATE MAX_TIME = IF(%d > MAX_ONLINE, NOW(), MAX_TIME), "
			"MAX_ONLINE = GREATEST(MAX_ONLINE, %d)", n, n, n);
	mysql_query(mysql, q);
}

static int query_int(const std::string &q)
{
	bool ok;
	std::string v = database::fetch((char*)q.c_str(), &ok);
	return ok ? atoi(v.c_str()) : 0;
}

// 많이 읽힌 글 (C++98 은 함수 안 구조체를 템플릿 인자로 못 써서 밖에)
struct hot { int hit; std::string board, title, user; };

struct board_info {
	std::string table, name;
};

// 메뉴(hanulso.mnu) 에서 이 회원이 볼 수 있는 게시판과 이름
static void collect_boards(pugi::xml_node node, std::vector<board_info> &list)
{
	for ( pugi::xml_node c = node.first_child(); c; c = c.next_sibling() ) {
		if ( c.type() != pugi::node_element ) continue;
		if ( !strcmp(c.attribute("type").value(), "board") && !c.attribute("id").empty() ) {
			int level = c.attribute("access_level").empty() ? 1 : atoi(c.attribute("access_level").value());
			if ( login_user_is_admin || level <= login_user_level ) {
				board_info b;
				b.table = c.attribute("id").value();
				b.name = trim(c.child_value("name"));
				// "공지 (notice)" -> "공지"
				std::string::size_type p = b.name.find(" (");
				if ( p != std::string::npos && p > 0 ) b.name = b.name.substr(0, p);
				if ( b.name.empty() ) b.name = b.table;
				list.push_back(b);
			}
		}
		collect_boards(c, list);
	}
}

static void put(int row, int col, const char *fmt, ...)
{
	printf("\033[%d;%dH", row, col);
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static std::string comma(long v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", v);
	std::string s = buf, r;
	int n = s.size();
	for ( int i = 0; i < n; i++ ) {
		r += s[i];
		if ( (n - i - 1) % 3 == 0 && i != n - 1 ) r += ',';
	}
	return r;
}

void show_stats(void)
{
	create_stat_table();
	static const char *wday[] = { "일", "월", "화", "수", "목", "금", "토" };
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	printf(ESC_CLEAR);
	printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
	printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	char head[64];
	snprintf(head, sizeof(head), "오늘의 통계 - %d월 %d일 (%s)", tm->tm_mon + 1, tm->tm_mday, wday[tm->tm_wday]);
	printf("\033[2;1H\r\033[%dC%s", (int)(80 - strlen(head)) / 2, head);
	printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());

	// ---- 왼쪽: 접속 / 회원
	int visitors = query_int("SELECT COUNT(*) FROM attendance WHERE ATT_DAY = CURDATE()");
	int now_on = online_count();
	std::vector<std::map<std::string, std::string> > today_max = database::fetch_rows((char*)
			"SELECT MAX_ONLINE, DATE_FORMAT(MAX_TIME, '%H:%i') AS T FROM stat_online WHERE DAY = CURDATE()");
	std::vector<std::map<std::string, std::string> > best_max = database::fetch_rows((char*)
			"SELECT MAX_ONLINE, DATE_FORMAT(DAY, '%Y-%m-%d') AS D FROM stat_online ORDER BY MAX_ONLINE DESC, DAY DESC LIMIT 1");
	int members = query_int("SELECT COUNT(*) FROM member");
	int joined = query_int("SELECT COUNT(*) FROM member WHERE REGISTRATION_DATETIME >= CURDATE()");

	int r = 5;
	put(r++, 2, S_CYAN "■ 접속" S_WHITE);
	put(r++, 4, "오늘 다녀간 회원  " S_YELLOW "%s명" S_WHITE, comma(visitors).c_str());
	put(r++, 4, "지금 접속         " S_YELLOW "%s명" S_WHITE, comma(now_on).c_str());
	if ( today_max.size() > 0 ) {
		put(r++, 4, "오늘 최고 동시    " S_YELLOW "%s명" S_GRAY " (%s)" S_WHITE,
				today_max[0]["MAX_ONLINE"].c_str(), today_max[0]["T"].c_str());
	}
	if ( best_max.size() > 0 ) {
		put(r++, 4, "역대 최고 동시    " S_YELLOW "%s명" S_GRAY " (%s)" S_WHITE,
				best_max[0]["MAX_ONLINE"].c_str(), best_max[0]["D"].c_str());
	}
	r++;
	put(r++, 2, S_CYAN "■ 회원" S_WHITE);
	put(r++, 4, "전체 회원         " S_YELLOW "%s명" S_WHITE, comma(members).c_str());
	put(r++, 4, "오늘 가입         " S_YELLOW "%s명" S_WHITE, comma(joined).c_str());
	int left_end = r;

	// ---- 오른쪽: 최근 7 일 다녀간 회원
	std::vector<std::map<std::string, std::string> > days = database::fetch_rows((char*)
			"SELECT DATE_FORMAT(ATT_DAY, '%m/%d') AS D, COUNT(*) AS N FROM attendance "
			"WHERE ATT_DAY > CURDATE() - INTERVAL 7 DAY GROUP BY ATT_DAY");
	std::map<std::string, int> count_of;
	int maxn = 1;
	for ( unsigned int i = 0; i < days.size(); i++ ) {
		int n = atoi(days[i]["N"].c_str());
		count_of[days[i]["D"]] = n;
		if ( n > maxn ) maxn = n;
	}
	r = 5;
	put(r++, 42, S_CYAN "■ 최근 7일 다녀간 회원" S_WHITE);
	// 아무도 안 온 날도 빠짐없이 6 일 전부터 오늘까지
	for ( int k = 6; k >= 0; k-- ) {
		time_t d = t - k * 86400L;
		struct tm dt = *localtime(&d);
		char md[8];
		snprintf(md, sizeof(md), "%02d/%02d", dt.tm_mon + 1, dt.tm_mday);
		int n = count_of.count(md) ? count_of[md] : 0;
		// 막대 최대 10 칸(20 바이트): 44 + 날짜 9 + 20 + 숫자 <= 79
		int len = (n * 10 + maxn - 1) / maxn;
		put(r++, 44, "%s%s %s " S_GREEN "%s" S_WHITE " %d", k == 0 ? S_YELLOW : S_GRAY,
				md, wday[dt.tm_wday], repeat("■", len).c_str(), n);
	}
	if ( r > left_end ) left_end = r;

	// ---- 게시판: 오늘 새 글, 많이 읽힌 글 (최근 7 일)
	std::vector<board_info> boards;
	pugi::xml_document doc;
	if ( doc.load_file("hanulso.mnu") ) collect_boards(doc.child("hanulso"), boards);

	int today_total = 0;
	std::vector<std::pair<int, std::string> > busy;		// (오늘 새 글, 게시판 이름)
	std::vector<hot> hots;
	for ( unsigned int i = 0; i < boards.size(); i++ ) {
		const std::string &tb = boards[i].table;
		// 아직 테이블이 없는 게시판은 건너뛴다 (fetch_rows 는 오류를 화면에 띄우므로)
		bool ok;
		std::string cnt = database::fetch((char*)("SELECT COUNT(*) FROM " + tb + " WHERE DATE_TIME >= CURDATE()").c_str(), &ok);
		if ( !ok ) continue;
		int n = atoi(cnt.c_str());
		today_total += n;
		if ( n > 0 ) busy.push_back(std::make_pair(n, boards[i].name));

		std::string q = "SELECT b.HIT, b.TITLE, b.USER_ID, m.NICK_NAME FROM " + tb + " b "
			"LEFT JOIN member m ON m.USER_ID = b.USER_ID "
			"WHERE b.DATE_TIME >= NOW() - INTERVAL 7 DAY ORDER BY b.HIT DESC LIMIT 4";
		std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)q.c_str());
		for ( unsigned int k = 0; k < rows.size(); k++ ) {
			hot h;
			h.hit = atoi(rows[k]["HIT"].c_str());
			h.board = boards[i].name;
			h.title = display_text(rows[k]["TITLE"]);
			h.user = display_text(rows[k]["NICK_NAME"]);
			if ( h.user.empty() ) h.user = display_text(rows[k]["USER_ID"]);
			if ( h.user.empty() ) h.user = "(탈퇴)";
			hots.push_back(h);
		}
	}
	// 많이 읽힌 순 (같으면 먼저 찾은 것)
	for ( unsigned int i = 1; i < hots.size(); i++ ) {
		for ( unsigned int k = i; k > 0 && hots[k].hit > hots[k - 1].hit; k-- ) std::swap(hots[k], hots[k - 1]);
	}
	std::sort(busy.begin(), busy.end());
	std::reverse(busy.begin(), busy.end());

	r = left_end + 1;
	put(r++, 2, S_CYAN "■ 게시판" S_WHITE "  오늘 새 글 " S_YELLOW "%d개" S_WHITE, today_total);
	if ( busy.size() > 0 ) {
		std::string s;
		for ( unsigned int i = 0; i < busy.size() && i < 4; i++ ) {
			char b[128];
			snprintf(b, sizeof(b), "%s%s %d", s.empty() ? "" : ", ", string_truncate(busy[i].second, 16, "").c_str(), busy[i].first);
			if ( s.size() + strlen(b) > 70 ) break;
			s += b;
		}
		put(r++, 4, S_GRAY "%s" S_WHITE, s.c_str());
	}
	r++;
	put(r++, 2, S_CYAN "■ 많이 읽힌 글" S_GRAY " (최근 7일)" S_WHITE);
	if ( hots.size() == 0 ) put(r++, 4, S_GRAY "최근 7일 동안 올라온 글이 없습니다." S_WHITE);
	// 4 + 번호 2 + 2 + [게시판] 12 + 1 + 제목 34 + 1 + 닉네임 12 + 조회 7 = 75 칸
	for ( unsigned int i = 0; i < hots.size() && i < 5 && r <= 23; i++ ) {
		std::string b = "[" + string_truncate(hots[i].board, 10, "") + "]";
		put(r++, 4, S_YELLOW "%d." S_GRAY " %-12s" S_WHITE " %-34s " S_CYAN "%-12s" S_GRAY "%7s" S_WHITE,
				i + 1, b.c_str(), string_truncate(hots[i].title, 34, "..").c_str(),
				string_truncate(hots[i].user, 12, "").c_str(), ("조회 " + comma(hots[i].hit)).c_str());
	}

	printf("\033[24;1H [Enter] 를 누르세요.");
	press_enter();
}
