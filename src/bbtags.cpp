#include "main.h"

// ------------------------------------------------------------------
// txt 파일(대문 등)에 쓰는 [태그] 중 BBS(main) 에서만 알 수 있는 값
//   textutil.cpp 의 replace_bbcode 가 모르는 태그를 bbcode_tag_hook 으로 여기에 묻는다.
//   태그 목록은 docs/txt_tags.md
// 여러 접속자가 대문을 열 때마다 DB 를 뒤지지 않도록, 모두에게 같은 값은
// 60 초 동안 tmp/tags.cache 에 캐시해서 함께 쓴다. 로그인한 회원 본인의 값은 그때그때.
// ------------------------------------------------------------------

// 값 하나 (오류가 나거나 없으면 빈 문자열, 화면에 안 보이는 글자는 뺀다)
static std::string q1(const std::string &q)
{
	bool ok;
	std::string v = database::fetch((char*)q.c_str(), &ok);
	return ok ? display_text(v) : "";
}

static std::string num_or_zero(const std::string &v)
{
	return v.empty() ? "0" : v;
}

// 메뉴에서 누구나 볼 수 있는(access_level 1) 게시판들과, go="notice" 게시판
static void public_boards(pugi::xml_node node, std::vector<std::string> &list, std::string &notice)
{
	for ( pugi::xml_node c = node.first_child(); c; c = c.next_sibling() ) {
		if ( c.type() != pugi::node_element ) continue;
		if ( !strcmp(c.attribute("type").value(), "board") && !c.attribute("id").empty() ) {
			int level = c.attribute("access_level").empty() ? 1 : atoi(c.attribute("access_level").value());
			if ( level <= 1 ) list.push_back(c.attribute("id").value());
			if ( notice.empty() && !strcmp(c.attribute("go").value(), "notice") ) notice = c.attribute("id").value();
		}
		public_boards(c, list, notice);
	}
}

// 모두에게 같은 값들을 새로 계산
static void compute_shared(std::map<std::string, std::string> &v)
{
	v["today_visitors"] = num_or_zero(q1("SELECT COUNT(*) FROM attendance WHERE ATT_DAY = CURDATE()"));
	v["max_online"] = num_or_zero(q1("SELECT MAX_ONLINE FROM stat_online WHERE DAY = CURDATE()"));
	v["max_online_ever"] = num_or_zero(q1("SELECT MAX(MAX_ONLINE) FROM stat_online"));
	v["today_members"] = num_or_zero(q1("SELECT COUNT(*) FROM member WHERE REGISTRATION_DATETIME >= CURDATE()"));
	v["newest_member"] = q1("SELECT NICK_NAME FROM member ORDER BY REGISTRATION_DATETIME DESC LIMIT 1");
	v["graffiti"] = q1("SELECT CONCAT(COALESCE(m.NICK_NAME, g.USER_ID), ': ', g.TEXT) FROM graffiti g "
			"LEFT JOIN member m ON m.USER_ID = g.USER_ID ORDER BY g.NO DESC LIMIT 1");
	v["birthdays"] = q1("SELECT GROUP_CONCAT(NICK_NAME ORDER BY NICK_NAME SEPARATOR ', ') FROM member WHERE "
			"(MONTH(BIRTHDAY) = MONTH(CURDATE()) AND DAYOFMONTH(BIRTHDAY) = DAYOFMONTH(CURDATE())) "
			"OR (MONTH(BIRTHDAY) = 2 AND DAYOFMONTH(BIRTHDAY) = 29 AND MONTH(CURDATE()) = 2 "
			"    AND DAYOFMONTH(CURDATE()) = 28 AND DAYOFMONTH(LAST_DAY(CURDATE())) = 28)");
	v["hero_champ"] = q1("SELECT NAME FROM game_hero ORDER BY WINS DESC, LEVEL DESC, EXP DESC LIMIT 1");
	v["rain_champ"] = q1("SELECT CONCAT(NAME, ' (', BEST, ')') FROM game_rain WHERE MODE = 0 ORDER BY BEST DESC, DATE_TIME LIMIT 1");

	// 최근 공지, 이번 주(7 일) 가장 많이 읽힌 글: 누구나 볼 수 있는 게시판만
	std::vector<std::string> boards;
	std::string notice;
	pugi::xml_document doc;
	if ( doc.load_file("hanulso.mnu") ) public_boards(doc.child("hanulso"), boards, notice);
	if ( notice.empty() ) notice = "bbs_notice";
	v["last_notice"] = q1("SELECT TITLE FROM " + notice + " ORDER BY NO DESC LIMIT 1");

	long best = -1;
	for ( unsigned int i = 0; i < boards.size(); i++ ) {
		bool ok;
		std::string r = database::fetch((char*)("SELECT CONCAT(HIT, ' ', TITLE) FROM " + boards[i] +
					" WHERE DATE_TIME >= NOW() - INTERVAL 7 DAY ORDER BY HIT DESC LIMIT 1").c_str(), &ok);
		if ( !ok || r.empty() ) continue;
		long hit = atol(r.c_str());
		std::string::size_type sp = r.find(' ');
		if ( hit > best && sp != std::string::npos ) {
			best = hit;
			v["hot_article"] = display_text(r.substr(sp + 1));
		}
	}

	// 음력 날짜와 오늘의 공휴일/명절 (bin/lunar --today : "음력 8월 25일<TAB>추석")
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "%s/bin/lunar --today", getenv("HANULSO"));
	bool ok;
	std::vector<std::string> lines = exec_command(cmd, &ok);
	if ( lines.size() > 0 ) {
		std::string l = lines[0];
		std::string::size_type tab = l.find('\t');
		v["lunar_date"] = display_text(l.substr(0, tab));
		v["holiday"] = tab != std::string::npos ? display_text(l.substr(tab + 1)) : "";
	}
}

// 모두에게 같은 값들 (60 초 캐시)
static std::map<std::string, std::string> &shared_values(void)
{
	static std::map<std::string, std::string> v;
	static time_t loaded = 0;
	if ( loaded && time(NULL) - loaded < 10 ) return v;		// 한 화면에 태그가 여러 개여도 파일은 한 번
	loaded = time(NULL);

	char path[1024];
	snprintf(path, sizeof(path), "%s/tmp/tags.cache", getenv("HANULSO"));
	struct stat st;
	v.clear();
	if ( stat(path, &st) == 0 && time(NULL) - st.st_mtime < 60 ) {
		std::vector<std::string> lines = split_string(read_file(path), '\n');
		for ( unsigned int i = 0; i < lines.size(); i++ ) {
			std::string::size_type tab = lines[i].find('\t');
			if ( tab != std::string::npos ) v[lines[i].substr(0, tab)] = lines[i].substr(tab + 1);
		}
		if ( v.size() > 0 ) return v;
	}

	compute_shared(v);

	// 다른 프로세스가 읽는 중에 반쯤 쓴 파일을 보지 않도록 임시 파일에 쓰고 이름을 바꾼다
	char tmp[1100];
	snprintf(tmp, sizeof(tmp), "%s.%d", path, (int)getpid());
	FILE *fp = fopen(tmp, "w");
	if ( fp != NULL ) {
		for ( std::map<std::string, std::string>::iterator it = v.begin(); it != v.end(); ++it ) {
			fprintf(fp, "%s\t%s\n", it->first.c_str(), it->second.c_str());
		}
		fclose(fp);
		rename(tmp, path);
	}
	return v;
}

// 로그인한 회원 본인의 값
static bool user_value(const std::string &name, std::string &out)
{
	static const char *names[] = { "nick", "user_id", "level", "new_memo", "streak", "last_login", NULL };
	bool mine = false;
	for ( int i = 0; names[i]; i++ ) if ( name == names[i] ) mine = true;
	if ( !mine ) return false;

	out = "";
	if ( login_user_id[0] == 0 ) return true;		// 로그인 전
	std::string id = database::escape(login_user_id);

	if ( name == "user_id" ) out = display_text(login_user_id);
	else if ( name == "nick" ) out = q1("SELECT NICK_NAME FROM member WHERE USER_ID='" + id + "'");
	else if ( name == "level" ) out = login_user_is_admin ? "시숍" : get_level_name(login_user_level);
	else if ( name == "new_memo" ) out = num_or_zero(q1("SELECT COUNT(*) FROM memo WHERE RECIPIENT_USER_ID='" + id +
				"' AND RECIPIENT_DELETED=0 AND CONFIRMATION_DATETIME IS NULL"));
	else if ( name == "streak" ) out = num_or_zero(q1("SELECT STREAK FROM attendance_stat WHERE USER_ID='" + id +
				"' AND LAST_DATE >= CURDATE() - INTERVAL 1 DAY"));
	else if ( name == "last_login" ) {
		// 접속을 끝낼 때 기록하므로 접속해 있는 동안은 지난번 접속 시각
		out = q1("SELECT DATE_FORMAT(LASTLOGIN_DATETIME, '%Y-%m-%d %H:%i') FROM member WHERE USER_ID='" + id + "'");
	}
	return true;
}

// replace_bbcode 가 모르는 태그. 모르는 이름이면 found = false (태그를 그대로 둔다)
std::string bbtag_value(const std::string &name, bool *found)
{
	std::string out;
	*found = true;
	if ( user_value(name, out) ) return out;

	std::map<std::string, std::string> &v = shared_values();
	std::map<std::string, std::string>::iterator it = v.find(name);
	if ( it != v.end() ) return it->second;

	// 캐시를 만들 때 값이 없던 태그 (게임 기록이 아직 없는 등)
	static const char *known[] = { "today_visitors", "max_online", "max_online_ever", "today_members",
		"newest_member", "graffiti", "birthdays", "hero_champ", "rain_champ", "last_notice",
		"hot_article", "lunar_date", "holiday", NULL };
	for ( int i = 0; known[i]; i++ ) if ( name == known[i] ) return "";

	*found = false;
	return "";
}
