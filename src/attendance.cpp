#include "main.h"

// ------------------------------------------------------------------
// 출석 체크 / 생일 축하
//   로그인할 때 하루 첫 접속이면 출석을 기록하고 (연속/총 출석),
//   로그인 화면에 출석과 오늘 생일인 회원을 보여 준다.
//   본인 생일이면 축하 화면을 띄운다.
//   AT 명령: 출석부 (오늘 출석한 회원, 내 기록, 연속 출석 순위)
// ------------------------------------------------------------------

#define A_WHITE		"\033[=15F"
#define A_YELLOW	"\033[=14F"
#define A_RED		"\033[=12F"
#define A_CYAN		"\033[=11F"
#define A_GREEN		"\033[=10F"
#define A_MAGENTA	"\033[=13F"
#define A_GRAY		"\033[=7F"

static void create_attendance_tables(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS attendance ( "
			"USER_ID VARCHAR(50) NOT NULL, "
			"ATT_DAY DATE NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"PRIMARY KEY (USER_ID, ATT_DAY), KEY IDX_DAY (ATT_DAY) )");
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS attendance_stat ( "
			"USER_ID VARCHAR(50) NOT NULL PRIMARY KEY, "
			"LAST_DATE DATE NOT NULL, "
			"STREAK INT NOT NULL, "
			"BEST INT NOT NULL, "
			"TOTAL INT NOT NULL )");
}

static int query_int(const std::string &q)
{
	bool ok;
	std::string v = database::fetch((char*)q.c_str(), &ok);
	return ok ? atoi(v.c_str()) : 0;
}

// 오늘 출석을 기록한다. 이미 했으면 false. rank 는 오늘 몇 번째인지
static bool check_in(const char *user_id, int *rank)
{
	std::string id = database::escape(user_id);

	std::string q = "INSERT IGNORE INTO attendance (USER_ID, ATT_DAY, DATE_TIME) VALUES ('" + id + "', CURDATE(), NOW())";
	if ( mysql_query(mysql, q.c_str()) != 0 ) return false;
	bool first = mysql_affected_rows(mysql) > 0;

	if ( first ) {
		// 어제 출석했으면 연속 +1, 아니면 1 부터
		q = "INSERT INTO attendance_stat (USER_ID, LAST_DATE, STREAK, BEST, TOTAL) VALUES ('" + id + "', CURDATE(), 1, 1, 1) "
			"ON DUPLICATE KEY UPDATE "
			"STREAK = IF(LAST_DATE = CURDATE() - INTERVAL 1 DAY, STREAK + 1, 1), "
			"BEST = GREATEST(BEST, STREAK), "
			"TOTAL = TOTAL + 1, "
			"LAST_DATE = CURDATE()";
		mysql_query(mysql, q.c_str());
	}

	*rank = query_int("SELECT COUNT(*) FROM attendance WHERE ATT_DAY = CURDATE() AND DATE_TIME <= "
			"(SELECT DATE_TIME FROM (SELECT DATE_TIME FROM attendance WHERE USER_ID='" + id + "' AND ATT_DAY=CURDATE()) t)");
	return first;
}

// 생일(YYYY-MM-DD) 이 오늘인지. 2/29 생일은 윤년이 아니면 2/28 에 축하
bool is_birthday_today(const std::string &birthday)
{
	int y, m, d;
	if ( sscanf(birthday.c_str(), "%d-%d-%d", &y, &m, &d) != 3 ) return false;

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	int ty = tm->tm_year + 1900, tmon = tm->tm_mon + 1, td = tm->tm_mday;
	bool leap = (ty % 4 == 0 && ty % 100 != 0) || ty % 400 == 0;

	if ( m == 2 && d == 29 && !leap ) return tmon == 2 && td == 28;
	return m == tmon && d == td;
}

// 로그인 화면에 출석 / 오늘 생일인 회원
void attendance_login_info(const char *user_id)
{
	create_attendance_tables();

	int rank = 0;
	bool first = check_in(user_id, &rank);

	std::string id = database::escape(user_id);
	std::vector<std::map<std::string, std::string> > r = database::fetch_rows(
			(char*)("SELECT * FROM attendance_stat WHERE USER_ID='" + id + "'").c_str());
	int streak = 0, total = 0;
	if ( r.size() > 0 ) {
		streak = atoi(r[0]["STREAK"].c_str());
		total = atoi(r[0]["TOTAL"].c_str());
	}

	if ( first ) {
		printf("\r\n [출    석] : " A_YELLOW "출석 도장 쾅! 오늘 %d번째 출석" A_WHITE " / 연속 %d일 / 총 %d일 (AT 로 출석부)", rank, streak, total);
	} else {
		printf("\r\n [출    석] : 오늘 출석 완료 (%d번째) / 연속 %d일 / 총 %d일 (AT 로 출석부)", rank, streak, total);
	}

	// 오늘 생일인 회원
	std::vector<std::map<std::string, std::string> > b = database::fetch_rows((char*)
			"SELECT USER_ID, NICK_NAME, BIRTHDAY FROM member WHERE "
			"(MONTH(BIRTHDAY) = MONTH(CURDATE()) AND DAYOFMONTH(BIRTHDAY) = DAYOFMONTH(CURDATE())) "
			"OR (MONTH(BIRTHDAY) = 2 AND DAYOFMONTH(BIRTHDAY) = 29 AND MONTH(CURDATE()) = 2 "
			"    AND DAYOFMONTH(CURDATE()) = 28 AND DAYOFMONTH(LAST_DAY(CURDATE())) = 28) "
			"ORDER BY NICK_NAME LIMIT 8");
	if ( b.size() > 0 ) {
		std::string names;
		for ( unsigned int i = 0; i < b.size(); i++ ) {
			std::string n = display_text(b[i]["NICK_NAME"]);
			if ( n.empty() ) n = display_text(b[i]["USER_ID"]);
			if ( n.empty() ) continue;
			if ( !names.empty() ) names += ", ";
			names += n;
		}
		if ( !names.empty() ) {
			printf("\r\n [생    일] : " A_MAGENTA "오늘 생일인 회원 %s" A_WHITE " 축하해 주세요!",
					string_truncate(names, 44, "..").c_str());
		}
	}
}

static const char *cake[] = {
	A_YELLOW "              *     *     *     *     *",
	A_RED    "              |     |     |     |     |",
	A_WHITE  "          ____" A_RED "|" A_WHITE "_____" A_RED "|" A_WHITE "_____" A_RED "|" A_WHITE "_____" A_RED "|" A_WHITE "_____" A_RED "|" A_WHITE "____",
	A_WHITE  "         |" A_MAGENTA "  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~  ~ " A_WHITE "|",
	A_WHITE  "         |" A_YELLOW "        HAPPY   BIRTHDAY   TO   YOU    " A_WHITE "|",
	A_WHITE  "       __|______________________________________|__",
	A_WHITE  "      |" A_CYAN "  o   o   o   o   o   o   o   o   o   o   o   " A_WHITE "|",
	A_WHITE  "      |______________________________________________|",
	A_GRAY   "   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
	NULL
};

// 본인 생일이면 축하 화면
void birthday_celebrate(const char *nick)
{
	printf(ESC_CLEAR);
	printf("\r\n\r\n");
	for ( int i = 0; cake[i]; i++ ) {
		printf("%s" A_WHITE "\r\n", cake[i]);
	}
	printf("\r\n\r\n");
	printf("          " A_YELLOW "%s" A_WHITE " 님, 생일을 진심으로 축하합니다!\r\n\r\n", nick);
	printf("          오늘 하루 행복한 일만 가득하길 바랍니다.\r\n");
	printf("          " A_GRAY "- %s 가족 일동 -" A_WHITE "\r\n", host_name);
	printf("\r\n\r\n [Enter] 를 누르세요.");
	press_enter();
}

// AT : 출석부
void show_attendance(const char *user_id)
{
	create_attendance_tables();

	printf(ESC_CLEAR);
	printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
	printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	char head[64];
	snprintf(head, sizeof(head), "출석부 - %d월 %d일", tm->tm_mon + 1, tm->tm_mday);
	printf("\033[2;1H\r\033[%dC%s", (int)(80 - strlen(head)) / 2, head);
	printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
	printf("\033[4;1H");

	// 내 기록
	std::string id = database::escape(user_id);
	std::vector<std::map<std::string, std::string> > me = database::fetch_rows(
			(char*)("SELECT * FROM attendance_stat WHERE USER_ID='" + id + "'").c_str());
	if ( me.size() > 0 ) {
		printf("  " A_CYAN "내 출석" A_WHITE "  연속 " A_YELLOW "%s일" A_WHITE "  최고 연속 %s일  총 %s일\r\n",
				me[0]["STREAK"].c_str(), me[0]["BEST"].c_str(), me[0]["TOTAL"].c_str());
	}

	// 오늘 출석한 회원 (먼저 온 순서, 세 단)
	std::vector<std::map<std::string, std::string> > today = database::fetch_rows((char*)
			"SELECT a.USER_ID, a.DATE_TIME, m.NICK_NAME FROM attendance a LEFT JOIN member m ON m.USER_ID = a.USER_ID "
			"WHERE a.ATT_DAY = CURDATE() ORDER BY a.DATE_TIME LIMIT 30");
	printf("\r\n  " A_GREEN "오늘 출석 %d명" A_WHITE "\r\n", (int)today.size());
	for ( unsigned int i = 0; i < today.size(); i++ ) {
		std::string n = display_text(today[i]["NICK_NAME"]);
		if ( n.empty() ) n = display_text(today[i]["USER_ID"]);
		std::string time_s = today[i]["DATE_TIME"].size() >= 16 ? today[i]["DATE_TIME"].substr(11, 5) : "";
		printf("  %2d. " A_GRAY "%s" A_WHITE " %-14s", i + 1, time_s.c_str(), string_truncate(n, 14, "").c_str());
		if ( (i + 1) % 3 == 0 ) printf("\r\n");
	}
	if ( today.size() % 3 != 0 ) printf("\r\n");

	// 연속 출석 순위 (오늘이나 어제까지 이어지는 것만)
	std::vector<std::map<std::string, std::string> > rank = database::fetch_rows((char*)
			"SELECT s.USER_ID, s.STREAK, s.TOTAL, m.NICK_NAME FROM attendance_stat s LEFT JOIN member m ON m.USER_ID = s.USER_ID "
			"WHERE s.LAST_DATE >= CURDATE() - INTERVAL 1 DAY ORDER BY s.STREAK DESC, s.TOTAL DESC LIMIT 5");
	printf("\r\n  " A_MAGENTA "연속 출석 순위" A_WHITE "\r\n");
	for ( unsigned int i = 0; i < rank.size(); i++ ) {
		std::string n = display_text(rank[i]["NICK_NAME"]);
		if ( n.empty() ) n = display_text(rank[i]["USER_ID"]);
		printf("  %d위  %-14s  연속 " A_YELLOW "%s일" A_WHITE "  (총 %s일)\r\n", i + 1,
				string_truncate(n, 14, "").c_str(), rank[i]["STREAK"].c_str(), rank[i]["TOTAL"].c_str());
	}

	printf("\r\n [Enter] 를 누르세요.");
	press_enter();
}
