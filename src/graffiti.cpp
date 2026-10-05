#include "main.h"

// ------------------------------------------------------------------
// 한줄 낙서장
//   로그인 화면에 가장 최근 낙서 한 줄을 보여 주고,
//   NS 명령으로 최근 낙서 15 줄을 보고 한 줄씩 남긴다.
// ------------------------------------------------------------------

#define G_WHITE		"\033[=15F"
#define G_YELLOW	"\033[=14F"
#define G_CYAN		"\033[=11F"
#define G_GREEN		"\033[=10F"
#define G_GRAY		"\033[=7F"

#define GRAFFITI_LEN	50		// 낙서 한 줄 길이 (바이트, 한글 25 자)
#define GRAFFITI_SHOW	15

static void create_graffiti_table(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS graffiti ( "
			"NO INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
			"USER_ID VARCHAR(50) NOT NULL, "
			"TEXT VARCHAR(255) NOT NULL, "
			"DATE_TIME DATETIME NOT NULL )");
}

static std::string nick_or_id(std::map<std::string, std::string> &r)
{
	std::string n = display_text(r["NICK_NAME"]);
	if ( n.empty() ) n = display_text(r["USER_ID"]);
	return n;
}

// 로그인 화면: 가장 최근 낙서
void graffiti_login_info(void)
{
	create_graffiti_table();
	std::vector<std::map<std::string, std::string> > r = database::fetch_rows((char*)
			"SELECT g.USER_ID, g.TEXT, m.NICK_NAME FROM graffiti g LEFT JOIN member m ON m.USER_ID = g.USER_ID "
			"ORDER BY g.NO DESC LIMIT 1");
	if ( r.size() == 0 ) return;

	// " [낙 서 장] : " 14 칸 + 닉네임 + ": " + 낙서 <= 79
	std::string nick = string_truncate(nick_or_id(r[0]), 12, "");
	std::string text = string_truncate(display_text(r[0]["TEXT"]), 79 - 14 - (int)nick.size() - 2, "..");
	printf("\r\n [낙 서 장] : " G_CYAN "%s" G_WHITE ": %s", nick.c_str(), text.c_str());
}

// NS : 낙서장
void show_graffiti(const char *user_id, bool is_admin)
{
	create_graffiti_table();
	std::string id = database::escape(user_id);
	std::string notice;

	while ( 1 ) {
		printf(ESC_CLEAR);
		printf("\033[1;1H");
		printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
		printf("\033[1;1H");
		printf("\033[1A\033[7m%s\033[0m", host_name);
		const char *head = "한줄 낙서장";
		printf("\033[2;1H\r\033[%dC%s", (int)(80 - strlen(head)) / 2, head);
		printf("\033[3;1H");
		printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
		printf("\033[4;1H");

		// 최근 15 줄 (오래된 것부터 아래로)
		std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)
				"SELECT * FROM (SELECT g.NO, g.USER_ID, g.TEXT, g.DATE_TIME, m.NICK_NAME FROM graffiti g "
				"LEFT JOIN member m ON m.USER_ID = g.USER_ID ORDER BY g.NO DESC LIMIT 15) t ORDER BY NO");
		if ( rows.size() == 0 ) {
			printf("\r\n  " G_GRAY "아직 낙서가 없습니다. 첫 낙서를 남겨 보세요!" G_WHITE "\r\n");
		}
		for ( unsigned int i = 0; i < rows.size(); i++ ) {
			std::string t = rows[i]["DATE_TIME"];
			std::string date = t.size() >= 10 ? t.substr(5, 2) + "/" + t.substr(8, 2) : "";
			bool mine = (rows[i]["USER_ID"] == user_id);
			// 2 + 번호 2 + 1 + 날짜 5 + 1 + 닉네임 12 + 1 + 낙서 50 = 74 칸
			printf("  " G_GRAY "%2d %s" " %s%-12s" G_WHITE " %s\r\n", i + 1, date.c_str(),
					mine ? G_YELLOW : G_CYAN, string_truncate(nick_or_id(rows[i]), 12, "").c_str(),
					string_truncate(display_text(rows[i]["TEXT"]), GRAFFITI_LEN, "").c_str());
		}

		printf("\033[21;1H");
		printf(G_GRAY " %s" G_WHITE "\r\n", repeat("─", 39).c_str());
		if ( !notice.empty() ) {
			printf(" " G_YELLOW "%s" G_WHITE, notice.c_str());
			notice.clear();
		} else {
			printf(" " G_GRAY "한 줄 남기고 Enter.  그냥 Enter: 나가기   DD {번호}: 내 낙서 지우기" G_WHITE);
		}
		printf("\033[23;1H\033[K");
		printf(ESC_ENG);
		printf(" 낙서 >> ");

		char buf[GRAFFITI_LEN + 8];
		line_input(buf, GRAFFITI_LEN);
		std::string text = trim(display_text(buf));
		if ( text.empty() ) return;

		// DD 번호 : 내 낙서 지우기 (운영자는 아무거나)
		if ( (text.size() > 3 && !strncasecmp(text.c_str(), "dd ", 3)) ) {
			int n = atoi(text.c_str() + 3);
			if ( n < 1 || n > (int)rows.size() ) {
				notice = "번호를 확인하세요.";
				continue;
			}
			if ( rows[n - 1]["USER_ID"] != user_id && !is_admin ) {
				notice = "내 낙서만 지울 수 있습니다.";
				continue;
			}
			std::string q = "DELETE FROM graffiti WHERE NO=" + rows[n - 1]["NO"];
			mysql_query(mysql, q.c_str());
			notice = "지웠습니다.";
			continue;
		}

		// 도배 막기: 10 초에 한 번
		bool ok;
		std::string recent = database::fetch((char*)("SELECT COUNT(*) FROM graffiti WHERE USER_ID='" + id
					+ "' AND DATE_TIME > NOW() - INTERVAL 10 SECOND").c_str(), &ok);
		if ( atoi(recent.c_str()) > 0 ) {
			notice = "잠시 후에 다시 남겨 주세요.";
			continue;
		}

		std::string q = "INSERT INTO graffiti (USER_ID, TEXT, DATE_TIME) VALUES ('" + id + "', '"
			+ database::escape(text.c_str()) + "', NOW())";
		mysql_query(mysql, q.c_str());
	}
}
