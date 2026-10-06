#include "main.h"

// ------------------------------------------------------------------
// 게시판 덧붙임: 꼬리말(한 줄 댓글), 새 글 모아보기(NEW), 전체 게시판 검색(FIND)
// ------------------------------------------------------------------

#define X_W		"\033[=15F"
#define X_Y		"\033[=14F"
#define X_C		"\033[=11F"
#define X_G		"\033[=7F"
#define X_R		"\033[=12F"

#define COMMENT_LEN	46		// 꼬리말 한 줄 (바이트, 한글 23 자)

void comments_init(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS comments ( "
			"NO INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
			"BOARD VARCHAR(64) NOT NULL, "
			"ARTICLE INT NOT NULL, "
			"USER_ID VARCHAR(50) NOT NULL, "
			"TEXT VARCHAR(255) NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"KEY IDX_ARTICLE (BOARD, ARTICLE) )");
}

static std::string nick_or_id(const std::string &id)
{
	static std::map<std::string, std::string> cache;
	std::map<std::string, std::string>::iterator it = cache.find(id);
	if ( it != cache.end() ) return it->second;
	bool exist;
	std::map<std::string, std::string> u = database::user_info((char*)id.c_str(), &exist);
	std::string n = exist ? display_text(u["NICK_NAME"]) : "";
	if ( exist && n.empty() ) n = display_text(id);
	if ( n.empty() ) n = "탈퇴회원";
	cache[id] = n;
	return n;
}

// 글 목록에 쓸 꼬리말 수
int comment_count(const char *table, int no)
{
	char q[512];
	bool ok;
	snprintf(q, sizeof(q), "SELECT COUNT(*) FROM comments WHERE BOARD='%s' AND ARTICLE=%d",
			database::escape(table).c_str(), no);
	return atoi(database::fetch(q, &ok).c_str());
}

static std::vector<std::map<std::string, std::string> > fetch_comments(const char *table, int no)
{
	char q[512];
	snprintf(q, sizeof(q), "SELECT * FROM comments WHERE BOARD='%s' AND ARTICLE=%d ORDER BY NO",
			database::escape(table).c_str(), no);
	return database::fetch_rows(q);
}

// 글 아래에 이어 붙일 꼬리말 줄들 (본문처럼 쪽을 넘긴다)
// 한 줄: 1 + 번호 3 + 1 + 닉네임 12 + 1 + 꼬리말 46 + 2 + 날짜 11 = 77 칸
std::vector<std::string> comment_lines(const char *table, int no)
{
	std::vector<std::string> out;
	std::vector<std::map<std::string, std::string> > r = fetch_comments(table, no);
	out.push_back("");
	char head[128];
	if ( r.empty() ) snprintf(head, sizeof(head), X_G "── 꼬리말이 없습니다. CO 로 달아 보세요 ──" X_W);
	else snprintf(head, sizeof(head), X_G "── 꼬리말 %d개 (CO 내용: 달기, CD 번호: 내 꼬리말 지우기) ──" X_W, (int)r.size());
	out.push_back(head);
	for ( unsigned int i = 0; i < r.size(); i++ ) {
		std::string t = r[i]["DATE_TIME"];
		std::string date = t.size() >= 16 ? t.substr(5, 5) + " " + t.substr(11, 5) : "";
		char line[512];
		snprintf(line, sizeof(line), " %3d " X_C "%-12s" X_W " %-46s  " X_G "%s" X_W, (int)i + 1,
				string_truncate(nick_or_id(r[i]["USER_ID"]), 12, "").c_str(),
				string_truncate(display_text(r[i]["TEXT"]), COMMENT_LEN, "").c_str(), date.c_str());
		out.push_back(line);
	}
	return out;
}

// 꼬리말 달기 (text 가 비었으면 묻는다). 달았으면 true
bool add_comment(const char *table, int no, std::string text)
{
	if ( text.empty() ) {
		char buf[COMMENT_LEN + 8];
		printf(ESC_HAN);
		printf("\r\n꼬리말 (한 줄, Enter: 취소) >> ");
		line_input(buf, COMMENT_LEN);
		printf(ESC_ENG);
		text = buf;
	}
	text = string_truncate(trim(display_text(text)), COMMENT_LEN, "");
	if ( text.empty() ) return false;

	// 도배 막기: 5 초 안에 또
	bool ok;
	char q0[512];
	snprintf(q0, sizeof(q0), "SELECT COUNT(*) FROM comments WHERE USER_ID='%s' AND DATE_TIME > NOW() - INTERVAL 5 SECOND",
			database::escape(login_user_id).c_str());
	if ( atoi(database::fetch(q0, &ok).c_str()) > 0 ) {
		printf("\r\n잠시 후에 다시 달아 주세요.\r\n[Enter] 를 누르세요.");
		press_enter();
		return false;
	}

	std::string q = "INSERT INTO comments (BOARD, ARTICLE, USER_ID, TEXT, DATE_TIME) VALUES ('" +
		database::escape(table) + "', " + TO_STRING(no) + ", '" + database::escape(login_user_id) + "', '" +
		database::escape(text.c_str()) + "', NOW())";
	if ( mysql_query(mysql, q.c_str()) != 0 ) return false;

	// 글쓴이가 접속해 있으면 알린다 (대화방 안에서도)
	char q2[512];
	snprintf(q2, sizeof(q2), "SELECT CONCAT(USER_ID, CHAR(9), TITLE) FROM %s WHERE NO=%d", table, no);
	std::string r = database::fetch(q2, &ok);
	std::string::size_type tab = r.find('\t');
	if ( ok && tab != std::string::npos ) {
		std::string author = r.substr(0, tab);
		if ( author != login_user_id ) {
			notify_online(author, "◆ 꼬리말 ─ " + nick_or_id(login_user_id) + " 님이 '" +
					string_truncate(display_text(r.substr(tab + 1)), 30, "..") + "' 에 꼬리말을 달았습니다.", false);
		}
	}
	return true;
}

// 꼬리말 지우기 (index 는 화면의 번호). 내 것만, 운영자는 아무거나
void delete_comment(const char *table, int no, int index)
{
	std::vector<std::map<std::string, std::string> > r = fetch_comments(table, no);
	if ( index < 1 || index > (int)r.size() ) {
		printf("\r\n그런 꼬리말이 없습니다.\r\n[Enter] 를 누르세요.");
		press_enter();
		return;
	}
	if ( r[index - 1]["USER_ID"] != login_user_id && !login_user_is_admin ) {
		printf("\r\n내 꼬리말만 지울 수 있습니다.\r\n[Enter] 를 누르세요.");
		press_enter();
		return;
	}
	std::string q = "DELETE FROM comments WHERE NO=" + r[index - 1]["NO"];
	mysql_query(mysql, q.c_str());
}

// 글을 지울 때 그 글의 꼬리말도
void delete_comments_of(const char *table, int no)
{
	char q[512];
	snprintf(q, sizeof(q), "DELETE FROM comments WHERE BOARD='%s' AND ARTICLE=%d", database::escape(table).c_str(), no);
	mysql_query(mysql, q);
}

// ------------------------------------------------------------------
// 여러 게시판에서 모은 글 목록 (새 글 모아보기, 전체 검색)
// ------------------------------------------------------------------
pugi::xml_node menu_root;

struct found_article {
	std::string table, board, user_id, date_time, title;
	int no, hit;
	pugi::xml_node node;
};

static bool newer_first(const found_article &a, const found_article &b)
{
	return a.date_time > b.date_time;
}

// 이 회원이 들어갈 수 있는 게시판들 (노드)
static void readable_boards(pugi::xml_node node, std::vector<pugi::xml_node> &out)
{
	for ( pugi::xml_node c = node.first_child(); c; c = c.next_sibling() ) {
		if ( c.type() != pugi::node_element ) continue;
		if ( !strcmp(c.attribute("type").value(), "board") && !c.attribute("id").empty() ) {
			int level = atoi(c.attribute("access_level").value());
			if ( login_user_is_admin || level <= login_user_level ) out.push_back(c);
		}
		readable_boards(c, out);
	}
}

static std::string board_name(pugi::xml_node n)
{
	std::string s = trim(n.child_value("name"));
	std::string::size_type p = s.find(" (");
	if ( p != std::string::npos && p > 0 ) s = s.substr(0, p);
	return s.empty() ? n.attribute("id").value() : s;
}

// 게시판마다 where 조건으로 찾아 모은다 (게시판마다 limit 개까지)
static std::vector<found_article> collect(const std::string &where, int limit)
{
	std::vector<found_article> out;
	std::vector<pugi::xml_node> boards;
	if ( menu_root ) readable_boards(menu_root, boards);
	for ( unsigned int i = 0; i < boards.size(); i++ ) {
		std::string table = boards[i].attribute("id").value();
		char lim[16];
		snprintf(lim, sizeof(lim), "%d", limit);
		std::string q = "SELECT NO, USER_ID, DATE_TIME, TITLE, HIT FROM " + table + " WHERE " + where +
			" ORDER BY NO DESC LIMIT " + lim;
		std::vector<std::map<std::string, std::string> > r = database::fetch_rows((char*)q.c_str());
		for ( unsigned int k = 0; k < r.size(); k++ ) {
			found_article a;
			a.table = table;
			a.board = board_name(boards[i]);
			a.no = atoi(r[k]["NO"].c_str());
			a.user_id = r[k]["USER_ID"];
			a.date_time = r[k]["DATE_TIME"];
			a.title = r[k]["TITLE"];
			a.hit = atoi(r[k]["HIT"].c_str());
			a.node = boards[i];
			out.push_back(a);
		}
	}
	std::sort(out.begin(), out.end(), newer_first);
	return out;
}

// 그 글 읽기 (게시판의 첨부/답글 설정을 따라)
static void open_article(const found_article &a)
{
	bool attachment = strcasecmp(a.node.child_value("attachment"), "no") != 0;
	bool reply = !strcasecmp(a.node.child_value("reply"), "yes");
	bool is_dir;
	std::string name = a.node.child_value("name");
	show_article((char*)a.table.c_str(), 1, 1, (char*)name.c_str(), a.no, attachment, reply, &is_dir);
}

static void article_list(const std::string &head, const std::string &empty_msg, std::vector<found_article> &list)
{
	unsigned int page = 0;
	const unsigned int per = 15;
	while ( 1 ) {
		unsigned int pages = list.empty() ? 1 : (list.size() + per - 1) / per;
		if ( page >= pages ) page = pages - 1;

		printf(ESC_CLEAR);
		printf("\033[1;1H");
		printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
		printf("\033[1;1H");
		printf("\033[1A\033[7m%s\033[0m", host_name);
		printf("\033[2;1H\r\033[%dC%s", (int)(80 - strlen(head.c_str())) / 2, head.c_str());
		char pg[64];
		snprintf(pg, sizeof(pg), "%d/%d (총 %d건)", page + 1, pages, (int)list.size());
		printf("\r\033[%dC%s", (int)(79 - strlen(pg)), pg);
		printf("\033[3;1H");
		printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
		printf("\033[4;1H");
		// 3 + 1 + 게시판 14 + 1 + 작성자 10 + 1 + 날짜 11 + 1 + 제목 35 = 77 칸
		printf("%3s %-14s %-10s %-11s %s\r\n", "", "게시판", "작성자", "날짜", "제목");
		printf("%s\r\n", repeat("─", 40).c_str());
		if ( list.empty() ) printf("%s\r\n", centered(empty_msg.c_str(), 80).c_str());
		for ( unsigned int i = page * per; i < list.size() && i < (page + 1) * per; i++ ) {
			const found_article &a = list[i];
			std::string d = a.date_time.size() >= 16 ? a.date_time.substr(5, 11) : a.date_time;
			int cc = comment_count(a.table.c_str(), a.no);
			std::string cs = cc > 0 ? " [" + TO_STRING(cc) + "]" : "";
			printf("%3d " X_C "%-14s" X_W " %-10s " X_G "%-11s" X_W " %s" X_C "%s" X_W "\r\n", i + 1,
					string_truncate(a.board, 14, "").c_str(), string_truncate(nick_or_id(a.user_id), 10, "").c_str(),
					d.c_str(), string_truncate(display_text(a.title), 35 - cs.size(), "").c_str(), cs.c_str());
		}
		printf("%s\r\n", repeat("━", 40).c_str());

		char cmd[32];
		printf(ESC_ENG);
		printf("읽기(번호) 다음(Enter/N) 이전(B) 나가기(P) >> ");
		line_input(cmd, 10);
		std::string c = trim(cmd);
		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "q") || !strcasecmp(c.c_str(), "x") ) return;
		if ( c.empty() || !strcasecmp(c.c_str(), "n") ) {
			if ( page + 1 < pages ) page++;
			else if ( c.empty() ) return;		// 마지막 쪽에서 Enter 면 나가기
		} else if ( !strcasecmp(c.c_str(), "b") ) {
			if ( page > 0 ) page--;
		} else if ( is_number((char*)c.c_str()) ) {
			int n = atoi(c.c_str());
			if ( n >= 1 && n <= (int)list.size() ) open_article(list[n - 1]);
		}
	}
}

// NEW : 지난번 접속 이후 새 글 (처음이면 최근 하루, 길어도 30 일)
void show_new_articles(void)
{
	bool exist;
	std::map<std::string, std::string> u = database::user_info(login_user_id, &exist);
	std::string since = u["LASTLOGIN_DATETIME"];
	std::string where;
	if ( since.size() >= 10 && since.compare(0, 4, "0000") != 0 ) {
		where = "DATE_TIME > GREATEST('" + database::escape(since.c_str()) + "', NOW() - INTERVAL 30 DAY)";
	} else {
		where = "DATE_TIME > NOW() - INTERVAL 1 DAY";
	}
	// 내 글은 빼고
	where += " AND USER_ID <> '" + database::escape(login_user_id) + "'";
	printf("\r\n새 글을 모으는 중입니다...");
	fflush(stdout);
	std::vector<found_article> list = collect(where, 100);
	std::string head = since.size() >= 16 ? "새 글 모아보기 (" + since.substr(5, 11) + " 이후)" : "새 글 모아보기 (최근 하루)";
	article_list(head, "지난번 접속 이후 올라온 새 글이 없습니다.", list);
}

// FIND 단어 : 모든 게시판의 제목/본문,  FIND @닉네임(아이디) : 글쓴이
void search_all_boards(std::string word)
{
	word = trim(word);
	if ( word.empty() ) {
		char buf[64];
		printf(ESC_HAN);
		printf("\r\n찾을 말 (@닉네임 이면 글쓴이로) >> ");
		line_input(buf, 40);
		printf(ESC_ENG);
		word = trim(buf);
		if ( word.empty() ) return;
	}

	std::string where, head;
	if ( word[0] == '@' ) {
		std::string who = word.substr(1);
		bool exist;
		std::string id;
		database::user_info((char*)who.c_str(), &exist);
		if ( exist ) id = who;
		else {
			std::map<std::string, std::string> u = database::user_info_by_nick_name((char*)who.c_str(), &exist);
			if ( exist ) id = u["USER_ID"];
		}
		if ( id.empty() ) {
			printf("\r\n'%s' 회원이 없습니다.\r\n[Enter] 를 누르세요.", string_truncate(display_text(who), 20, "").c_str());
			press_enter();
			return;
		}
		where = "USER_ID='" + database::escape(id.c_str()) + "'";
		head = "글쓴이 찾기: " + nick_or_id(id);
	} else {
		std::string w = database::escape(word.c_str());
		where = "(TITLE LIKE '%" + w + "%' OR CONTENT LIKE '%" + w + "%')";
		head = "모든 게시판에서 찾기: " + string_truncate(display_text(word), 30, "");
	}
	printf("\r\n모든 게시판에서 찾는 중입니다...");
	fflush(stdout);
	std::vector<found_article> list = collect(where, 50);
	article_list(head, "찾은 글이 없습니다.", list);
}
