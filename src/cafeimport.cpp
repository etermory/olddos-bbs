// ------------------------------------------------------------------
// 네이버 카페 글 가져오기
//   cafeimport <tty> fetch <글 번호 범위...>   받기 (BBS 는 그대로)  예) fetch 103001-103050 103100
//   cafeimport <tty> list | show <번호> | drop <번호>   받은 글 보기/빼기
//   cafeimport <tty> apply [번호...]            받아 둔 글을 BBS 에 올림 (올린 글은 받은 폴더에서 지움)
//
// 카페 글 JSON(로그인 쿠키 필요)을 받아 제목/작성자/본문/그림/첨부를 data/cafe/download/ 에 받아 두고,
// 확인한 뒤 src/restore/table.txt 의 메뉴 대응표에 따라 BBS 게시판에 올린다.
//
// hanulso.cfg:
//   <naver>
//     <cafe_id>28655511</cafe_id>
//     <cookie>NID_AUT=...; NID_SES=...</cookie>   (브라우저에서 로그인한 뒤의 쿠키)
//   </naver>
//
// 기록 (git 에 올리지 않는 data/cafe/):
//   imported.log  가져온 카페 글 번호 (예전 src/restore/restore.log 도 함께 읽어 중복을 막는다)
//   members/      memberKey -> BBS 아이디 (카페의 가려진 아이디, 예 juya****)
//   assigned/     BBS 아이디 -> memberKey (가려진 아이디가 겹치면 juya****2 처럼 나눔)
//   links/        memberKey -> 기존 BBS 회원 아이디 (자동 연결 또는 sysop 에서 연결)
//   unlinked.log  연결 후보 (가려진 아이디, 닉네임, memberKey, 앞부분이 같은 BBS 회원들)
//   link_check.log 닉네임으로 자동 연결했지만 아이디 앞부분이 다른 경우 (확인용)
// 네이버는 카페 매니저에게도 회원 아이디를 가려서 보여 준다 (네이버 개인정보 정책). 전체 아이디는 얻을 수 없다.
// ------------------------------------------------------------------
#include "main.h"
#include "picojson.h"
#include <set>

// utility/database 가 쓰는 전역 (이 프로그램에서는 쓰지 않음)
int host_close() { exit(1); return 0; }
char tty[10];

static std::string home;		// $HANULSO
static std::string state_dir;	// $HANULSO/data/cafe
static std::string cafe_id;
static std::string cookie;
static std::string api_base = "https://apis.naver.com";	// 시험할 때 CAFE_API_BASE 로 바꿀 수 있다
static std::string curl_cfg_cookie;	// 쿠키를 넣은 curl 설정 파일 (권한 600)
static std::string curl_cfg_plain;	// 쿠키 없는 것 (그림 받기)

static int n_ok = 0, n_skip_done = 0, n_skip_menu = 0, n_fail = 0, n_files = 0;

static void out(const std::string &s) { printf("%s\r\n", s.c_str()); fflush(stdout); }

// 최대 sec 초 기다리며 Q (또는 ESC) 를 눌렀는지 본다. 눌렀으면 true
// (운영자 메뉴는 터미널을 한 글자씩 받는 모드로 두므로 Ctrl+C 가 오지 않는다)
static bool user_stop(int sec)
{
	fd_set fds;
	struct timeval tv;
	time_t until = time(0) + sec;
	do {
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		tv.tv_sec = sec > 0 ? 1 : 0;
		tv.tv_usec = 0;
		if ( select(1, &fds, NULL, NULL, &tv) > 0 ) {
			char c;
			if ( read(0, &c, 1) == 1 && (c == 'q' || c == 'Q' || c == 27) ) return true;
		}
	} while ( time(0) < until );
	return false;
}
static std::string u2c(const std::string &s) { return utf8_to_cp949(s); }

// ------------------------------------------------------------------
// curl
// ------------------------------------------------------------------
static bool write_curl_cfg(const std::string &path, bool with_cookie)
{
	FILE *fp = fopen(path.c_str(), "w");
	if ( !fp ) return false;
	chmod(path.c_str(), 0600);
	fprintf(fp, "silent\nlocation\nmax-time = 60\n");
	fprintf(fp, "user-agent = \"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36\"\n");
	fprintf(fp, "header = \"Referer: https://cafe.naver.com/\"\n");
	if ( with_cookie ) fprintf(fp, "header = \"Cookie: %s\"\n", cookie.c_str());
	fclose(fp);
	return true;
}

// url 을 path 에 받는다. HTTP 상태 코드를 돌려준다 (실패하면 0)
static int http_get(const std::string &url, const std::string &path, bool with_cookie)
{
	std::string cmd = "curl -K " + shell_quote(with_cookie ? curl_cfg_cookie : curl_cfg_plain)
		+ " -o " + shell_quote(path) + " -w '%{http_code}' " + shell_quote(url);
	bool ok;
	std::vector<std::string> lines = exec_command((char*)cmd.c_str(), &ok);
	if ( lines.empty() ) return 0;
	return atoi(lines.back().c_str());
}

static std::string last_json;	// 마지막으로 받은 JSON 원본 (받은 글에 article.json 으로 남긴다)

// JSON 받기. 실패하면 err 에 이유
static bool get_json(const std::string &url, picojson::value &v, std::string &err)
{
	std::string tmp = state_dir + "/tmp/api.json";
	int code = 0;
	for (int tries = 0; tries < 3; tries++) {
		code = http_get(url, tmp, true);
		if ( code == 429 || code >= 500 || code == 0 ) { sleep(10 * (tries + 1)); continue; }
		break;
	}
	std::string body = read_file(tmp.c_str());
	unlink(tmp.c_str());
	last_json = body;
	std::string perr = picojson::parse(v, body);
	if ( !perr.empty() ) {
		char b[64]; snprintf(b, sizeof(b), "HTTP %d", code);
		err = std::string(b) + " 응답을 읽지 못했습니다";
		return false;
	}
	if ( v.is<picojson::object>() && v.contains("result") && v.get("result").is<picojson::object>()
	     && v.get("result").contains("errorCode") ) {
		std::string ec = v.get("result").get("errorCode").to_str();
		std::string reason = v.get("result").contains("reason") ? v.get("result").get("reason").to_str() : "";
		err = "[" + ec + "] " + u2c(reason);
		if ( ec == "0004" ) err = "LOGIN";
		return false;
	}
	return true;
}

static std::string jstr(const picojson::value &v, const char *key)
{
	if ( v.is<picojson::object>() && v.contains(key) && !v.get(key).is<picojson::null>() )
		return v.get(key).to_str();
	return "";
}

// ------------------------------------------------------------------
// 작성자 아이디: memberKey -> 카페 프로필의 maskedMemberId (회원별로 저장)
// ------------------------------------------------------------------
// BBS 회원의 닉네임은 카페 닉네임과 같게 쓰고 있다 (BBS 는 닉네임이 겹치지 않게 가입받는다).
// 닉네임이 같은 BBS 회원이 딱 한 명이면 그 회원으로 자동 연결한다.
// 가려진 아이디(coma****)의 앞부분은 확인용: 다르면 link_check.log 에 남긴다.
// 닉네임이 맞는 회원이 없으면 아이디 앞부분이 같은 회원을 후보로 남긴다 (sysop 14 -> 4 에서 연결).
static std::string auto_link(const std::string &member_key, const std::string &masked, const std::string &nick)
{
	size_t star = masked.find('*');
	std::string prefix = (star == std::string::npos || masked == "탈퇴멤버") ? "" : masked.substr(0, star);

	// 카페에서 가져온 회원(비밀번호 '!')은 빼고
	std::string q = "SELECT USER_ID, NICK_NAME FROM member WHERE NICK_NAME = '" + database::escape(nick.c_str())
		+ "' AND USER_ID NOT LIKE '%*%' AND PASSWORD <> '!'";
	std::vector<std::map<std::string, std::string> > rows = database::fetch_rows((char*)q.c_str());
	if ( rows.size() == 1 && !trim(nick).empty() ) {
		std::string id = rows[0]["USER_ID"];
		bool pre = !prefix.empty() && strncasecmp(id.c_str(), prefix.c_str(), prefix.size()) == 0;
		FILE *fp = fopen((state_dir + "/links/" + member_key).c_str(), "w");
		if ( fp ) { fputs(id.c_str(), fp); fclose(fp); }
		if ( !pre ) {
			FILE *fc = fopen((state_dir + "/link_check.log").c_str(), "a");
			if ( fc ) { fprintf(fc, "%s\t%s\t%s\t%s\n", masked.c_str(), nick.c_str(), member_key.c_str(), id.c_str()); fclose(fc); }
		}
		out("    카페 " + masked + " (" + nick + ") -> BBS 회원 " + id + " 자동 연결"
			+ (pre ? "" : " (닉네임만 같음, 아이디 앞부분 다름: data/cafe/link_check.log)"));
		return id;
	}

	// 후보: 아이디 앞부분이 같은 BBS 회원들
	std::string cands;
	if ( !prefix.empty() ) {
		q = "SELECT USER_ID FROM member WHERE USER_ID LIKE '" + database::escape(prefix.c_str())
			+ "%' AND USER_ID NOT LIKE '%*%' AND PASSWORD <> '!'";
		rows = database::fetch_rows((char*)q.c_str());
		for (unsigned int i=0; i<rows.size(); i++)
			cands += (cands.empty() ? "" : ",") + rows[i]["USER_ID"];
	}
	if ( masked == "탈퇴멤버" ) return "";	// 탈퇴 회원은 아이디 단서가 없어 후보로 남기지 않는다
	FILE *fp = fopen((state_dir + "/unlinked.log").c_str(), "a");
	if ( fp ) { fprintf(fp, "%s\t%s\t%s\t%s\n", masked.c_str(), nick.c_str(), member_key.c_str(), cands.c_str()); fclose(fp); }
	return "";
}

static std::string member_id(const std::string &member_key, const std::string &nick)
{
	if ( member_key.empty() ) return "탈퇴멤버";
	// 운영자가 연결했거나 자동 연결된 BBS 회원
	std::string linked = trim(read_file((state_dir + "/links/" + member_key).c_str()));
	if ( !linked.empty() ) return linked;
	std::string path = state_dir + "/members/" + member_key;
	std::string cached = trim(read_file(path.c_str()));
	if ( !cached.empty() ) {
		// 그 사이에 같은 닉네임으로 BBS 에 가입했을 수 있다
		std::string bbs = auto_link(member_key, cached, nick);
		return bbs.empty() ? cached : bbs;
	}

	picojson::value v; std::string err;
	std::string id;
	std::string url = api_base + "/cafe-web/cafe-cafeinfo-api/v3.0/cafes/" + cafe_id
		+ "/members/" + member_key + "/profiles";
	if ( get_json(url, v, err) ) id = u2c(jstr(v.get("result"), "maskedMemberId"));
	else if ( err == "LOGIN" ) return "";
	if ( id.empty() ) id = "탈퇴멤버";

	// 닉네임으로 BBS 회원 찾기 (카페를 탈퇴한 회원도 닉네임은 남아 있다)
	{
		std::string bbs = auto_link(member_key, id, nick);
		if ( !bbs.empty() ) return bbs;
	}

	// 가려진 아이디(coma****)는 다른 사람과 겹칠 수 있다.
	// assigned/<BBS 아이디> 에 그 아이디를 가진 memberKey 를 적어 두고,
	// 다른 사람이 이미 쓰고 있으면 coma****2, coma****3 ... 으로 나눈다.
	// (탈퇴 회원은 누구인지 알 수 없으므로 모두 '탈퇴멤버' 하나로)
	if ( id != "탈퇴멤버" ) {
		std::string base = id;
		for (int n = 1; ; n++) {
			char sfx[16] = "";
			if ( n > 1 ) snprintf(sfx, sizeof(sfx), "%d", n);
			std::string cand = base + sfx;
			std::string apath = state_dir + "/assigned/" + cand;
			std::string owner = trim(read_file(apath.c_str()));
			if ( owner.empty() ) {
				FILE *fa = fopen(apath.c_str(), "w");
				if ( fa ) { fputs(member_key.c_str(), fa); fclose(fa); }
				id = cand;
				break;
			}
			if ( owner == member_key ) { id = cand; break; }
		}
	}

	FILE *fp = fopen(path.c_str(), "w");
	if ( fp ) { fputs(id.c_str(), fp); fclose(fp); }
	return id;
}

// ------------------------------------------------------------------
// 메뉴 대응표, 가져온 글 기록
// ------------------------------------------------------------------
static std::map<int, std::string> load_table(void)
{
	std::map<int, std::string> m;
	std::vector<std::string> lines = split_string(read_file((home + "/src/restore/table.txt").c_str()), '\n');
	for (unsigned int i=0; i<lines.size(); i++) {
		std::string l = lines[i];
		size_t c = l.find(';');
		if ( c != std::string::npos ) l = l.substr(0, c);
		l = trim(l);
		if ( l.empty() ) continue;
		int id; char t[256];
		if ( sscanf(l.c_str(), "%d %255s", &id, t) == 2 ) m[id] = t;
	}
	return m;
}

static void load_done(const std::string &path, std::set<int> &done)
{
	std::vector<std::string> lines = split_string(read_file(path.c_str()), '\n');
	for (unsigned int i=0; i<lines.size(); i++) {
		std::string l = trim(lines[i]);
		if ( l.empty() || l[0] == ';' ) continue;
		int n = atoi(l.c_str());
		if ( n > 0 ) done.insert(n);
	}
}

static void mark_done(int num)
{
	FILE *fp = fopen((state_dir + "/imported.log").c_str(), "a");
	if ( fp ) { fprintf(fp, "%d\n", num); fclose(fp); }
}

// ------------------------------------------------------------------
// 본문: 그림 태그를 [그림 n: 이름] 으로 바꾸고 lynx 로 텍스트(EUC-KR)로
// ------------------------------------------------------------------
struct remote_file { std::string url, name; bool cookie; };

static std::string url_file_name(const std::string &url)
{
	std::string u = url.substr(0, url.find('?'));
	size_t s = u.rfind('/');
	return (s == std::string::npos) ? u : u.substr(s + 1);
}

static bool has_tag(const std::string &t)
{
	return t.find("<span") != std::string::npos || t.find("<p") != std::string::npos
		|| t.find("<div") != std::string::npos || t.find("<br") != std::string::npos
		|| t.find("</") != std::string::npos;
}

// 태그를 지우고 (문단/줄바꿈 태그는 줄바꿈으로) 문자 참조를 푼다.
// 한 번 더 감싸진 HTML 은 참조를 풀면 태그가 다시 드러나므로 태그가 없어질 때까지 되풀이한다.
static std::string strip_html(std::string t)
{
	static const char *ents[][2] = { {"&nbsp;", " "}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""},
		{"&#39;", "'"}, {"&amp;", "&"}, {NULL, NULL} };
	for (int round = 0; round < 3; round++) {
		if ( has_tag(t) ) {
			std::string o;
			for (size_t i=0; i<t.size(); i++) {
				if ( t[i] == '<' ) {
					size_t e = t.find('>', i);
					if ( e == std::string::npos ) { o += t.substr(i); break; }
					// 태그 이름 (소문자). 맨 앞의 '/' 는 닫는 태그 표시, 공백이나 '/' 에서 멈춘다
					std::string nm;
					for (size_t k=i+1; k<e && nm.size() < 8; k++) {
						if ( t[k] == ' ' || (t[k] == '/' && k > i + 1) ) break;
						nm += (char)tolower((unsigned char)t[k]);
					}
					if ( nm == "br" || nm == "/p" || nm == "/div" || nm == "/li" ) o += "\n";
					i = e;
					continue;
				}
				o += t[i];
			}
			t = o;
		}
		bool had_ent = false;
		for (int k=0; ents[k][0]; k++) {
			size_t q = 0;
			while ( (q = t.find(ents[k][0], q)) != std::string::npos ) {
				t.replace(q, strlen(ents[k][0]), ents[k][1]);
				q += strlen(ents[k][1]);
				had_ent = true;
			}
		}
		if ( !had_ent || !has_tag(t) ) break;
	}
	return t;
}

static std::string html_to_text(std::string html, std::vector<remote_file> &images)
{
	// <script> 덩어리 빼기
	size_t p;
	while ( (p = html.find("<script")) != std::string::npos ) {
		size_t e = html.find("</script>", p);
		if ( e == std::string::npos ) break;
		html.erase(p, e + 9 - p);
	}
	// 그림
	p = 0;
	while ( (p = html.find("<img", p)) != std::string::npos ) {
		size_t e = html.find('>', p);
		if ( e == std::string::npos ) break;
		std::string tag = html.substr(p, e + 1 - p);
		std::string src;
		size_t s = tag.find("src=\"");
		if ( s != std::string::npos ) {
			s += 5;
			src = tag.substr(s, tag.find('"', s) - s);
		}
		std::string repl;
		// 스티커/아이콘이 아닌 카페 그림만
		if ( src.find("pstatic.net") != std::string::npos && src.find("sticker") == std::string::npos
		     && src.find("storep-phinf") == std::string::npos ) {
			remote_file f; f.url = src; f.name = url_file_name(src); f.cookie = false;
			images.push_back(f);
			char b[32]; snprintf(b, sizeof(b), "%d", (int)images.size());
			// 본문 HTML 은 UTF-8, 이 소스는 EUC-KR 이라 "그림" 은 문자 참조로
			repl = "<p>[&#44536;&#47548; " + std::string(b) + ": " + f.name + "]</p>";
		}
		html.replace(p, e + 1 - p, repl);
		p += repl.size();
	}

	std::string in = state_dir + "/tmp/body.html", txt = state_dir + "/tmp/body.txt";
	FILE *fp = fopen(in.c_str(), "w");
	if ( !fp ) return "";
	fprintf(fp, "<html><head><meta charset=\"utf-8\"></head><body>%s</body></html>", html.c_str());
	fclose(fp);
	std::string cmd = "lynx -dump -nolist -nomargins -width=76 -assume_charset=utf-8 -display_charset=euc-kr "
		+ shell_quote(in) + " > " + shell_quote(txt) + " 2>/dev/null";
	system(cmd.c_str());
	std::string t = read_file(txt.c_str());
	unlink(in.c_str()); unlink(txt.c_str());
	// lynx 가 없거나 실패하면 원래 HTML 에서 직접 태그를 지운다
	if ( trim(t).empty() && !trim(html).empty() ) t = u2c(html);
	// 안전장치: 태그가 남았으면 (lynx 가 없거나, 본문이 &lt;span&gt; 처럼 한 번 더 감싸진 글) 직접 지운다
	t = strip_html(t);
	// 빈 줄이 셋 이상 이어지면 둘로
	size_t q;
	while ( (q = t.find("\n\n\n\n")) != std::string::npos ) t.erase(q, 1);
	// 끝의 빈 줄 정리
	while ( !t.empty() && (t[t.size()-1] == '\n' || t[t.size()-1] == ' ') ) t.erase(t.size()-1);
	return t + "\n";
}

// 첨부: 이름/주소가 들어 있을 만한 항목을 차례로 찾는다
static void collect_attaches(const picojson::value &result, std::vector<remote_file> &files, int num)
{
	if ( !result.contains("attaches") || !result.get("attaches").is<picojson::array>() ) return;
	const picojson::array &a = result.get("attaches").get<picojson::array>();
	static const char *names[] = { "name", "fileName", "originalFileName", "originalName", "attachName", NULL };
	static const char *urls[] = { "url", "downloadUrl", "fileUrl", "attachUrl", "link", NULL };
	for (unsigned int i=0; i<a.size(); i++) {
		remote_file f; f.cookie = true;
		for (int k=0; names[k] && f.name.empty(); k++) f.name = jstr(a[i], names[k]);
		for (int k=0; urls[k] && f.url.empty(); k++) f.url = jstr(a[i], urls[k]);
		if ( f.name.empty() && !f.url.empty() ) f.name = url_file_name(f.url);
		if ( f.url.empty() ) {
			// 모르는 구조: 맞출 수 있게 남겨 둔다
			FILE *fp = fopen((state_dir + "/attach_unknown.log").c_str(), "a");
			if ( fp ) { fprintf(fp, "%d %s\n", num, a[i].serialize().c_str()); fclose(fp); }
			out("    첨부 정보를 읽지 못함 (data/cafe/attach_unknown.log 에 남김)");
			continue;
		}
		files.push_back(f);
	}
}

// 카페 회원을 BBS 회원으로 (로그인할 수 없는 비밀번호)
static void ensure_member(const std::string &uid, const std::string &nick)
{
	if ( database::exist_user_id((char*)uid.c_str()) ) return;
	std::string q = "INSERT INTO member (USER_ID, NICK_NAME, BIRTHDAY, PASSWORD, EMAIL, SEX, LEVEL, "
		"REGISTRATION_DATETIME, LASTLOGIN_DATETIME) VALUES ('" + database::escape(uid.c_str()) + "', '"
		+ database::escape(nick.c_str()) + "', '" + date_now_string(false) + "', "
		"'!', '', 1, 1, NOW(), NOW())";	// '!' 는 어떤 비밀번호와도 맞지 않는다 (로그인 불가)
	if ( mysql_query(mysql, q.c_str()) != 0 )
		out("    회원을 만들지 못함 (" + uid + "): " + mysql_error(mysql));
}

static bool table_exists(const std::string &t)
{
	std::string q = "SHOW TABLES LIKE '" + database::escape(t.c_str()) + "'";
	return database::fetch_rows((char*)q.c_str()).size() > 0;
}

// ------------------------------------------------------------------
// 받아 둔 글: data/cafe/download/<글 번호>/
//   meta.txt  키=값 (EUC-KR): board, menu_id, menu, title, nick, member_key, uid, date, time,
//             file=<저장 이름>\t<원래 이름> (여러 줄)
//   body.txt  BBS 에 들어갈 본문 (EUC-KR, 올리기 전에 고쳐도 된다)
//   files/    그림과 첨부
// ------------------------------------------------------------------
static std::string stage_dir(int num)
{
	char b[32]; snprintf(b, sizeof(b), "%d", num);
	return state_dir + "/download/" + b;
}

struct staged {
	int num;
	std::map<std::string, std::string> m;
	std::vector<std::pair<std::string, std::string> > files;	// 저장 이름, 원래 이름
};

static bool load_staged(int num, staged &s)
{
	std::string txt = read_file((stage_dir(num) + "/meta.txt").c_str());
	if ( txt.empty() ) return false;
	s.num = num;
	std::vector<std::string> lines = split_string(txt, '\n');
	for (unsigned int i=0; i<lines.size(); i++) {
		size_t e = lines[i].find('=');
		if ( e == std::string::npos ) continue;
		std::string k = lines[i].substr(0, e), v = lines[i].substr(e + 1);
		if ( k == "file" ) {
			size_t t = v.find('\t');
			if ( t != std::string::npos ) s.files.push_back(std::make_pair(v.substr(0, t), v.substr(t + 1)));
		} else s.m[k] = v;
	}
	return true;
}

static std::vector<int> staged_nums(void)
{
	std::vector<int> nums;
	std::vector<std::string> d = find_files(state_dir + "/download/*");
	for (unsigned int i=0; i<d.size(); i++) {
		int n = atoi(d[i].substr(d[i].rfind('/') + 1).c_str());
		if ( n > 0 ) nums.push_back(n);
	}
	std::sort(nums.begin(), nums.end());
	return nums;
}

static std::string one_line(std::string s)
{
	for (unsigned int i=0; i<s.size(); i++) if ( s[i] == '\n' || s[i] == '\r' || s[i] == '\t' ) s[i] = ' ';
	return s;
}

// ------------------------------------------------------------------
// 받기: BBS 는 바꾸지 않는다. 로그인 쿠키 문제면 false (전체를 멈춘다)
// ------------------------------------------------------------------
static bool fetch_one(int num, const std::map<int, std::string> &table, std::set<int> &done)
{
	char nb[32]; snprintf(nb, sizeof(nb), "%d", num);
	std::string head = std::string("[") + nb + "] ";

	if ( done.count(num) ) { n_skip_done++; out(head + "이미 BBS 에 올린 글"); return true; }
	staged tmp;
	if ( load_staged(num, tmp) ) { n_skip_done++; out(head + "이미 받아 둔 글"); return true; }

	picojson::value v; std::string err;
	std::string url = api_base + "/cafe-web/cafe-articleapi/v2.1/cafes/" + cafe_id
		+ "/articles/" + nb + "?useCafeId=true";
	if ( !get_json(url, v, err) ) {
		if ( err == "LOGIN" ) return false;
		n_fail++; out(head + "건너뜀: " + err);
		return true;
	}
	std::string article_json = last_json;	// 받은 글에 원본으로 남긴다 (작성자 프로필을 받으면 덮어써지므로)
	const picojson::value &result = v.get("result");
	const picojson::value &a = result.get("article");

	int menu_id = (int)a.get("menu").get("id").get<double>();
	std::string menu_name = u2c(jstr(a.get("menu"), "name"));
	std::map<int, std::string>::const_iterator it = table.find(menu_id);
	if ( it == table.end() ) {
		char mb[32]; snprintf(mb, sizeof(mb), "%d", menu_id);
		n_skip_menu++; out(head + "건너뜀: 메뉴 " + mb + " " + menu_name + " (table.txt 에 없음)");
		return true;
	}
	std::string board = it->second;
	if ( !table_exists(board) ) {
		n_fail++; out(head + "건너뜀: 게시판 테이블 " + board + " 이 없음");
		return true;
	}

	std::string title = u2c(jstr(a, "subject"));
	std::string hd = u2c(jstr(a, "head"));
	if ( !hd.empty() ) title = "[" + hd + "] " + title;
	std::string nick = u2c(jstr(a.get("writer"), "nick"));
	std::string member_key = jstr(a.get("writer"), "memberKey");
	std::string uid = member_id(member_key, nick);
	if ( uid.empty() ) return false;	// 로그인 문제

	time_t t = (time_t)(a.get("writeDate").get<double>() / 1000);
	struct tm tmv; localtime_r(&t, &tmv);
	char date[32], tim[32];
	strftime(date, sizeof(date), "%Y-%m-%d", &tmv);
	strftime(tim, sizeof(tim), "%H:%M:%S", &tmv);

	std::vector<remote_file> files;
	std::string content = html_to_text(jstr(a, "contentHtml"), files);
	collect_attaches(result, files, num);
	content += "\n" "(네이버 카페 원문: https://cafe.naver.com/olddos/" + std::string(nb) + ")\n";

	// 받기
	std::string dir = stage_dir(num);
	system(("mkdir -p " + shell_quote(dir + "/files")).c_str());
	{
		FILE *fj = fopen((dir + "/article.json").c_str(), "w");	// 카페 JSON 원본 (UTF-8, 문제 확인용)
		if ( fj ) { fputs(article_json.c_str(), fj); fclose(fj); }
	}
	std::string meta;
	meta += "board=" + board + "\n";
	char mid[32]; snprintf(mid, sizeof(mid), "%d", menu_id);
	meta += "menu_id=" + std::string(mid) + "\n";
	meta += "menu=" + one_line(menu_name) + "\n";
	meta += "title=" + one_line(title) + "\n";
	meta += "nick=" + one_line(nick) + "\n";
	meta += "member_key=" + member_key + "\n";
	meta += "uid=" + uid + "\n";
	meta += "date=" + std::string(date) + "\n";
	meta += "time=" + std::string(tim) + "\n";
	int got = 0;
	for (unsigned int i=0; i<files.size(); i++) {
		char sname[32]; snprintf(sname, sizeof(sname), "%02d", (int)i + 1);
		std::string dest = dir + "/files/" + sname;
		int code = http_get(files[i].url, dest, files[i].cookie);
		if ( code != 200 || file_size(dest) <= 0 ) {
			unlink(dest.c_str());
			char b[32]; snprintf(b, sizeof(b), "%d", code);
			// 어떤 주소였는지 (뒤의 ? 이하 인증 값은 빼고)
			std::string shown = files[i].url.substr(0, files[i].url.find('?'));
			out("    받기 실패 (HTTP " + std::string(b) + ") " + (files[i].cookie ? "첨부" : "그림") + " "
				+ u2c(files[i].name) + " <- " + shown + "  (원본: " + stage_dir(num) + "/article.json)");
			continue;
		}
		meta += "file=" + std::string(sname) + "\t" + one_line(u2c(files[i].name)) + "\n";
		got++;
	}
	FILE *fp = fopen((dir + "/body.txt").c_str(), "w");
	if ( fp ) { fputs(content.c_str(), fp); fclose(fp); }
	fp = fopen((dir + "/meta.txt").c_str(), "w");
	if ( fp ) { fputs(meta.c_str(), fp); fclose(fp); }

	n_ok++;
	n_files += got;
	char cnt[64]; snprintf(cnt, sizeof(cnt), " (첨부 %d)", got);
	out(head + "받음 -> " + board + ": " + string_truncate(title, 40, "...") + " / " + nick + "(" + uid + ")"
		+ (got ? cnt : ""));
	return true;
}

// ------------------------------------------------------------------
// 올리기: 받아 둔 글 하나를 BBS 에. 올렸으면 받은 폴더를 지운다
// ------------------------------------------------------------------
static bool apply_one(int num, std::set<int> &done)
{
	char nb[32]; snprintf(nb, sizeof(nb), "%d", num);
	std::string head = std::string("[") + nb + "] ";
	staged s;
	if ( !load_staged(num, s) ) { n_fail++; out(head + "받아 둔 글이 없음"); return true; }
	std::string dir = stage_dir(num);
	if ( done.count(num) ) {
		n_skip_done++; out(head + "이미 BBS 에 올린 글 (받은 폴더를 지움)");
		system(("rm -rf " + shell_quote(dir)).c_str());
		return true;
	}
	std::string board = s.m["board"];
	if ( board.empty() || !table_exists(board) ) { n_fail++; out(head + "게시판 테이블 " + board + " 이 없음"); return true; }

	// 작성자: 받은 뒤에 연결했을 수 있으므로 다시 정한다
	std::string uid = member_id(s.m["member_key"], s.m["nick"]);
	if ( uid.empty() ) return false;
	std::string content = read_file((dir + "/body.txt").c_str());

	ensure_member(uid, s.m["nick"]);
	int no = database::add_article((char*)board.c_str(), (char*)uid.c_str(), (char*)s.m["date"].c_str(),
		(char*)s.m["time"].c_str(), (char*)s.m["title"].c_str(), (char*)content.c_str());
	if ( no == -1 ) { n_fail++; out(head + "게시판에 넣지 못함"); return true; }
	database::attach_article((char*)board.c_str(), no, no);

	for (unsigned int i=0; i<s.files.size(); i++) {
		// 저장 이름: 영문자/숫자만
		static const char an[] = "abcdefghijklmnopqrstuvwxyz0123456789";
		char rnd[64];
		snprintf(rnd, sizeof(rnd), "cafe%d_%d_", no, (int)i);
		std::string tmpname = rnd;
		for (int k=0; k<8; k++) tmpname += an[rand() % 36];
		std::string src = dir + "/files/" + s.files[i].first;
		std::string dest = home + "/file/" + tmpname;
		std::string cmd = "cp " + shell_quote(src) + " " + shell_quote(dest);
		if ( system(cmd.c_str()) != 0 ) { out("    첨부를 옮기지 못함: " + s.files[i].second); continue; }
		chmod(dest.c_str(), 0644);
		database::add_attachment((char*)board.c_str(), no, (char*)uid.c_str(), (char*)s.m["date"].c_str(),
			(char*)s.m["time"].c_str(), (char*)tmpname.c_str(), (char*)s.files[i].second.c_str());
		n_files++;
	}

	mark_done(num);
	done.insert(num);
	system(("rm -rf " + shell_quote(dir)).c_str());
	n_ok++;
	out(head + "올림 -> " + board + ": " + string_truncate(s.m["title"], 40, "...") + " / "
		+ s.m["nick"] + "(" + uid + ")");
	return true;
}

// 받은 글 목록
static void list_staged(void)
{
	std::vector<int> nums = staged_nums();
	if ( nums.empty() ) { out("  받아 둔 글이 없습니다."); return; }
	for (unsigned int i=0; i<nums.size(); i++) {
		staged s;
		if ( !load_staged(nums[i], s) ) continue;
		// 작성자는 지금 연결 상태로 (캐시만 보고, 네이버에 묻지 않는다)
		std::string uid = trim(read_file((state_dir + "/links/" + s.m["member_key"]).c_str()));
		if ( uid.empty() ) uid = s.m["uid"];
		char b[512];
		snprintf(b, sizeof(b), "  %6d %-22s %s", nums[i], string_truncate(s.m["board"], 22, "").c_str(),
			string_truncate(s.m["title"], 44, "...").c_str());
		out(b);
		char nf[32] = "";
		if ( !s.files.empty() ) snprintf(nf, sizeof(nf), "첨부 %d", (int)s.files.size());
		snprintf(b, sizeof(b), "         %s(%s) %s %s", s.m["nick"].c_str(), uid.c_str(), s.m["date"].c_str(), nf);
		out(b);
	}
	char b[64]; snprintf(b, sizeof(b), "  모두 %d 개", (int)nums.size());
	out(b);
}

// 받은 글 보기 (20 줄씩)
static void show_staged(int num)
{
	staged s;
	if ( !load_staged(num, s) ) { out("  받아 둔 글이 없습니다."); return; }
	out("  제목: " + s.m["title"]);
	out("  작성: " + s.m["nick"] + "(" + s.m["uid"] + ")  " + s.m["date"] + " " + s.m["time"]);
	out("  카페 메뉴: " + s.m["menu"] + " -> 게시판 " + s.m["board"]);
	for (unsigned int i=0; i<s.files.size(); i++) out("  첨부: " + s.files[i].second);
	out("  ------------------------------------------------------------------------");
	std::vector<std::string> lines = split_string(read_file((stage_dir(num) + "/body.txt").c_str()), '\n');
	for (unsigned int i=0; i<lines.size(); i++) {
		out("  " + lines[i]);
		if ( (i + 1) % 18 == 0 && i + 1 < lines.size() ) {
			printf("  -- 계속: Enter, 그만: Q --");
			fflush(stdout);
			int c = getchar();
			printf("\r\n");
			if ( c == 'q' || c == 'Q' ) break;
		}
	}
}

// "103001-103050 103100" -> 번호 목록
static std::vector<int> parse_ranges(int argc, char **argv, int from)
{
	std::vector<int> nums;
	for (int i=from; i<argc; i++) {
		int a = 0, b = 0;
		if ( sscanf(argv[i], "%d-%d", &a, &b) == 2 ) {
			if ( b < a ) std::swap(a, b);
			if ( b - a > 5000 ) b = a + 5000;	// 실수 방지
			for (int n=a; n<=b; n++) nums.push_back(n);
		} else if ( (a = atoi(argv[i])) > 0 ) nums.push_back(a);
	}
	return nums;
}

int main(int argc, char **argv)
{
	if ( argc < 3 ) {
		printf("사용법: %s <tty> fetch <범위...> | list | show <번호> | drop <번호> | apply\n", argv[0]);
		printf("  예) %s - fetch 103001-103050 103100\n", argv[0]);
		return 1;
	}
	snprintf(tty, sizeof(tty), "%s", argv[1]);
	std::string mode = argv[2];
	home = getenv("HANULSO") ? getenv("HANULSO") : ".";
	if ( chdir(home.c_str()) != 0 ) { out("HANULSO 디렉터리로 갈 수 없습니다."); return 1; }
	if ( getenv("CAFE_API_BASE") ) api_base = getenv("CAFE_API_BASE");
	state_dir = home + "/data/cafe";
	system(("mkdir -p " + shell_quote(state_dir + "/members") + " " + shell_quote(state_dir + "/assigned") + " "
		+ shell_quote(state_dir + "/links") + " " + shell_quote(state_dir + "/tmp") + " "
		+ shell_quote(state_dir + "/download")).c_str());
	umask(0022);
	srand(time(0) ^ getpid());

	if ( mode == "list" ) { list_staged(); return 0; }
	if ( mode == "show" ) { show_staged(argc > 3 ? atoi(argv[3]) : 0); return 0; }
	if ( mode == "drop" ) {
		int n = argc > 3 ? atoi(argv[3]) : 0;
		staged s;
		if ( n <= 0 || !load_staged(n, s) ) { out("  받아 둔 글이 없습니다."); return 1; }
		system(("rm -rf " + shell_quote(stage_dir(n))).c_str());
		out("  " + std::string(argv[3]) + " 번 글을 뺐습니다.");
		return 0;
	}
	if ( mode != "fetch" && mode != "apply" ) { out("모르는 명령: " + mode); return 1; }

	read_settings("hanulso.cfg");
	pugi::xml_document doc;
	doc.load_file("hanulso.cfg");
	cafe_id = trim(doc.child("hanulso").child("naver").child("cafe_id").child_value());
	cookie = trim(doc.child("hanulso").child("naver").child("cookie").child_value());
	if ( cafe_id.empty() ) cafe_id = "28655511";
	// 쿠키는 받기에 필요하다 (올리기는 작성자 아이디를 아직 모를 때만)
	if ( mode == "fetch" && (cookie.empty() || cookie.find('"') != std::string::npos) ) {
		out("hanulso.cfg 에 <naver><cookie>NID_AUT=...; NID_SES=...</cookie></naver> 를 넣으세요.");
		return 1;
	}
	curl_cfg_cookie = state_dir + "/tmp/curl_cookie.cfg";
	curl_cfg_plain = state_dir + "/tmp/curl_plain.cfg";
	if ( !write_curl_cfg(curl_cfg_cookie, true) || !write_curl_cfg(curl_cfg_plain, false) ) {
		out("임시 파일을 만들 수 없습니다: " + state_dir + "/tmp");
		return 1;
	}

	std::map<int, std::string> table = load_table();
	std::set<int> done;
	load_done(home + "/src/restore/restore.log", done);
	load_done(state_dir + "/imported.log", done);

	if ( !database::open() ) { unlink(curl_cfg_cookie.c_str()); return 1; }

	bool login_ok = true;
	char b[160];
	if ( mode == "fetch" ) {
		std::vector<int> nums = parse_ranges(argc, argv, 3);
		snprintf(b, sizeof(b), "카페 %s: 글 %d 개 받기 (메뉴 대응 %d 개)", cafe_id.c_str(), (int)nums.size(), (int)table.size());
		out(b);
		out("  (멈추려면 Q: 지금 받는 글을 마치고 멈춥니다)");
		for (unsigned int i=0; i<nums.size(); i++) {
			if ( !fetch_one(nums[i], table, done) ) { login_ok = false; break; }
			// 네이버에 부담 주지 않게 1 초 쉬며 Q 를 본다
			if ( i + 1 < nums.size() && user_stop(1) ) {
				char r[96]; snprintf(r, sizeof(r), "  멈췄습니다. 남은 글: %d - %d", nums[i + 1], nums.back());
				out(r);
				break;
			}
		}
		snprintf(b, sizeof(b), "받음 %d, 이미 있음 %d, 메뉴 대응 없음 %d, 실패 %d, 첨부 파일 %d",
			n_ok, n_skip_done, n_skip_menu, n_fail, n_files);
	} else {
		std::vector<int> nums = (argc > 3) ? parse_ranges(argc, argv, 3) : staged_nums();
		snprintf(b, sizeof(b), "받아 둔 글 %d 개를 BBS 에 올립니다.", (int)nums.size());
		out(b);
		out("  (멈추려면 Q: 지금 올리는 글을 마치고 멈춥니다. 남은 글은 받은 그대로 남습니다)");
		for (unsigned int i=0; i<nums.size(); i++) {
			if ( !apply_one(nums[i], done) ) { login_ok = false; break; }
			if ( i + 1 < nums.size() && user_stop(0) ) { out("  멈췄습니다."); break; }
		}
		snprintf(b, sizeof(b), "올림 %d, 이미 올린 글 %d, 실패 %d, 첨부 파일 %d", n_ok, n_skip_done, n_fail, n_files);
	}
	database::close();
	unlink(curl_cfg_cookie.c_str());
	unlink(curl_cfg_plain.c_str());

	if ( !login_ok ) {
		out("");
		out("네이버 로그인이 되어 있지 않습니다. 쿠키가 만료되었을 수 있습니다.");
		out("브라우저에서 다시 로그인한 뒤 NID_AUT, NID_SES 쿠키를 hanulso.cfg 에 넣으세요.");
	}
	out("");
	out(b);
	return login_ok ? 0 : 2;
}
