#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#define MAX_LINE 	1024
#define MAX_CLIENT 	1000

char greeting[1024];
//char greeting[] = "\r\n## \033[7m대화방에 오신걸 환영합니다.\033[0m\r\n\r\n";

int max_user;
int server_fd;
int server_port;

struct client {
	bool author;
	char userid[256];
	char nickname[256];
	char ip[40];
	int socket;
	int port;
};

std::vector<client> clients;

int send_msg(int socket, const char *msg);
int recv_msg(int socket, char *msg, int bufsize);
void write_user_count(void);
void write_room_info(void);

std::vector<std::string> split_string(std::string str, char delimiter)
{
	std::stringstream test(str);
	std::string segment;
	std::vector<std::string> result;

	while(std::getline(test, segment, delimiter))
	   result.push_back(segment);
	
	return result;
}

//앞에 있는 개행 문자 제거
std::string ltrim(std::string s) 
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), 
				std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

//뒤에 있는 개행 문자 제거
std::string rtrim(std::string s) 
{
	s.erase(std::find_if(s.rbegin(), s.rend(), 
				std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

//양쪽 끝의 개행 문자 제거
std::string trim(std::string s)
{
	return ltrim(rtrim(s));
}

// 제어문자(ESC 등) 제거 - ANSI 코드 삽입 방지
std::string strip_control(const std::string &s)
{
	std::string r;
	for (unsigned int i=0; i<s.size(); i++) {
		unsigned char ch = (unsigned char)s[i];
		if (ch < 0x20 || ch == 0x7f) continue;
		r += s[i];
	}
	return r;
}

client push_client(int socket, char *userid, char* nickname, char* ip, int port)
{
	client c;
	// 첫번째 접속자가 방 개설자이다.
	if (clients.size() == 0)
		c.author = true;
	else
		c.author = false;
	strcpy(c.userid, userid);
	strcpy(c.nickname, nickname);
	strcpy(c.ip, ip);
	c.port = port;
	c.socket = socket;
	clients.push_back(c);
	return c;
}

int pop_client(client c)
{
	std::vector<client> res;

	for(unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		if ( c.socket != c2.socket ) {
			res.push_back(c2);
		}
	}

	clients = res;
	close(c.socket);

	return 0;
}

void constr_func(client c2, client c)
{
	char buf1[MAX_LINE];
	
	memset(buf1, 0, sizeof(buf1));
	snprintf(buf1, sizeof(buf1), "\r\n\033[7m%s(%s) 님이 입장 하셨습니다.\033[0m\r\n", c.nickname, c.userid);

	send_msg(c2.socket, buf1);
}

void quit_func(client c)
{
	char buf1[MAX_LINE];

	memset(buf1, 0, sizeof(buf1));
	printf("%s is leaved at %s\r\n", c.userid, c.ip);

	snprintf(buf1, sizeof(buf1), "\r\n\033[7m%s(%s) 님이 퇴장 하셨습니다.\033[0m\r\n", c.nickname, c.userid);

	if ( c.author == true ) {
		unsigned int cnt = 0;
		for(unsigned int i=0; i<clients.size(); i++) {
			client c2 = clients[i];
			if (c.socket != c2.socket) {
				// 방장이 퇴장하면 다음 사용자에게 위임한다.
				clients[i].author = true;
				// 같은 버퍼를 원본/대상으로 쓰지 않도록 별도 버퍼 사용
				char buf2[MAX_LINE];
				snprintf(buf2, sizeof(buf2), "%s\033[7m%s(%s) 님이 방장을 위임받았습니다.\033[0m\r\n",
					buf1, c2.nickname, c2.userid);
				strcpy(buf1, buf2);
				break;
			}
		}
	}

	unsigned int cnt = 0;
	for(unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		if (c.socket != c2.socket) {
			send_msg(c2.socket, buf1);
			cnt++;
		}
	}
}

void list_func(client c)
{
	char buf1[MAX_LINE];

	memset(buf1, 0, sizeof(buf1));
	snprintf(buf1, sizeof(buf1), "\r\n\033[7m대화방 접속인원은 %d명 입니다.\033[0m\r\n", (int)clients.size());
	send_msg(c.socket, buf1);

	for(unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		if ( c2.author == true ) {
			snprintf(buf1, sizeof(buf1), "\r\n[%s(%s) from %s:%d] : 방장\r\n", c2.nickname, c2.userid, c2.ip, c2.port);
		} else {
			snprintf(buf1, sizeof(buf1), "\r\n[%s(%s) from %s:%d]\r\n", c2.nickname, c2.userid, c2.ip, c2.port);
		}
		send_msg(c.socket, buf1);
	}
}

int say_func(client from, client to, char *msg)
{
	// 클라이언트 수신 버퍼(1024)를 넘지 않도록 MAX_LINE 으로 제한
	char buf[MAX_LINE];
	snprintf(buf, sizeof(buf), "\r\n\033[7m!%s(%s)\033[0m : %s\r\n", from.nickname, from.userid, msg);

	// 귓속말 받을 회원에게 메세지 전달
	send_msg(to.socket, buf);

	// 귓속말 보낸 사람에게도 보냄
	send_msg(from.socket, buf);
	return 0;
}

// len 바이트를 모두 읽는다. EOF/에러(타임아웃 포함)면 -1
int read_full(int socket, char *buf, int len)
{
	int got = 0;
	while (got < len) {
		int n = read(socket, buf+got, len-got);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		// 상대방 연결 종료
		if (n == 0) return -1;
		got += n;
	}
	return got;
}

// len 바이트를 모두 쓴다. 에러면 -1
int write_full(int socket, const char *buf, int len)
{
	int done = 0;
	while (done < len) {
		int n = write(socket, buf+done, len-done);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (n == 0) return -1;
		done += n;
	}
	return done;
}

// 메세지 수신. 연결 종료/에러/잘못된 길이면 -1
int recv_msg(int socket, char *msg, int bufsize)
{
	int size;
	msg[0] = '\0';

	if (read_full(socket, (char*)&size, sizeof(int)) < 0) return -1;

	// 상대가 보낸 길이를 그대로 믿지 않는다
	if (size < 0 || size >= bufsize) return -1;

	if (size > 0 && read_full(socket, msg, size) < 0) return -1;
	msg[size] = '\0';

	return size;
}

int send_msg(int socket, const char *msg)
{
	int size = strlen(msg);
	if (write_full(socket, (const char*)&size, sizeof(int)) < 0) return -1;
	if (write_full(socket, msg, size) < 0) return -1;
	return 0;
}

int server_close (void)  
{
	for (unsigned int i=0; i<clients.size(); i++) {
		client c = clients[i];
		
		send_msg(c.socket, (char*)"/quit");
		usleep(1000*500);
	}

	close(server_fd);
	
	char buf[1024];
	sprintf(buf, "%s/chatt/%d.room", getenv("HANULSO"), server_port);
	unlink(buf);

	sleep(1);

	exit(0);
}

// 대화방 접속 인원수 파일 업데이트
void write_user_count(void)
{
	char buf[1024];
	sprintf(buf, "%s/chatt/%d.room", getenv("HANULSO"), server_port);
	FILE *fp = fopen(buf, "w");
	if (fp == NULL) return;
	fprintf(fp, "%d", (int)clients.size());
	fclose(fp);
}

// 대화방 정보 파일 업데이트
void write_room_info()
{
	char buf[1024];
	sprintf(buf, "%s/chatt/%d.room", getenv("HANULSO"), server_port);

	std::string author;
	for (unsigned int i=0; i<clients.size(); i++) {
		client c = clients[i];
		if ( c.author == true ) {
			author = c.userid;
		}
	}

	FILE *fp = fopen(buf, "w");
	if (fp == NULL) return;
	// 방장,접속인원수
	fprintf(fp, "%s,%d", author.c_str(), (int)clients.size());
	fclose(fp);
}

// 접속자 퇴장 처리 (/bye 또는 연결 끊김)
void remove_client(client c)
{
	quit_func(c);
	pop_client(c);

	// 대화방 접속 인원수 파일 업데이트
	write_room_info();

	// 대화방에 아무도 없으면 방 종료
	if ( clients.size() == 0 ) {
		server_close();
	}
}

// 새 접속자 처리
void accept_client(void)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd,(struct sockaddr *)&client_addr, &client_len);
	if (client_fd < 0) return;

	// select() 로 감시할 수 없는 fd 는 받지 않는다
	if (client_fd >= FD_SETSIZE) {
		close(client_fd);
		return;
	}

	// userid 를 보내지 않는 클라이언트가 대화방 전체를 멈추지 않도록 수신 타임아웃(5초)
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	char userid[256];
	char nickname[256];
	char ip[256];
	int port;

	memset(userid, 0, sizeof(userid));
	memset(nickname, 0, sizeof(nickname));
	memset(ip, 0, sizeof(ip));

	// 접속 사용자ID, 닉네임 받음
	if (recv_msg(client_fd, userid, sizeof(userid)) < 0 ||
		recv_msg(client_fd, nickname, sizeof(nickname)) < 0) {
		close(client_fd);
		return;
	}
	strcpy(userid, strip_control(userid).c_str());
	strcpy(nickname, strip_control(nickname).c_str());

	// 허용 인원 초과
	if (max_user > 0 && (int)clients.size() >= max_user) {
		send_msg(client_fd, "\r\n\033[7m대화방 허용 인원이 꽉 찼습니다.\033[0m\r\n");
		close(client_fd);
		return;
	}

	// 접속 사용자 IP 주소
	inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
	// 접속 사용자 포트 번호
	port = ntohs(client_addr.sin_port);

	// 클라이언트 추가
	client c = push_client(client_fd, userid, nickname, ip, port);
	printf("%s is connected from %s\r\n", c.userid, c.ip);

	// 대화방 접속 인원수 파일 업데이트
	write_room_info();

	// 접속자에게 환영 메세지 보내기
	send_msg(client_fd, greeting);

	// 다른 접속자들에게 접속 사실을 알림
	for (unsigned int i=0; i<clients.size(); i++) {
		client c2 = clients[i];
		if (c2.socket != c.socket) {
			constr_func(c2, c);
		}
	}
}

int main(int argc,char *argv[])
{
	struct sockaddr_in server_addr;

	int max_fd = 0;
	fd_set read_fds;

	if (argc < 4)
	{
		printf("## HANULSO BBS ##\n");
		printf("Chatting server program\n");
		printf("usage: %s port max_user greeting \n",argv[0]);
		exit(-1);
	}
	
	server_port = atoi(argv[1]);
	max_user = atoi(argv[2]);
	sprintf(greeting, "\r\n## \033[7m%s\033[0m\r\n\r\n", argv[3]);
		
    signal(SIGQUIT, (__sighandler_t)server_close);
    signal(SIGINT, (__sighandler_t)server_close);
    signal(SIGTERM, (__sighandler_t)server_close);
    signal(SIGHUP, (__sighandler_t)server_close);
    signal(SIGSEGV, (__sighandler_t)server_close);
    signal(SIGBUS, (__sighandler_t)server_close);
	// 끊어진 소켓에 write 시 프로세스가 죽지 않도록
    signal(SIGPIPE, SIG_IGN);

	server_fd=socket(AF_INET,SOCK_STREAM,0);
	memset(&server_addr,0,sizeof(server_addr));
	// chattclient 는 127.0.0.1 로만 접속한다
	server_addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(server_port);

	// prevent bind error
	int sockopt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));

	if(bind(server_fd,(struct sockaddr *) &server_addr,sizeof(server_addr))==-1){
		printf("Can not Bind\n");
		return -1;
	}

	if(listen(server_fd, MAX_CLIENT)==-1){
		printf("listen Fail\n");
		return -1;
	}
	
	char buf1[MAX_LINE];

	memset(buf1, 0, sizeof(buf1));
	inet_ntop(AF_INET, &server_addr.sin_addr, buf1, sizeof(buf1));
	printf("[server address is %s : %d]\r\n", buf1, ntohs(server_addr.sin_port));

	for (;;)
	{
		FD_ZERO(&read_fds);
		FD_SET(server_fd, &read_fds);
		
		max_fd = server_fd;

		for (unsigned int i=0; i<clients.size(); i++) {
			client c = clients[i];

            //if valid socket descriptor then add to read list
            if (c.socket > 0)
                FD_SET(c.socket, &read_fds);
             
            //highest file descriptor number, need it for the select function
            if(c.socket > max_fd)
                max_fd = c.socket;
		}

		if (select(max_fd+1, &read_fds, NULL, NULL, NULL) < 0)
		{
			printf("Select error\n");
			exit(1);
		}

		// 클라이언트 접속 감지
		if (FD_ISSET(server_fd, &read_fds))
		{
			accept_client();
		}

		// 루프 중 pop_client 로 원소가 지워지므로 int 인덱스를 쓰고 삭제 후 i-- 한다
		for (int i=0; i<(int)clients.size(); i++) {
			client c = clients[i];

			if (FD_ISSET(c.socket, &read_fds)) {
				char line[MAX_LINE];
				memset(line, 0, sizeof(line));

				//int n = read(c.socket, msg, sizeof(msg));
				int n = recv_msg(c.socket, line, sizeof(line));

				// 연결 끊김 또는 잘못된 메세지
				if (n < 0) {
					remove_client(c);
					i--;
					continue;
				}

				if (n > 0) {
					std::vector<std::string> args = split_string(trim(line), ' ');

					// 빈 줄
					if (args.size() == 0) continue;

					// 접속자 로그아웃
					if (!strcasecmp(args[0].c_str(), "/bye")) {
						remove_client(c);
						i--;
						continue;
					}
					
					// 방장으로부터 방폭파 메세지가 왔을때
					if (!strcasecmp(args[0].c_str(), "/quit")) {
						printf("destroy requested from %s\n", c.userid);

						// 방장인지 아닌지 검사
						if ( c.author == true ) {
							// 모든 접속자에게 메세지 전달
							for(unsigned int j=0; j<clients.size(); j++) {
								client c2 = clients[j];
								char msg[MAX_LINE];
								snprintf(msg, sizeof(msg), "\r\n\033[7m%s(%s) 님으로부터 대화방이 종료 되었습니다.\033[0m\r\n",
										c.nickname, c.userid);
								send_msg(c2.socket, msg);
							}

							server_close();

						} else {
							char msg[MAX_LINE];
							sprintf(msg, "\r\n\033[7m방장만 대화방을 종료 가능합니다.\033[0m\r\n");
							send_msg(c.socket, msg);
						}

						continue;
					}

					// 요청 접속자에게 접속자 목록 전달
					if (!strcasecmp(args[0].c_str(), "/list")) {
						list_func(c);
						continue;
					}

					// 특정 접속자에게 개인 메세지 전달
					if (!strcasecmp(args[0].c_str(), "/say")) {
						/* 
						 /say olddos 안녕 모두~
						*/
						if (args.size() < 2) continue;

						char say[1024];
						memset(say, 0, sizeof(say));

						// '안녕 모두~' 를 찾는다.
						int space_count=0;
						for(unsigned int j=0; j<strlen(line); j++) {
							if ( line[j] == ' ' ) space_count++;
							if ( space_count == 2 ) {
								snprintf(say, sizeof(say), "%s", strip_control(trim(line+j)).c_str());
								break;
							}
						}

						for(unsigned int j=0; j<clients.size(); j++) {
							client c2 = clients[j];
							if ( !strcmp(c2.userid, args[1].c_str()) ) {
								say_func(c, c2, say);
							}
						}

						continue;
					}

					// 클라이언트가 만든 "닉네임(ID)" 머리말은 버리고 서버가 다시 만든다.
					// (닉네임 위장 및 ANSI 코드 삽입 방지)
					std::string prefix = std::string("\r\n\033[7m") + c.nickname + "(" + c.userid + ")\033[0m ";
					std::string body = line;
					if (body.compare(0, prefix.size(), prefix) == 0) {
						body = body.substr(prefix.size());
					}
					char out[MAX_LINE];
					snprintf(out, sizeof(out), "\r\n\033[7m%s(%s)\033[0m %s\r\n",
							c.nickname, c.userid, strip_control(body).c_str());

					// 모든 접속자에게 메세지 전달
					for(unsigned int j=0; j<clients.size(); j++) {
						client c2 = clients[j];
						send_msg(c2.socket, out);
					}
				}
			}
		}

		// 0.1sec
		usleep(1000*100);
	}

	return 0;
}

