// ------------------------------------------------------------------
// mudlink : BBS 에서 같은 서버의 머드(MudOS 등)로 연결해 주는 중계 프로그램
//   bin/mudlink <호스트이름> <아이디> <tty> <포트>
//
// - 127.0.0.1 의 정해진 포트에만 연결한다 (telnet 명령처럼 다른 곳으로 갈 수 없음)
// - 머드는 줄 단위 입력을 기대하므로 여기서 한 줄을 입력받아 보낸다
//   (입력한 글자를 화면에 되돌려 주고, 한글 백스페이스도 처리)
// - 머드가 IAC WILL ECHO 를 보내면 (비밀번호 입력) 글자를 보이지 않게 한다
// - 텔넷 협상은 모두 거절하고 화면에는 보내지 않는다
// - 머드의 \n 을 \r\n 으로 바꿔 출력한다
// ------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <termio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IAC		255
#define DONT	254
#define DO		253
#define WONT	252
#define WILL	251
#define SB		250
#define SE		240
#define TELOPT_ECHO	1

#define LINE_MAX_LEN	512

static struct termio sys_term;
static int sock = -1;

// 입력 중인 줄
static char line[LINE_MAX_LEN + 1];
static int line_len = 0;
static bool wait_trail = false;		// 한글 두 번째 바이트를 기다리는 중
static bool server_echo = false;	// 머드가 글자를 대신 보여 주는 중 (비밀번호)

// 텔넷 명령 해석 상태
static int iac_state = 0;			// 0: 보통, 1: IAC 뒤, 2: 옵션 기다림, 3: SB 안, 4: SB 안 IAC 뒤
static int iac_cmd = 0;

// 화면에 마지막으로 \r 을 보냈는지 (\r\n 이 겹치지 않도록)
static bool last_cr = false;
// 사용자가 마지막으로 \r 을 쳤는지 (\r\n 으로 오는 터미널)
static bool user_cr = false;

static void raw_mode(void)
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

static void quit(int code)
{
	if ( sock >= 0 ) close(sock);
	ioctl(0, TCSETAF, &sys_term);
	exit(code);
}

static void on_signal(int sig)
{
	(void)sig;
	quit(1);
}

static void out(const char *buf, int len)
{
	while ( len > 0 ) {
		int n = write(1, buf, len);
		if ( n < 0 ) {
			if ( errno == EINTR ) continue;
			quit(1);
		}
		buf += n;
		len -= n;
	}
}

static void out_str(const char *s)
{
	out(s, strlen(s));
}

static void send_sock(const unsigned char *buf, int len)
{
	while ( len > 0 ) {
		int n = write(sock, buf, len);
		if ( n < 0 ) {
			if ( errno == EINTR ) continue;
			quit(0);
		}
		buf += n;
		len -= n;
	}
}

static void telnet_reply(int cmd, int opt)
{
	unsigned char r[3];
	r[0] = IAC;
	r[2] = opt;

	if ( cmd == WILL ) {
		// 머드가 글자를 대신 보여 주겠다 (비밀번호 입력)
		if ( opt == TELOPT_ECHO ) {
			server_echo = true;
			r[1] = DO;
		} else {
			r[1] = DONT;
		}
		send_sock(r, 3);
	} else if ( cmd == WONT ) {
		if ( opt == TELOPT_ECHO ) server_echo = false;
		r[1] = DONT;
		send_sock(r, 3);
	} else if ( cmd == DO ) {
		r[1] = WONT;
		send_sock(r, 3);
	}
	// DONT 에는 답하지 않는다
}

// 머드에서 온 내용을 해석해 화면에 출력
static void from_server(const unsigned char *buf, int len)
{
	char o[8192];
	int on = 0;

	for (int i=0; i<len; i++) {
		unsigned char c = buf[i];

		switch ( iac_state ) {
		case 0:
			if ( c == IAC ) {
				iac_state = 1;
			} else if ( c == '\n' ) {
				if ( !last_cr ) o[on++] = '\r';
				o[on++] = '\n';
				last_cr = false;
			} else if ( c == 0 ) {
				// 무시
			} else {
				o[on++] = c;
				last_cr = (c == '\r');
			}
			break;
		case 1:
			if ( c == IAC ) {
				o[on++] = (char)IAC;
				iac_state = 0;
			} else if ( c == WILL || c == WONT || c == DO || c == DONT ) {
				iac_cmd = c;
				iac_state = 2;
			} else if ( c == SB ) {
				iac_state = 3;
			} else {
				iac_state = 0;	// GA, NOP 등
			}
			break;
		case 2:
			telnet_reply(iac_cmd, c);
			iac_state = 0;
			break;
		case 3:
			if ( c == IAC ) iac_state = 4;
			break;
		case 4:
			iac_state = (c == SE) ? 0 : 3;
			break;
		}

		if ( on > (int)sizeof(o) - 4 ) {
			out(o, on);
			on = 0;
		}
	}

	if ( on > 0 ) out(o, on);
}

static bool is_han(unsigned char c)
{
	return (c & 0x80) != 0;
}

// 입력에 바로 이어서 온 글자가 있는지
static bool input_pending(int msec)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = msec * 1000;
	return select(1, &fds, NULL, NULL, &tv) > 0;
}

static void echo(const char *s, int len)
{
	if ( server_echo ) {
		// 비밀번호: 별표로
		for (int i=0; i<len; i++) out_str("*");
	} else {
		out(s, len);
	}
}

// 사용자가 친 글자 하나 처리
static void from_user(unsigned char c)
{
	if ( c != '\r' && c != '\n' ) user_cr = false;

	if ( c == '\r' || c == '\n' ) {
		// 한글 두 번째 바이트 없이 끝났으면 첫 바이트는 버린다
		if ( wait_trail ) {
			line_len--;
			wait_trail = false;
		}
		// \r\n 이 함께 오면 \n 은 무시
		bool skip = (c == '\n' && line_len == 0 && user_cr);
		user_cr = (c == '\r');
		if ( skip ) {
			return;
		}
		out_str("\r\n");
		last_cr = false;
		line[line_len++] = '\r';
		line[line_len++] = '\n';
		send_sock((unsigned char *)line, line_len);
		line_len = 0;
		return;
	}

	if ( c == '\b' || c == 127 ) {
		wait_trail = false;
		if ( line_len <= 0 ) return;

		// 마지막 글자가 한글이면 두 바이트를 한 번에 지운다
		int last = 1;
		int k = 0;
		while ( k < line_len ) {
			if ( is_han(line[k]) && k + 1 < line_len ) { last = 2; k += 2; }
			else { last = 1; k += 1; }
		}
		line_len -= last;
		for (int n=0; n<last; n++) out_str("\b \b");

		// 한 글자에 백스페이스를 두 번 보내는 터미널이면 두 번째는 버린다
		if ( last == 2 && input_pending(10) ) {
			unsigned char c2;
			if ( read(0, &c2, 1) == 1 && c2 != '\b' && c2 != 127 ) {
				from_user(c2);
			}
		}
		return;
	}

	// 그 밖의 제어 문자는 무시
	if ( c < 0x20 ) return;

	if ( is_han(c) ) {
		if ( !wait_trail ) {
			if ( line_len + 2 > LINE_MAX_LEN - 2 ) return;
			line[line_len++] = c;
			wait_trail = true;
		} else {
			line[line_len++] = c;
			wait_trail = false;
			echo(line + line_len - 2, 2);
		}
		return;
	}

	if ( wait_trail ) {
		line_len--;
		wait_trail = false;
	}
	if ( line_len + 1 > LINE_MAX_LEN - 2 ) return;
	line[line_len++] = c;
	echo((char *)&c, 1);
}

int main(int argc, char **argv)
{
	// 포트는 네 번째 인자 (메뉴의 args)
	int port = (argc > 4) ? atoi(argv[4]) : 4444;
	if ( port <= 0 || port > 65535 ) port = 4444;

	ioctl(0, TCGETA, &sys_term);
	raw_mode();

	signal(SIGHUP, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGPIPE, SIG_IGN);

	out_str("\r\n 머드 서버에 접속하는 중입니다...\r\n");

	sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if ( sock < 0 || connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
		out_str("\r\n 머드 서버가 열려 있지 않습니다. 운영자에게 알려 주세요.\r\n");
		out_str("\r\n [Enter] 를 누르세요.");
		unsigned char c;
		while ( read(0, &c, 1) == 1 && c != '\r' && c != '\n' ) ;
		quit(0);
	}

	unsigned char buf[4096];
	while (1) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(sock, &fds);

		if ( select(sock + 1, &fds, NULL, NULL, NULL) < 0 ) {
			if ( errno == EINTR ) continue;
			break;
		}

		if ( FD_ISSET(sock, &fds) ) {
			int n = read(sock, buf, sizeof(buf));
			if ( n <= 0 ) {
				if ( n < 0 && errno == EINTR ) continue;
				out_str("\r\n\r\n 머드와 연결이 끊어졌습니다. BBS 로 돌아갑니다.\r\n");
				sleep(2);
				break;
			}
			from_server(buf, n);
		}

		if ( FD_ISSET(0, &fds) ) {
			int n = read(0, buf, sizeof(buf));
			if ( n <= 0 ) {
				if ( n < 0 && errno == EINTR ) continue;
				break;	// 사용자 접속 끊김
			}
			for (int i=0; i<n; i++) {
				from_user(buf[i]);
			}
		}
	}

	quit(0);
	return 0;
}
