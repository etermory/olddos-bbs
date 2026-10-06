#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termio.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <signal.h>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#include "ansi.h"

#define CHATDATA 1024

int cursor_pos = 0;

char userid[256];
char nickname[256];
char room_no[16] = "";		// ¹æ ¹øÈ£ (/INVITE Àüº¸¿¡ ¾¸)
char notice_path[1024] = "";	// Àüº¸/ÂÊÁö ¾Ë¸² ÆÄÀÏ (tmp/<tty>.notice)

int sock_fd;
int max_fd;
fd_set read_fds;

int scroll_starty=2;
int scroll_endy=30;

pthread_t threads[5];

int chatt_close (void);

bool is_han(char c)
{
	if((c & 0x80)==0) {
		return false;
	} else {
		return true;
	}
}

// ÀÔ·Â ¹öÆÛ(stdio)³ª ¼ÒÄÏ¿¡ ¹Ù·Î ÀÌ¾î¼­ µé¾î¿Â ±ÛÀÚ°¡ ÀÖ´ÂÁö
static bool input_pending(int msec)
{
	if ( stdin->_IO_read_ptr < stdin->_IO_read_end ) return true;

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = msec * 1000;
	return select(1, &fds, NULL, NULL, &tv) > 0;
}

void line_input(char *str, int len)
{
    cursor_pos = 0;
    int ch;
	// ÇÑ±Û µÎ ¹øÂ° ¹ÙÀÌÆ®¸¦ ±â´Ù¸®´Â ÁßÀÎÁö, ±× ¹ÙÀÌÆ®¸¦ ¹ö·Á¾ß ÇÏ´ÂÁö
	bool wait_trail = false;
	bool skip_trail = false;

    while((ch=getchar()) != '\r' ) {
		// ÀÔ·Â Á¾·á(EOF) ½Ã ¹«ÇÑ ·çÇÁ ¹æÁö
		if(ch == EOF) {
			chatt_close();
		}
        if(ch == '\b') {
			wait_trail = false;
			skip_trail = false;
            if(cursor_pos > 0) {
				// ¸¶Áö¸· ±ÛÀÚ°¡ ÇÑ±Û(2 ¹ÙÀÌÆ®)ÀÌ¸é ÇÑ ¹ø¿¡ Áö¿î´Ù
				int last = 1;
				int k = 0;
				while ( k < cursor_pos ) {
					if ( is_han(str[k]) && k + 1 < cursor_pos ) {
						last = 2;
						k += 2;
					} else {
						last = 1;
						k += 1;
					}
				}

				cursor_pos -= last;
				for (int n=0; n<last; n++) {
					putchar('\b'); putchar(' '); putchar('\b');
				}

				// ÇÑ±Û ÇÑ ±ÛÀÚ¿¡ ¹é½ºÆäÀÌ½º¸¦ ¹ÙÀÌÆ® ¼ö¸¸Å­(2 ¹ø) º¸³»´Â ÅÍ¹Ì³ÎÀÌ¸é
				// ¹Ù·Î ÀÌ¾î¼­ µé¾î¿Â µÎ ¹øÂ° ¹é½ºÆäÀÌ½º´Â ¹ö¸°´Ù
				if ( last == 2 && input_pending(10) ) {
					int c2 = getchar();
					if ( c2 != '\b' && c2 != EOF ) {
						ungetc(c2, stdin);
					}
				}
            }
        }
		else if(ch == 27) {
			getchar();
			getchar();
		}
        else if((ch == 0x1b) | (ch == 0x18) | (ch == 0x0f));
		else if(is_han((char)ch)) {
			if ( !wait_trail ) {
				// ÇÑ±Û Ã¹ ¹ÙÀÌÆ®: µÎ ¹ÙÀÌÆ®°¡ ´Ù µé¾î°¥ ÀÚ¸®°¡ ¾øÀ¸¸é ±ÛÀÚ¸¦ ¹ŞÁö ¾Ê´Â´Ù
				wait_trail = true;
				skip_trail = (cursor_pos + 2 > len);
				if ( !skip_trail ) {
					str[cursor_pos++] = ch;
				}
			} else {
				wait_trail = false;
				if ( !skip_trail ) {
					str[cursor_pos++] = ch;
					putchar(str[cursor_pos-2]);
					putchar(ch);
				}
				skip_trail = false;
			}
		}
        else {
			// ÇÑ±Û µÎ ¹øÂ° ¹ÙÀÌÆ®°¡ ¿ÀÁö ¾Ê¾ÒÀ¸¸é Ã¹ ¹ÙÀÌÆ®´Â ¹ö¸°´Ù
			if ( wait_trail ) {
				if ( !skip_trail ) cursor_pos--;
				wait_trail = false;
				skip_trail = false;
			}
			if(cursor_pos < len) {
				str[cursor_pos++] = ch;
				putchar(ch);
			}
        }
    }

	// ÇÑ±Û µÎ ¹øÂ° ¹ÙÀÌÆ® ¾øÀÌ ³¡³µÀ¸¸é Ã¹ ¹ÙÀÌÆ®´Â ¹ö¸°´Ù
	if ( wait_trail && !skip_trail ) cursor_pos--;

    str[cursor_pos] = 0;
}

void press_enter(void) 
{
	char buff[2];
	line_input(buff, 1);
}

std::vector<std::string> split_string(std::string str, char delimiter)
{
	std::stringstream test(str);
	std::string segment;
	std::vector<std::string> result;

	while(std::getline(test, segment, delimiter))
	   result.push_back(segment);
	
	return result;
}

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

	return;
}

// ÅÍ¹Ì³Î ÁÙ ¼ö ¾Ë¾Æ³»±â
// Ä¿¼­¸¦ ¸Ç ¾Æ·¡·Î º¸³½ µÚ À§Ä¡ º¸°í(ESC[6n)¸¦ ¿äÃ»ÇØ ÀÀ´ä ESC[Çà;¿­R À» ÀĞ´Â´Ù.
// ÀÀ´äÀÌ ¾øÀ¸¸é telnet ÀÌ ¾Ë·ÁÁØ Ã¢ Å©±â, ±×°Íµµ ¾øÀ¸¸é 24 ÁÙ
int detect_screen_rows(void)
{
	int rows = 0;

	printf("\033[999;1H\033[6n");
	fflush(stdout);

	char buf[32];
	int len = 0;
	while (len < (int)sizeof(buf)-1) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		if (select(1, &fds, NULL, NULL, &tv) <= 0) break;

		char ch;
		if (read(0, &ch, 1) != 1) break;
		buf[len++] = ch;
		if (ch == 'R') break;
	}
	buf[len] = 0;

	char *p = strchr(buf, '[');
	int r, c;
	if (p != NULL && sscanf(p+1, "%d;%d", &r, &c) == 2) {
		rows = r;
	}

	if (rows < 10) {
		struct winsize ws;
		if (ioctl(0, TIOCGWINSZ, &ws) == 0 && ws.ws_row >= 10) {
			rows = ws.ws_row;
		}
	}

	if (rows < 10) rows = 24;
	return rows;
}

void print_message(char *msg)
{
	printf("[%d;%dr", scroll_starty, scroll_endy);
	fflush(stdout);
	printf("[%d;1H%s", scroll_endy-1, msg);
	fflush(stdout);
	printf("[%d;%dH", scroll_endy+2, 9+cursor_pos);
	fflush(stdout);
}

// len ¹ÙÀÌÆ®¸¦ ¸ğµÎ ÀĞ´Â´Ù. EOF/¿¡·¯¸é -1
int read_full(int socket, char *buf, int len)
{
	int got = 0;
	while (got < len) {
		int n = read(socket, buf+got, len-got);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		// »ó´ë¹æ ¿¬°á Á¾·á
		if (n == 0) return -1;
		got += n;
	}
	return got;
}

// len ¹ÙÀÌÆ®¸¦ ¸ğµÎ ¾´´Ù. ¿¡·¯¸é -1
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

// ¸Ş¼¼Áö ¼ö½Å. ¿¬°á Á¾·á/¿¡·¯/Àß¸øµÈ ±æÀÌ¸é -1
int recv_msg(int socket, char *msg, int bufsize)
{
	int size;
	msg[0] = '\0';

	if (read_full(socket, (char*)&size, sizeof(int)) < 0) return -1;

	// »ó´ë°¡ º¸³½ ±æÀÌ¸¦ ±×´ë·Î ¹ÏÁö ¾Ê´Â´Ù
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

// ¼Ğ ¸í·É ÀÎÀÚ¸¦ ÀÛÀºµû¿ÈÇ¥·Î °¨½Ñ´Ù
std::string shell_quote(const std::string &s)
{
	std::string r = "'";
	for (unsigned int i=0; i<s.size(); i++) {
		if (s[i] == '\'') r += "'\\''";
		else r += s[i];
	}
	return r + "'";
}

// 79 Ä­ ¾ÈÀ¸·Î (ÇÑ±ÛÀÌ ¹İÀ¸·Î Àß¸®Áö ¾Ê°Ô)
std::string cut79(const std::string &s)
{
	unsigned int k = 0;
	while (k < s.size()) {
		unsigned int w = ((unsigned char)s[k] >= 0x80 && k + 1 < s.size()) ? 2 : 1;
		if (k + w > 79) break;
		k += w;
	}
	return s.substr(0, k);
}

// ´ëÈ­¹æ¿¡ ÀÖ´Â µ¿¾È ¿Â Àüº¸/ÂÊÁö ¾Ë¸² (´Ù¸¥ È¸¿øÀÌ tmp/<tty>.notice ¿¡ ³²±ä´Ù)
void show_notices(void)
{
	if (notice_path[0] == 0) return;
	FILE *fp = fopen(notice_path, "r");
	if (fp == NULL) return;
	char line[1024];
	std::string out;
	while (fgets(line, sizeof(line), fp) != NULL) {
		std::string l = line;
		while (!l.empty() && (l[l.size()-1] == '\n' || l[l.size()-1] == '\r')) l.erase(l.size()-1);
		if (!l.empty()) out += "\r\n\033[=14F" + cut79(l) + "\033[=15F";
	}
	fclose(fp);
	unlink(notice_path);
	if (!out.empty()) {
		out += "\r\n";
		print_message((char*)out.c_str());
	}
}

// /INVITE ¾ÆÀÌµğ : ´ëÈ­¹æ ¹ÛÀÇ Á¢¼ÓÀÚ¿¡°Ô ÀÌ ¹æÀ¸·Î ¿À¶ó´Â Àüº¸ (bin/tgsend °¡ DB ¿¡ ³Ö´Â´Ù)
void invite(const std::string &who)
{
	std::string text = std::string(room_no) == "0" ?
		"¸¸³²ÀÇ ±¤Àå(0¹ø ´ëÈ­¹æ)À¸·Î ¿À¼¼¿ä! GO CHAT ¿¡¼­ 0¹ø" :
		std::string(room_no) + "¹ø ´ëÈ­¹æÀ¸·Î ¿À¼¼¿ä! GO CHAT ¿¡¼­ " + room_no + "¹ø";
	std::string cmd = std::string(getenv("HANULSO")) + "/bin/tgsend " + shell_quote(userid) + " "
		+ shell_quote(who) + " " + shell_quote(text) + " 2>/dev/null";
	FILE *fp = popen(cmd.c_str(), "r");
	char line[512] = "";
	if (fp != NULL) {
		if (fgets(line, sizeof(line), fp) == NULL) line[0] = 0;
		pclose(fp);
	}
	std::string l = line;
	while (!l.empty() && (l[l.size()-1] == '\n' || l[l.size()-1] == '\r')) l.erase(l.size()-1);
	if (l.empty()) l = "ÃÊ´ë¸¦ º¸³»Áö ¸øÇß½À´Ï´Ù.";
	std::string out = "\r\n\033[=7F[ÃÊ´ë] " + l + "\033[=15F\r\n";
	print_message((char*)out.c_str());
}

void *chatt_message(void *arg)
{
	char msg[CHATDATA];

	while (1) {
		FD_ZERO(&read_fds);
		FD_SET(0, &read_fds);
		FD_SET(sock_fd, &read_fds);

		// 2 ÃÊ¸¶´Ù ±ú¾î³ª Àüº¸/ÂÊÁö ¾Ë¸²À» º»´Ù
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if(select(max_fd, &read_fds, (fd_set *)0, (fd_set *)0, &tv) <0) {
			if (errno == EINTR) continue;
			printf("select error\n");
			exit(1);
		}
		show_notices();

		if (FD_ISSET(sock_fd, &read_fds)) {
			// ¼­¹ö ¿¬°áÀÌ ²÷¾îÁ³°Å³ª Àß¸øµÈ ¸Ş¼¼Áö
			if (recv_msg(sock_fd, msg, sizeof(msg)) < 0) {
				sleep(2);

				chatt_close();
			}

			// ¼­¹ö·ÎºÎÅÍ /quit ¹®ÀÚ¿­À» ¹ŞÀ¸¸é ¹æÀÌ Á¾·á µÈ °ÍÀÓ.
			if ( !strcasecmp(msg, "/quit") ) {
				sleep(2);

				chatt_close();
			}

			print_message(msg);
		}

		// 0.1sec
		usleep(1000*100);
	}
}

int chatt_close (void)  
{
	send_msg(sock_fd, "/bye");
	
	sleep(1);

	close(sock_fd);
	
	// ½ºÅ©·Ñ ¿µ¿ª ÃÊ±âÈ­
	printf("[r");
	fflush(stdout);

	exit(0);
}

int main(int argc,char *argv[])
{
	struct sockaddr_in servaddr;

	printf(ESC_CLEAR);

	if (argc < 5) {
		printf("usage:%s ip, port, userid, nickname\n", argv[0]);
		exit(-1);
	}

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)chatt_close);
    signal(SIGSEGV, (__sighandler_t)chatt_close);
    signal(SIGBUS, (__sighandler_t)chatt_close);
	// ²÷¾îÁø ¼ÒÄÏ¿¡ write ½Ã ÇÁ·Î¼¼½º°¡ Á×Áö ¾Êµµ·Ï
    signal(SIGPIPE, SIG_IGN);
    
	raw_mode();
	umask(0111);

	// È­¸é ¸Ç ¾Æ·¡ 2 ÁÙÀº ¾È³»ÁÙ°ú ÀÔ·ÂÁÙ, ±× À§´Â ´ëÈ­ ½ºÅ©·Ñ ¿µ¿ª
	scroll_endy = detect_screen_rows() - 2;

	printf(ESC_CLEAR);

	sock_fd=socket(AF_INET, SOCK_STREAM,0);
	
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_addr.s_addr=inet_addr(argv[1]);
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(atoi(argv[2]));

	if(connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr))==-1){
		printf("Can not connect\n");
		return -1;
	}

	snprintf(userid, sizeof(userid), "%s", argv[3]);
	snprintf(nickname, sizeof(nickname), "%s", argv[4]);
	if (argc > 5) snprintf(room_no, sizeof(room_no), "%s", argv[5]);

	// Àüº¸/ÂÊÁö ¾Ë¸² ÆÄÀÏ: BBS °¡ ¾²´Â tty ÀÌ¸§ (/dev/pts/3 -> 3)
	const char *tn = ttyname(0);
	if (tn != NULL && strlen(tn) > 9 && getenv("HANULSO") != NULL) {
		snprintf(notice_path, sizeof(notice_path), "%s/tmp/%s.notice", getenv("HANULSO"), tn + 9);
	}

	// Á¢¼Ó »ç¿ëÀÚID º¸³¿
	send_msg(sock_fd, userid);
	// Á¢¼Ó »ç¿ëÀÚ ´Ğ³×ÀÓ º¸³¿
	send_msg(sock_fd, nickname);

	max_fd = sock_fd +1;
	
	int i=0;
	pthread_create(&threads[0], NULL, &chatt_message, (void *)i);
	
	while (1) {
		char input[1024];
		printf("[%d;1H±Ó¼Ó¸»(/SAY) Á¢¼ÓÀÚ(/LIST) Çàµ¿(/ME) ÃÊ´ë(/INVITE) µµ¿ò¸»(/HELP) ÅğÀå(/BYE)[K", scroll_endy+1);
		printf("[%d;1H´ëÈ­ >> [K",scroll_endy+2);

		char user[9072];
		snprintf(user, sizeof(user), "%s(%s)", nickname, userid);

		// ¸»ÇÑ ½Ã°¢ "[21:30] " (8 Ä­) ÀÌ ºÙ¾îµµ 80 Ä­À» ³ÑÁö ¾Ê°Ô
		int maxlen = 67 - (int)strlen(user);
		if (maxlen < 1) maxlen = 1;
		line_input(input, maxlen);
		if (strlen(input) <= 0 ) continue;

		// ¸¶Áö¸· 1¹ÙÀÌÆ®°¡°¡ ÇÑ±ÛÀÌ°í, ¸¶Áö¸· ÀÌÀü ¹ÙÀÌÆ®°¡ ¿µ¹®ÀÌ¸é..
		// ¸¶Áö¸· ÇÑ±Û ±úÁü¹æÁö¸¦ À§ÇØ ¸¶Áö¸·À» Áö¿ò
		int ilen = strlen(input);
		if (is_han(input[ilen-1]) && (ilen < 2 || !is_han(input[ilen-2]))) {
			input[ilen-1] = '\0';
		}
		if (strlen(input) <= 0 ) continue;

		// ¼­¹ö/»ó´ë Å¬¶óÀÌ¾ğÆ® ¼ö½Å ¹öÆÛ(CHATDATA)¸¦ ³ÑÁö ¾Êµµ·Ï Á¦ÇÑ
		char msg[CHATDATA];
		snprintf(msg, sizeof(msg), "\r\n\033[7m%s\033[0m %s\r\n", user, input);

		std::vector<std::string> args = split_string(input, ' ');
		if (args.size() == 0) continue;
		
		if (!strcasecmp(args[0].c_str(), "/quit")) {
			send_msg(sock_fd, (char*)args[0].c_str());
			continue;
		}

		if (!strcasecmp(args[0].c_str(), "/bye")) {
			send_msg(sock_fd, (char*)args[0].c_str());
			break;
		}

		if (!strcasecmp(args[0].c_str(), "/list")) {
			send_msg(sock_fd, (char*)args[0].c_str());
			continue;
		}

		if (!strcasecmp(args[0].c_str(), "/say")) {
			if ( args.size() >= 3 ) {
				send_msg(sock_fd, input);
			}
			continue;
		}

		if (!strcasecmp(args[0].c_str(), "/invite")) {
			if ( args.size() >= 2 ) invite(args[1]);
			else print_message((char*)"\r\n\033[=7F[ÃÊ´ë] /INVITE ¾ÆÀÌµğ\033[=15F\r\n");
			continue;
		}

		// ±× ¹ÛÀÇ '/' ¸í·É (/ME, /ÁÖ»çÀ§, /³¡¸»ÀÕ±â, /ÄûÁî, /HELP ...) Àº ¼­¹ö°¡ Ã³¸®
		if (input[0] == '/') {
			send_msg(sock_fd, input);
			continue;
		}

		send_msg(sock_fd, msg);
	}

	//rc = pthread_join(threads[0], (void **)&status);

	close(sock_fd);
	
	// ½ºÅ©·Ñ ¿µ¿ª ÃÊ±âÈ­
	printf("[r");
	fflush(stdout);

    exit(0);
}

