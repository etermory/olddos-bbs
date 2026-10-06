#include "main.h"

// ------------------------------------------------------------------
// 전보 보내기 (대화방의 /INVITE 처럼 DB 를 쓰지 않는 프로그램이 부른다)
//   bin/tgsend <보내는 아이디> <받는 아이디/닉네임> <내용>
// 결과 한 줄을 출력한다. 보냈으면 종료 코드 0
// ------------------------------------------------------------------

struct termio sys_term;
char tty[10];

void raw_mode(void) {}

int host_close(void)
{
	database::close();
	exit(1);
}

int main(int argc, char **argv)
{
	if ( argc < 4 ) {
		printf("usage: %s <from_id> <to> <text>\n", argv[0]);
		return 1;
	}
	std::string from = argv[1], who = argv[2], text = argv[3];

	read_settings("hanulso.cfg");
	if ( database::open() == false ) {
		printf("전보를 보내지 못했습니다.\n");
		return 1;
	}

	bool exist;
	std::string to;
	database::user_info((char*)who.c_str(), &exist);
	if ( exist ) to = who;
	else {
		std::map<std::string, std::string> u = database::user_info_by_nick_name((char*)who.c_str(), &exist);
		if ( exist ) to = u["USER_ID"];
	}
	if ( to.empty() ) {
		printf("'%s' 회원이 없습니다.\n", string_truncate(display_text(who), 20, "").c_str());
		return 1;
	}
	if ( to == from ) {
		printf("자기 자신에게는 보낼 수 없습니다.\n");
		return 1;
	}

	std::map<std::string, std::string> u = database::user_info((char*)to.c_str(), &exist);
	std::string nick = display_text(u["NICK_NAME"]);
	if ( nick.empty() ) nick = display_text(to);
	std::map<std::string, std::string> me = database::user_info((char*)from.c_str(), &exist);
	std::string my_nick = display_text(me["NICK_NAME"]);
	if ( my_nick.empty() ) my_nick = display_text(from);

	bool ok;
	std::string refused = database::fetch((char*)("SELECT COUNT(*) FROM telegram_refuse WHERE USER_ID='" +
				database::escape(to.c_str()) + "'").c_str(), &ok);
	if ( atoi(refused.c_str()) > 0 ) {
		printf("%s 님은 지금 전보를 받지 않습니다.\n", nick.c_str());
		return 1;
	}

	std::string q = "INSERT INTO telegram (FROM_USER_ID, TO_USER_ID, TEXT, DATE_TIME) VALUES ('" +
		database::escape(from.c_str()) + "', '" + database::escape(to.c_str()) + "', '" +
		database::escape(text.c_str()) + "', NOW())";
	if ( mysql_query(mysql, q.c_str()) != 0 ) {
		printf("전보를 보내지 못했습니다.\n");
		return 1;
	}
	// 접속해 있으면 바로 알린다 (프롬프트에서 전보, 대화방 안이면 대화창에)
	if ( notify_online(to, "★ 전보 ─ " + my_nick + ": " + display_text(text), true) == 0 ) {
		printf("%s 님은 지금 접속해 있지 않습니다. (다음에 접속하면 보입니다)\n", nick.c_str());
		database::close();
		return 0;
	}
	printf("%s 님께 초대 전보를 보냈습니다.\n", nick.c_str());
	database::close();
	return 0;
}
