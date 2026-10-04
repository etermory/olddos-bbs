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

void line_input(char *str, int len)
{
    cursor_pos = 0;
    int ch;

    while((ch=getchar()) != '\r' ) {
		// ÀÔ·Â Á¾·á(EOF) ½Ã ¹«ÇÑ ·çÇÁ ¹æÁö
		if(ch == EOF) {
			chatt_close();
		}
        if(ch == '\b') {
            if(cursor_pos > 0) {
#if 0
				if (is_han(str[cursor_pos-1])) {
					putchar(ch); putchar(' '); putchar(ch);
					if(cursor_pos > 0) cursor_pos--;
					putchar(ch); putchar(' '); putchar(ch);
					if(cursor_pos > 0) cursor_pos--;
				} else {
					putchar(ch); putchar(' '); putchar(ch);
					if(cursor_pos > 0) cursor_pos--;
				}
#else
				putchar(ch); putchar(' '); putchar(ch);
				if(cursor_pos > 0) cursor_pos--;
#endif
            }
        }
		else if(ch == 27) {
			char ch2 = getchar();
			char ch3 = getchar();
		}
        else if((ch == 0x1b) | (ch == 0x18) | (ch == 0x0f));
        else if(cursor_pos < len) {
			str[cursor_pos++] = ch;
			putchar(ch);
        }
    }

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
// Ä¿¼­¸¦ ¸Ç ¾Æ·¡·Î º¸³½ µÚ À§Ä¡ º¸°í(ESC[6n)¸¦ ¿äÃ»ÇØ ÀÀ´ä ESC[Çà;¿­R À» ÀÐ´Â´Ù.
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

// len ¹ÙÀÌÆ®¸¦ ¸ðµÎ ÀÐ´Â´Ù. EOF/¿¡·¯¸é -1
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

// len ¹ÙÀÌÆ®¸¦ ¸ðµÎ ¾´´Ù. ¿¡·¯¸é -1
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

// ¸Þ¼¼Áö ¼ö½Å. ¿¬°á Á¾·á/¿¡·¯/Àß¸øµÈ ±æÀÌ¸é -1
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

void *chatt_message(void *arg)
{
	char msg[CHATDATA];

	while (1) {
		FD_ZERO(&read_fds);
		FD_SET(0, &read_fds);
		FD_SET(sock_fd, &read_fds);
	
		if(select(max_fd, &read_fds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) <0) {
			printf("select error\n");
			exit(1);
		}

		if (FD_ISSET(sock_fd, &read_fds)) {
			// ¼­¹ö ¿¬°áÀÌ ²÷¾îÁ³°Å³ª Àß¸øµÈ ¸Þ¼¼Áö
			if (recv_msg(sock_fd, msg, sizeof(msg)) < 0) {
				sleep(2);

				chatt_close();
			}

			// ¼­¹ö·ÎºÎÅÍ /quit ¹®ÀÚ¿­À» ¹ÞÀ¸¸é ¹æÀÌ Á¾·á µÈ °ÍÀÓ.
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
	printf("[%d;%dr", 0, 0);

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

	// Á¢¼Ó »ç¿ëÀÚID º¸³¿
	send_msg(sock_fd, userid);
	// Á¢¼Ó »ç¿ëÀÚ ´Ð³×ÀÓ º¸³¿
	send_msg(sock_fd, nickname);

	max_fd = sock_fd +1;
	
	int i=0;
	pthread_create(&threads[0], NULL, &chatt_message, (void *)i);
	
	while (1) {
		char input[1024];
		printf("[%d;1H±Ó¼Ó¸»(/SAY) Á¢¼ÓÀÚÁ¶È¸(/LIST) ÅðÀå(/BYE) ´ëÈ­¹æÁ¾·á(/QUIT)[K", scroll_endy+1);
		printf("[%d;1H´ëÈ­ >> [K",scroll_endy+2);

		char user[9072];
		snprintf(user, sizeof(user), "%s(%s)", nickname, userid);

		int maxlen = 75 - (int)strlen(user);
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

		// ¼­¹ö/»ó´ë Å¬¶óÀÌ¾ðÆ® ¼ö½Å ¹öÆÛ(CHATDATA)¸¦ ³ÑÁö ¾Êµµ·Ï Á¦ÇÑ
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

		send_msg(sock_fd, msg);
	}

	//rc = pthread_join(threads[0], (void **)&status);

	close(sock_fd);
	
	// ½ºÅ©·Ñ ¿µ¿ª ÃÊ±âÈ­
	printf("[%d;%dr", 0, 0);

    exit(0);
}

