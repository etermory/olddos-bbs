#include "main.h"

// ------------------------------------------------------------------
// 전보 : 지금 접속해 있는 회원 화면에 바로 한 줄을 띄운다
//   TO {아이디/닉네임} [내용]   보내기
//   TO OFF / TO ON             전보 받지 않기 / 받기
// 보낼 때 DB 에 넣고 받는 사람의 BBS 프로세스에 SIGUSR1 을 보낸다.
// 받는 쪽은 프롬프트에서 입력을 기다리는 동안 바로 띄우고,
// 게임/대화방 같은 다른 화면에 있었으면 프롬프트로 돌아왔을 때 띄운다.
// (시그널이 닿지 않는 경우를 위해 프롬프트에서는 20 초마다 DB 도 확인)
// ------------------------------------------------------------------

#define T_WHITE		"\033[=15F"
#define T_YELLOW	"\033[=14F"
#define T_CYAN		"\033[=11F"
#define T_GRAY		"\033[=7F"
#define T_RED		"\033[=12F"

static volatile sig_atomic_t telegram_signal = 0;
static time_t last_poll = 0;
static std::string live_prompt;

static void on_telegram_signal(int)
{
	telegram_signal = 1;
}

void telegram_init(void)
{
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS telegram ( "
			"NO INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, "
			"FROM_USER_ID VARCHAR(50) NOT NULL, "
			"TO_USER_ID VARCHAR(50) NOT NULL, "
			"TEXT VARCHAR(255) NOT NULL, "
			"DATE_TIME DATETIME NOT NULL, "
			"DELIVERED INT NOT NULL DEFAULT 0, "
			"KEY IDX_TO (TO_USER_ID, DELIVERED) )");
	mysql_query(mysql, "CREATE TABLE IF NOT EXISTS telegram_refuse ( "
			"USER_ID VARCHAR(50) NOT NULL PRIMARY KEY )");
	// 일주일 지난 전보는 지운다
	mysql_query(mysql, "DELETE FROM telegram WHERE DATE_TIME < NOW() - INTERVAL 7 DAY");

	// 읽기(read) 도중에 와도 그대로 다시 읽도록 SA_RESTART
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_telegram_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);
}

// 아직 못 받은 전보
static std::vector<std::map<std::string, std::string> > fetch_telegrams(void)
{
	std::string q = "SELECT t.NO, t.FROM_USER_ID, t.TEXT, t.DATE_TIME, m.NICK_NAME FROM telegram t "
		"LEFT JOIN member m ON m.USER_ID = t.FROM_USER_ID "
		"WHERE t.TO_USER_ID='" + database::escape(login_user_id) + "' AND t.DELIVERED=0 ORDER BY t.NO LIMIT 10";
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)q.c_str());
	if ( rows.size() > 0 ) {
		q = "UPDATE telegram SET DELIVERED=1 WHERE TO_USER_ID='" + database::escape(login_user_id)
			+ "' AND DELIVERED=0 AND NO <= " + rows[rows.size() - 1]["NO"];
		mysql_query(mysql, q.c_str());
	}
	return rows;
}

// 전보를 두 줄씩 (보낸 사람/시각, 내용) 출력
static void print_telegrams(std::vector<std::map<std::string, std::string> > &rows)
{
	for ( unsigned int i = 0; i < rows.size(); i++ ) {
		std::string nick = display_text(rows[i]["NICK_NAME"]);
		std::string id = display_text(rows[i]["FROM_USER_ID"]);
		std::string t = rows[i]["DATE_TIME"].size() >= 16 ? rows[i]["DATE_TIME"].substr(11, 5) : "";
		if ( nick.empty() ) nick = id;
		printf(T_YELLOW "★ 전보" T_WHITE " ─ " T_CYAN "%s" T_GRAY "(%s) %s" T_WHITE "\r\n",
				string_truncate(nick, 20, "").c_str(), string_truncate(id, 20, "").c_str(), t.c_str());
		printf("   %s\r\n", string_truncate(display_text(rows[i]["TEXT"]), 74, "").c_str());
	}
	if ( rows.size() > 0 ) {
		printf(T_GRAY "   (답장: TO %s 내용)" T_WHITE "\r\n",
				string_truncate(display_text(rows[rows.size() - 1]["FROM_USER_ID"]), 40, "").c_str());
	}
}

// 프롬프트를 띄우기 전: 다른 화면에 있는 동안 온 전보
void telegram_show_pending(void)
{
	telegram_signal = 0;
	last_poll = time(NULL);
	std::vector<std::map<std::string, std::string> > rows = fetch_telegrams();
	if ( rows.size() > 0 ) {
		printf("\r\n");
		print_telegrams(rows);
	}
}

// 입력을 기다리는 동안 (1 초마다) : 전보가 왔으면 입력 줄 위에 띄우고 입력 줄을 다시 그린다
static void telegram_wait_hook(const char *typed)
{
	time_t now = time(NULL);
	if ( !telegram_signal && now - last_poll < 20 ) return;
	telegram_signal = 0;
	last_poll = now;

	std::vector<std::map<std::string, std::string> > rows = fetch_telegrams();
	if ( rows.size() == 0 ) return;

	printf("\r\033[K");
	print_telegrams(rows);
	printf("%s%s", live_prompt.c_str(), typed);
	fflush(stdout);
}

void telegram_live_begin(const char *prompt_text)
{
	live_prompt = prompt_text;
	line_input_wait_hook = telegram_wait_hook;
}

void telegram_live_end(void)
{
	line_input_wait_hook = NULL;
}

// 그 아이디로 접속해 있는 BBS 프로세스들 (tmp/<tty>.tty : "아이디 pid")
static std::vector<int> online_pids(const std::string &user_id)
{
	std::vector<int> pids;
	char buf[1024];
	snprintf(buf, sizeof(buf), "%s/tmp/*.tty", getenv("HANULSO"));
	std::vector<std::string> files = find_files_time_sorted(buf);
	for ( unsigned int i = 0; i < files.size(); i++ ) {
		std::string id;
		if ( !read_tty_file(files[i], id) || id != user_id ) continue;
		char sid[256] = "";
		int pid = 0;
		sscanf(trim(read_file(files[i].c_str())).c_str(), "%255s %d", sid, &pid);
		if ( pid <= 0 ) continue;
		// 강제 종료로 남은 파일의 pid 를 다른 프로그램이 받았을 수 있으니 BBS(main) 인지 확인
		// (ctime 이 execl("bin/main", "main", ...) 으로 띄우므로 argv[0] 이 main)
		char path[64];
		snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
		std::string argv0 = read_file(path).c_str();
		std::string::size_type slash = argv0.rfind('/');
		if ( slash != std::string::npos ) argv0 = argv0.substr(slash + 1);
		if ( argv0 != "main" ) continue;
		pids.push_back(pid);
	}
	return pids;
}

static bool is_refused(const std::string &user_id)
{
	bool ok;
	std::string q = "SELECT COUNT(*) FROM telegram_refuse WHERE USER_ID='" + database::escape(user_id.c_str()) + "'";
	return atoi(database::fetch((char*)q.c_str(), &ok).c_str()) > 0;
}

static void done(void)
{
	printf("\r\n[Enter] 를 누르세요.");
	press_enter();
}

// TO 명령. cmd 는 프롬프트에 친 줄 전체
void telegram_command(const std::string &cmd)
{
	std::vector<std::string> args = split_string(cmd, ' ');
	std::string me = login_user_id;

	if ( args.size() > 1 && (!strcasecmp(args[1].c_str(), "off") || !strcasecmp(args[1].c_str(), "on")) ) {
		bool off = !strcasecmp(args[1].c_str(), "off");
		std::string q = off ?
			"INSERT IGNORE INTO telegram_refuse (USER_ID) VALUES ('" + database::escape(me.c_str()) + "')" :
			"DELETE FROM telegram_refuse WHERE USER_ID='" + database::escape(me.c_str()) + "'";
		mysql_query(mysql, q.c_str());
		printf("\r\n%s", off ? "이제 전보를 받지 않습니다. (TO ON 으로 다시 받기)" : "이제 전보를 받습니다.");
		done();
		return;
	}

	// 받는 사람
	std::string who;
	if ( args.size() > 1 ) {
		who = args[1];
	} else {
		char buf[64];
		printf(ESC_ENG);
		printf("\r\n받는 사람 (아이디/닉네임, 접속자는 US) >> ");
		line_input(buf, 40);
		who = trim(buf);
		if ( who.empty() ) return;
	}

	bool exist;
	std::string to;
	database::user_info((char*)who.c_str(), &exist);
	if ( exist ) {
		to = who;
	} else {
		std::map<std::string, std::string> u = database::user_info_by_nick_name((char*)who.c_str(), &exist);
		if ( exist ) to = u["USER_ID"];
	}
	if ( to.empty() ) {
		printf("\r\n'%s' 회원을 찾을 수 없습니다.", string_truncate(display_text(who), 40, "").c_str());
		done();
		return;
	}
	if ( to == me ) {
		printf("\r\n자기 자신에게는 보낼 수 없습니다.");
		done();
		return;
	}

	std::map<std::string, std::string> u = database::user_info((char*)to.c_str(), &exist);
	std::string nick = display_text(u["NICK_NAME"]);
	if ( nick.empty() ) nick = display_text(to);

	std::vector<int> pids = online_pids(to);
	if ( pids.size() == 0 ) {
		printf("\r\n%s 님은 지금 접속해 있지 않습니다. 쪽지를 보내 보세요. (MEMO %s)",
				nick.c_str(), display_text(to).c_str());
		done();
		return;
	}
	if ( is_refused(to) ) {
		printf("\r\n%s 님은 지금 전보를 받지 않습니다. 쪽지를 보내 보세요. (MEMO %s)",
				nick.c_str(), display_text(to).c_str());
		done();
		return;
	}

	// 내용 (명령 줄에 같이 썼으면 그것)
	std::string text;
	if ( args.size() > 2 ) {
		std::string::size_type p = cmd.find(args[1]);
		if ( p != std::string::npos ) text = trim(cmd.substr(p + args[1].size()));
	}
	if ( text.empty() ) {
		char buf[128];
		printf(ESC_ENG);
		printf("\r\n" T_CYAN "%s" T_WHITE " 님께 전보 (Enter: 취소)\r\n>> ", nick.c_str());
		line_input(buf, 70);
		text = trim(buf);
		if ( text.empty() ) return;
	}

	// 너무 자주 보내지 못하게
	static time_t last_send = 0;
	if ( time(NULL) - last_send < 3 ) {
		printf("\r\n잠시 후에 다시 보내 주세요.");
		done();
		return;
	}
	last_send = time(NULL);

	std::string q = "INSERT INTO telegram (FROM_USER_ID, TO_USER_ID, TEXT, DATE_TIME) VALUES ('"
		+ database::escape(me.c_str()) + "', '" + database::escape(to.c_str()) + "', '"
		+ database::escape(text.c_str()) + "', NOW())";
	if ( mysql_query(mysql, q.c_str()) != 0 ) {
		printf("\r\n전보를 보내지 못했습니다.");
		done();
		return;
	}
	for ( unsigned int i = 0; i < pids.size(); i++ ) {
		kill(pids[i], SIGUSR1);
	}
	printf("\r\n" T_CYAN "%s" T_WHITE " 님께 전보를 보냈습니다.\r\n", nick.c_str());
}
