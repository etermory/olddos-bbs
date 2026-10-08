#include "main.h"
#include "picojson.h"
#include <iconv.h>
#include <errno.h>

// ------------------------------------------------------------------
// 우편번호 찾기 : 도로명/지번 주소나 건물 이름으로 우편번호(5 자리)를 찾는다
//   bin/zipcode <호스트이름> <아이디> <tty>
// 행정안전부 도로명주소 검색 API (https://business.juso.go.kr) 를 쓴다. 무료지만 승인키가 필요하다.
// hanulso.cfg:
//   <juso>
//     <key>승인키</key>
//   </juso>
// 시험할 때는 환경 변수 JUSO_API_URL 로 주소를 바꿀 수 있다.
// ------------------------------------------------------------------

struct termio sys_term;

char title[1024] = "우편번호 찾기";

char tty[10];

char host_name[256];

#define T_W		"\033[=15F"
#define T_Y		"\033[=14F"
#define T_C		"\033[=11F"
#define T_G		"\033[=7F"
#define T_R		"\033[=12F"

static const int PER_PAGE = 8;		// 한 화면에 주소 8 개 (주소 하나에 두 줄)

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
}

/* 프로그램 종료 루틴 */
int host_close (void)
{
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

void print_header(const char *head_title)
{
	printf(ESC_CLEAR);
    printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	int center = (80-strlen(strip_ansi_codes(head_title)))/2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, head_title);
    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
    printf("\033[4;1H");
}

// 글자 코드 바꾸기 (바꿀 수 없는 글자는 건너뛴다)
static std::string convert(const std::string &in, const char *to, const char *from)
{
	iconv_t cd = iconv_open(to, from);
	if ( cd == (iconv_t)-1 ) return in;
	std::string out;
	char *src = (char*)in.data();
	size_t left = in.size();
	char buf[4096];
	while ( left > 0 ) {
		char *dst = buf;
		size_t room = sizeof(buf);
		size_t r = iconv(cd, &src, &left, &dst, &room);
		out.append(buf, dst - buf);
		if ( r == (size_t)-1 && errno != E2BIG ) { src++; left--; }
	}
	iconv_close(cd);
	return out;
}
static std::string to_utf8(const std::string &s) { return convert(s, "UTF-8", "CP949"); }
static std::string to_cp949(const std::string &s) { return convert(s, "CP949//TRANSLIT", "UTF-8"); }

static std::string url_encode(const std::string &s)
{
	std::string o;
	char b[4];
	for ( unsigned int i = 0; i < s.size(); i++ ) {
		unsigned char c = (unsigned char)s[i];
		if ( isalnum(c) || c == '-' || c == '_' || c == '.' ) o += (char)c;
		else { snprintf(b, sizeof(b), "%%%02X", c); o += b; }
	}
	return o;
}

static std::string jstr(const picojson::value &v, const char *key)
{
	if ( v.is<picojson::object>() && v.contains(key) && !v.get(key).is<picojson::null>() ) return v.get(key).to_str();
	return "";
}

// 검색어 정리: 한글/영문/숫자/공백/- 만 남긴다 (API 가 특수문자, SQL 예약어 같은 말을 막는다)
static std::string clean_keyword(const std::string &in)
{
	std::string o;
	for ( unsigned int i = 0; i < in.size(); i++ ) {
		unsigned char c = (unsigned char)in[i];
		if ( c >= 0x80 && i + 1 < in.size() ) { o += in[i]; o += in[i + 1]; i++; continue; }
		if ( isalnum(c) || c == ' ' || c == '-' ) o += (char)c;
		else o += ' ';
	}
	return trim(o);
}

struct address { std::string zip, road, jibun; };

// 찾기. 실패하면 err 에 이유 (EUC-KR)
static bool search(const std::string &key, const std::string &keyword, int page,
		std::vector<address> &list, int &total, std::string &err)
{
	const char *base = getenv("JUSO_API_URL");
	std::string url = std::string(base ? base : "https://business.juso.go.kr/addrlink/addrLinkApi.do")
		+ "?confmKey=" + url_encode(key) + "&currentPage=" + TO_STRING(page)
		+ "&countPerPage=" + TO_STRING(PER_PAGE) + "&resultType=json&keyword=" + url_encode(to_utf8(keyword));
	std::string body;
	list.clear();
	total = 0;
	if ( !download_text(url, body) ) { err = "주소 서버에 연결하지 못했습니다. 잠시 뒤에 다시 해 보세요."; return false; }
	picojson::value v;
	if ( !picojson::parse(v, body).empty() || !v.is<picojson::object>() || !v.contains("results") ) {
		err = "주소 서버의 대답을 읽지 못했습니다.";
		return false;
	}
	const picojson::value &res = v.get("results");
	const picojson::value &common = res.get("common");
	std::string code = jstr(common, "errorCode");
	if ( code != "0" ) {
		err = to_cp949(jstr(common, "errorMessage"));
		if ( code == "E0001" ) err += " (hanulso.cfg 의 <juso><key> 승인키를 확인하세요)";
		if ( err.empty() ) err = "찾지 못했습니다 (" + code + ")";
		return false;
	}
	total = atoi(jstr(common, "totalCount").c_str());
	if ( res.contains("juso") && res.get("juso").is<picojson::array>() ) {
		const picojson::array &a = res.get("juso").get<picojson::array>();
		for ( unsigned int i = 0; i < a.size(); i++ ) {
			address ad;
			ad.zip = jstr(a[i], "zipNo");
			ad.road = to_cp949(jstr(a[i], "roadAddr"));
			ad.jibun = to_cp949(jstr(a[i], "jibunAddr"));
			list.push_back(ad);
		}
	}
	return true;
}

static std::string config_key(void)
{
	pugi::xml_document doc;
	std::string cfg = std::string(getenv("HANULSO") ? getenv("HANULSO") : ".") + "/hanulso.cfg";
	if ( !doc.load_file(cfg.c_str()) ) return "";
	return trim(doc.child("hanulso").child("juso").child("key").child_value());
}

int main(int argc, char **argv)
{
	if ( argc > 1 ) {
		snprintf(host_name, sizeof(host_name), "%s", argv[1]);
	}

    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, (__sighandler_t)host_close);
    signal(SIGSEGV, (__sighandler_t)host_close);
    signal(SIGBUS, (__sighandler_t)host_close);

    ioctl(0,TCGETA, &sys_term);
	raw_mode();
    umask(0111);

	std::string key = config_key();
	std::string keyword, message;
	std::vector<address> list;
	int page = 1, total = 0;

	while ( 1 ) {
		print_header(title);
		if ( key.empty() ) {
			printf("\r\n  " T_R "우편번호 찾기가 아직 준비되지 않았습니다." T_W "\r\n");
			printf("\r\n  " T_G "운영자: https://business.juso.go.kr 에서 '도로명주소 검색 API' 승인키를 받아" T_W);
			printf("\r\n  " T_G "        hanulso.cfg 에 <juso><key>승인키</key></juso> 로 넣어 주세요." T_W "\r\n");
			printf("\r\n [Enter] 를 누르세요.");
			press_enter();
			break;
		}
		if ( keyword.empty() ) {
			printf("\r\n  도로명 주소, 지번 주소, 건물 이름으로 우편번호를 찾습니다.\r\n");
			printf("\r\n  " T_G "예)  세종대로 110      반포동 30-1      대치동 은마아파트      강남구 테헤란로 521" T_W "\r\n");
			printf("\r\n  " T_G "    동/길 이름만 넣으면 너무 많이 나오니 번지나 건물 이름을 함께 넣으세요." T_W "\r\n");
		} else {
			int pages = (total + PER_PAGE - 1) / PER_PAGE;
			printf("  " T_Y "'%s'" T_W " 찾은 주소 %d 개", keyword.c_str(), total);
			if ( pages > 1 ) printf("  " T_G "(%d / %d 쪽)" T_W, page, pages);
			printf("\r\n");
			for ( unsigned int i = 0; i < list.size(); i++ ) {
				printf("  " T_Y "%s" T_W "  %s\r\n", list[i].zip.c_str(), string_truncate(list[i].road, 69, "..").c_str());
				printf("         " T_G "%s" T_W "\r\n", string_truncate(list[i].jibun, 69, "..").c_str());
			}
			if ( list.empty() ) printf("\r\n  " T_G "찾은 주소가 없습니다. 띄어쓰기를 바꾸거나 번지, 건물 이름을 넣어 보세요." T_W "\r\n");
		}
		if ( !message.empty() ) printf("\r\n  %s\r\n", message.c_str());
		message.clear();

		char in[128];
		int pages = (total + PER_PAGE - 1) / PER_PAGE;
		printf(ESC_HAN);
		if ( pages > 1 ) printf("\r\n찾을 주소  다음 쪽(N) 앞 쪽(B)  상위메뉴(P) 종료(X) >> ");
		else printf("\r\n찾을 주소  상위메뉴(P) 종료(X) >> ");
		line_input(in, 60);
		printf(ESC_ENG);

		std::string c = trim(in);
		if ( c.empty() ) continue;
		if ( !strcasecmp(c.c_str(), "p") || !strcasecmp(c.c_str(), "x") || !strcasecmp(c.c_str(), "q") ) break;

		int want = 0;
		std::string kw = keyword;
		if ( !strcasecmp(c.c_str(), "n") ) {
			if ( page >= pages ) { message = T_G "마지막 쪽입니다." T_W; continue; }
			want = page + 1;
		} else if ( !strcasecmp(c.c_str(), "b") ) {
			if ( page <= 1 ) { message = T_G "첫 쪽입니다." T_W; continue; }
			want = page - 1;
		} else {
			kw = clean_keyword(c);
			if ( kw.size() < 2 ) { message = T_R "두 글자 이상 넣으세요." T_W; continue; }
			want = 1;
		}

		printf("\r\n  " T_G "찾는 중..." T_W);
		fflush(stdout);
		std::vector<address> found;
		int t = 0;
		std::string err;
		if ( !search(key, kw, want, found, t, err) ) {
			message = T_R + err + T_W;
			continue;
		}
		keyword = kw;
		list = found;
		total = t;
		page = want;
	}

	host_close();
	return 0;
}
