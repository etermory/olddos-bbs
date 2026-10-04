#include "main.h"

// BB코드를 교체하는 printf 함수
void bbcode_printf(const char *fmt,...)
{
	char buff[9072];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buff, sizeof(buff), fmt, ap);
	va_end(ap);

	printf("%s", replace_bbcode(buff).c_str());
}

// 첫 화면 통계
struct bbs_stats {
	int members;
	int conns;
	int articles;
	int today;
};

// 통계를 새로 계산
static void compute_bbs_stats(bbs_stats &s)
{
	char buf[9072];
	bool ok;

	s.members = 0;
	s.conns = 0;
	s.articles = 0;
	s.today = 0;

	// 전체 회원 수
	std::string count = database::fetch((char*)"SELECT COUNT(*) FROM member;", &ok);
	if ( ok ) s.members = atoi(count.c_str());

	// 전체 접속자 수: 로그인하면 tmp/<tty>.tty 에 아이디를 기록하므로 내용이 있는 파일 수
	// (접속자마다 DB 를 조회하면 접속자가 많을 때 부담이 커서 파일만 확인)
	sprintf(buf, "%s/tmp/*.tty", getenv("HANULSO"));
	std::vector<std::string> files = find_files(buf);
	// (강제 종료로 남은 파일은 read_tty_file 이 pid 를 확인해 지운다)
	for(unsigned int i=0; i<files.size(); i++) {
		std::string user_id;
		if ( read_tty_file(files[i], user_id) ) {
			s.conns += 1;
		}
	}

	// 전체 게시글 수, 오늘의 게시글 수
	for(unsigned int i=0; i<table_names.size(); i++) {
		sprintf(buf, "SELECT COUNT(*), SUM(DATE(DATE_TIME)=CURDATE()) FROM %s;", table_names[i].c_str());
		std::vector<std::map<std::string, std::string> > rows = database::fetch_rows(buf);
		if ( rows.size() > 0 ) {
			std::map<std::string, std::string>::iterator it = rows[0].begin();
			for ( ; it != rows[0].end(); ++it ) {
				if ( it->first.compare(0, 5, "COUNT") == 0 ) s.articles += atoi(it->second.c_str());
				else s.today += atoi(it->second.c_str());
			}
		}
	}
}

// 통계는 60초 동안 파일에 캐시해서 여러 접속자가 함께 쓴다
static void get_bbs_stats(bbs_stats &s)
{
	char path[1024];
	snprintf(path, sizeof(path), "%s/tmp/stats.cache", getenv("HANULSO"));

	struct stat st;
	if ( stat(path, &st) == 0 && time(NULL) - st.st_mtime < 60 ) {
		std::string txt = read_file(path);
		if ( sscanf(txt.c_str(), "%d %d %d %d", &s.members, &s.conns, &s.articles, &s.today) == 4 ) {
			return;
		}
	}

	compute_bbs_stats(s);

	// 다른 프로세스가 읽는 중에 반쯤 쓴 파일을 보지 않도록 임시 파일에 쓰고 이름을 바꾼다
	char tmp[1100];
	snprintf(tmp, sizeof(tmp), "%s.%d", path, (int)getpid());
	FILE *fp = fopen(tmp, "w");
	if ( fp != NULL ) {
		fprintf(fp, "%d %d %d %d\n", s.members, s.conns, s.articles, s.today);
		fclose(fp);
		rename(tmp, path);
	}
}

static std::string int_string(int v)
{
	std::ostringstream tmp;
	tmp << v;
	return tmp.str();
}

std::string replace_bbcode(std::string text)
{
	// 호스트 이름
	if ( text.find("[hostname]", 0) != std::string::npos ) {
		text = replace_all(text, "[hostname]", host_name);
	}

	// 전체 회원 수, 접속자 수, 게시글 수, 오늘의 게시글 수
	if ( text.find("[nummembers]", 0) != std::string::npos ||
			text.find("[numconns]", 0) != std::string::npos ||
			text.find("[numarticles]", 0) != std::string::npos ||
			text.find("[todaynumarticles]", 0) != std::string::npos ) {
		bbs_stats s;
		get_bbs_stats(s);
		text = replace_all(text, "[nummembers]", int_string(s.members));
		text = replace_all(text, "[numconns]", int_string(s.conns));
		text = replace_all(text, "[numarticles]", int_string(s.articles));
		text = replace_all(text, "[todaynumarticles]", int_string(s.today));
	}

/*
	char *tmp = "[numarticles:bbb:10 ]";
	char a[10];
	sscanf(tmp, "[ numarticles:%s ]", a);
	printf("%s %s\n", a);

	press_enter();
	*/
	
#if 0

	static const char* COLORNAMES[] = { 
		"black",     "darkgrey",
		"blue",      "lightblue",
		"green",     "lightgreen",
		"cyan",      "lightcyan",
		"red",       "lightred",
		"purple",    "magenta",
		"brown",     "yellow",
		"grey",      "white",

		"bblack",    "bdarkgrey",
		"bblue",     "blightblue",
		"bgreen",    "blightgreen",
		"bcyan",     "blightcyan",
		"bred",      "blightred",
		"bpurple",   "bmagenta",
		"bbrown",    "byellow",
		"bgrey",     "bwhite"
	};

	static const char* COLORCODES[] = { 
		"[=0F", "[=8F",
		"[=1F", "[=9F",
		"[=2F", "[=10F",
		"[=3F", "[=11F",
		"[=4F", "[=12F",
		"[=5F", "[=13F",
		"[=6F", "[=14F",
		"[=7F", "[=15F",

		"[=0G", "[=8G",
		"[=1G", "[=9G",
		"[=2G", "[=10G",
		"[=3G", "[=11G",
		"[=4G", "[=12G",
		"[=5G", "[=13G",
		"[=6G", "[=14G",
		"[=7G", "[=15G"
	};
	
	// 색 변경
	for(unsigned int i=0; i<32; i++) {
		char bbcode[1024];
		sprintf(bbcode, "[%s]", COLORNAMES[i]);

		if ( text.find(bbcode, 0) != std::string::npos ) {
			text = replace_all(text, bbcode, COLORCODES[i]);
		}
	}
#endif
	
	return text;
}

void print_file(const char *filename)
{
    char buf[9072];

    sprintf(buf,"%s/%s", getenv("HANULSO"), filename);
	std::string text = read_file(buf);
	text = replace_bbcode(text);

	std::vector<std::string> lines = split_string(text, '\n');

	for(int i=0; i<lines.size(); i++) {
        std::string line = lines[i];
        std::size_t pos = line.find("#PAGE");

        if (pos != std::string::npos) {
            std::string before = line.substr(0, pos);
            const std::size_t keyLen = std::string("#PAGE").length();
            printf("%s", before.c_str());
            press_enter();
            printf("\r\n");
        } else {
            printf("%s\r\n", line.c_str());
        }
	}
}
