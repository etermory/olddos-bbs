#include "main.h"

// BBÄÚµå¸¦ ±³Ã¼ÇÏ´Â printf ÇÔ¼ö
void bbcode_printf(const char *fmt,...)
{
	char buff[9072];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buff, sizeof(buff), fmt, ap);
	va_end(ap);

	printf("%s", replace_bbcode(buff).c_str());
}

// Ã¹ È­¸é Åë°è
struct bbs_stats {
	int members;
	int conns;
	int articles;
	int today;
};

// Åë°è¸¦ »õ·Î °è»ê
static void compute_bbs_stats(bbs_stats &s)
{
	char buf[9072];
	bool ok;

	s.members = 0;
	s.conns = 0;
	s.articles = 0;
	s.today = 0;

	// ÀüÃ¼ È¸¿ø ¼ö
	std::string count = database::fetch((char*)"SELECT COUNT(*) FROM member;", &ok);
	if ( ok ) s.members = atoi(count.c_str());

	// ÀüÃ¼ Á¢¼ÓÀÚ ¼ö: ·Î±×ÀÎÇÏ¸é tmp/<tty>.tty ¿¡ ¾ÆÀÌµğ¸¦ ±â·ÏÇÏ¹Ç·Î ³»¿ëÀÌ ÀÖ´Â ÆÄÀÏ ¼ö
	// (Á¢¼ÓÀÚ¸¶´Ù DB ¸¦ Á¶È¸ÇÏ¸é Á¢¼ÓÀÚ°¡ ¸¹À» ¶§ ºÎ´ãÀÌ Ä¿¼­ ÆÄÀÏ¸¸ È®ÀÎ)
	sprintf(buf, "%s/tmp/*.tty", getenv("HANULSO"));
	std::vector<std::string> files = find_files(buf);
	// (°­Á¦ Á¾·á·Î ³²Àº ÆÄÀÏÀº read_tty_file ÀÌ pid ¸¦ È®ÀÎÇØ Áö¿î´Ù)
	for(unsigned int i=0; i<files.size(); i++) {
		std::string user_id;
		if ( read_tty_file(files[i], user_id) ) {
			s.conns += 1;
		}
	}

	// ÀüÃ¼ °Ô½Ã±Û ¼ö, ¿À´ÃÀÇ °Ô½Ã±Û ¼ö
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

// Åë°è´Â 60ÃÊ µ¿¾È ÆÄÀÏ¿¡ Ä³½ÃÇØ¼­ ¿©·¯ Á¢¼ÓÀÚ°¡ ÇÔ²² ¾´´Ù
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

	// ´Ù¸¥ ÇÁ·Î¼¼½º°¡ ÀĞ´Â Áß¿¡ ¹İÂë ¾´ ÆÄÀÏÀ» º¸Áö ¾Êµµ·Ï ÀÓ½Ã ÆÄÀÏ¿¡ ¾²°í ÀÌ¸§À» ¹Ù²Û´Ù
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

// textutil ÀÌ ¸ğ¸£´Â [ÅÂ±×] ¸¦ ¹°¾îº¼ °÷ (BBS(main) Àº bbtags.cpp ÀÇ bbtag_value)
std::string (*bbcode_tag_hook)(const std::string &name, bool *found) = NULL;

// ¾î´À ÇÁ·Î±×·¥¿¡¼­³ª ¾Æ´Â ÅÂ±×. ¿¹Àü ÀÌ¸§(nummembers)°ú _ ÀÌ¸§(num_members) µÑ ´Ù
static bool basic_tag(const std::string &name, std::string &out, bbs_stats &s, bool &stats_loaded)
{
	if ( name == "hostname" || name == "host_name" ) {
		out = host_name;
		return true;
	}

	static const char *stat_names[][2] = {
		{ "nummembers", "num_members" }, { "numconns", "num_conns" },
		{ "numarticles", "num_articles" }, { "todaynumarticles", "today_num_articles" } };
	for ( int i = 0; i < 4; i++ ) {
		if ( name == stat_names[i][0] || name == stat_names[i][1] ) {
			if ( !stats_loaded ) {
				get_bbs_stats(s);
				stats_loaded = true;
			}
			int v[] = { s.members, s.conns, s.articles, s.today };
			out = int_string(v[i]);
			return true;
		}
	}

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	char buf[64];
	if ( name == "date" ) {
		strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
		out = buf;
		return true;
	}
	if ( name == "time" ) {
		strftime(buf, sizeof(buf), "%H:%M", tm);
		out = buf;
		return true;
	}
	if ( name == "weekday" ) {
		static const char *wday[] = { "ÀÏ", "¿ù", "È­", "¼ö", "¸ñ", "±İ", "Åä" };
		out = wday[tm->tm_wday];
		return true;
	}
	if ( name == "uptime" ) {
		long sec = atol(read_file("/proc/uptime").c_str());
		snprintf(buf, sizeof(buf), "%ldÀÏ %ld½Ã°£", sec / 86400, sec % 86400 / 3600);
		out = buf;
		return true;
	}
	return false;
}

// [ÅÂ±×:N] Ä­ ¸ÂÃã: N ÀÌ ¾ç¼ö¸é ¿À¸¥ÂÊ, À½¼ö¸é ¿ŞÂÊÀ¸·Î N Ä­. ±æ¸é ÀÚ¸¥´Ù
// È­¸é¿¡ º¸ÀÌ´Â Ä­ ¼ö (ESC[..±ÛÀÚ »ö ÄÚµå´Â »©°í ¼¾´Ù)
static int visible_width(const std::string &s)
{
	int w = 0;
	for ( std::string::size_type i = 0; i < s.size(); i++ ) {
		if ( s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[' ) {
			i += 2;
			while ( i < s.size() && !isalpha((unsigned char)s[i]) ) i++;
			continue;
		}
		w++;
	}
	return w;
}

// °¡¿îµ¥ ¸ÂÃã: ¾Õ¿¡ ºóÄ­À» ³Ö¾î width Ä­ÀÇ °¡¿îµ¥¿¡ ³õ´Â´Ù
static std::string center_in(const std::string &s, int width)
{
	int w = visible_width(s);
	return w >= width ? s : std::string((width - w) / 2, ' ') + s;
}

static std::string fit_width(const std::string &v, int width)
{
	if ( width == 0 ) return v;
	int w = width < 0 ? -width : width;
	std::string s = (int)v.size() > w ? string_truncate(v, w, "") : v;
	std::string pad((int)s.size() < w ? w - s.size() : 0, ' ');
	return width > 0 ? pad + s : s + pad;
}

// ±Û ¼ÓÀÇ [ÅÂ±×] ¸¦ °ªÀ¸·Î ¹Ù²Û´Ù. ÅÂ±× ÀÌ¸§Àº ¿µ¹® ¼Ò¹®ÀÚ/¼ıÀÚ/_ ,
// ¸ğ¸£´Â ÅÂ±×³ª [Enter] Ã³·³ ÅÂ±×°¡ ¾Æ´Ñ °ÍÀº ±×´ë·Î µĞ´Ù.
//   [ÀÌ¸§]  [ÀÌ¸§:8]  [ÀÌ¸§:-8]
//   [ÀÌ¸§?¹®Àå %s ¹®Àå]  °ªÀÌ ºñ¾ú°Å³ª 0 ÀÌ¸é ÅëÂ°·Î ºóÄ­, ¾Æ´Ï¸é %s ÀÚ¸®¿¡ °ª
std::string replace_bbcode(std::string text)
{
	std::string out;
	bbs_stats s;
	bool stats_loaded = false;
	std::string::size_type pos = 0;
	while ( 1 ) {
		std::string::size_type b = text.find('[', pos);
		if ( b == std::string::npos ) {
			out.append(text, pos, std::string::npos);
			break;
		}
		std::string::size_type e = text.find(']', b + 1);
		std::string name, value;
		int width = 0;
		bool center = false;
		bool found = false;
		std::string tmpl;
		bool conditional = false;
		if ( e != std::string::npos && e - b <= 200 ) {
			std::string inner = text.substr(b + 1, e - b - 1);
			std::string::size_type q = inner.find('?');
			if ( q != std::string::npos ) {
				tmpl = inner.substr(q + 1);
				conditional = true;
				inner = inner.substr(0, q);
			}
			std::string::size_type colon = inner.find(':');
			name = inner.substr(0, colon);
			if ( colon != std::string::npos ) {
				// [ÀÌ¸§:^N] °¡¿îµ¥ ¸ÂÃã
				center = inner.size() > colon + 1 && inner[colon + 1] == '^';
				width = atoi(inner.c_str() + colon + (center ? 2 : 1));
			}
			bool valid = !name.empty();
			for ( unsigned int i = 0; i < name.size(); i++ ) {
				char c = name[i];
				if ( !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') ) valid = false;
			}
			if ( valid ) {
				found = basic_tag(name, value, s, stats_loaded);
				if ( !found && bbcode_tag_hook != NULL ) value = bbcode_tag_hook(name, &found);
			}
		}
		if ( found ) {
			out.append(text, pos, b - pos);
			if ( center ) {
				// °¡¿îµ¥ ¸ÂÃã: ¹®ÀåÀÌ ÀÖÀ¸¸é ¹®Àå ÀüÃ¼¸¦ width Ä­ °¡¿îµ¥¿¡. ³ÑÄ¡¸é °ªÀ» ÁÙÀÎ´Ù
				if ( !conditional || (!value.empty() && value != "0") ) {
					std::string::size_type ps = tmpl.find("%s");
					std::string before = conditional ? (ps == std::string::npos ? tmpl : tmpl.substr(0, ps)) : "";
					std::string after = (conditional && ps != std::string::npos) ? tmpl.substr(ps + 2) : "";
					int room = width - visible_width(before) - visible_width(after);
					if ( room < 0 ) room = 0;
					std::string v = (int)value.size() > room ? string_truncate(value, room, "") : value;
					if ( conditional && ps == std::string::npos ) v = "";
					out += center_in(before + v + after, width);
				}
			} else if ( !conditional ) {
				out += fit_width(value, width);
			} else if ( !value.empty() && value != "0" ) {
				std::string::size_type ps = tmpl.find("%s");
				out += (ps == std::string::npos) ? tmpl : tmpl.substr(0, ps) + fit_width(value, width) + tmpl.substr(ps + 2);
			}
			pos = e + 1;
		} else {
			// ÅÂ±×°¡ ¾Æ´Ï¸é '[' ¸¸ ³Ñ±â°í °è¼Ó (ESC[ ´ÙÀ½¿¡ ¿À´Â ÅÂ±×µµ Ã£µµ·Ï)
			out.append(text, pos, b + 1 - pos);
			pos = b + 1;
		}
	}
	text = out;

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
	
	// »ö º¯°æ
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

// Á¢¼ÓÇÑ ÅÍ¹Ì³ÎÀÇ ÁÙ ¼ö (BBS ´Â ·Î±×ÀÎÇÒ ¶§ terminal_rows() ·Î ¾Ë¾Æ³½´Ù)
int screen_rows = 24;

void print_file(const char *filename)
{
    char buf[9072];

    sprintf(buf,"%s/%s", getenv("HANULSO"), filename);

	// È­¸é ÁÙ ¼ö¿¡ ¸ÂÃç µû·Î ¸¸µç ÆÄÀÏ. ÀÌ¸§ ³¡ ¼ıÀÚ°¡ ±× ÆÄÀÏÀÌ ¾²´Â È­¸é ÁÙ ¼ö´Ù.
	//   top.txt (25 ÁÙ), top24.txt (24 ÁÙ), top28.txt (28 ÁÙ ÀÌ»ó) ...
	// È­¸é¿¡ µé¾î°¡´Â °Í Áß °¡Àå Å« °ÍÀ» ¾´´Ù. (24 ÁÙ È­¸éÀÌ¸é top24.txt, 29 ÁÙÀÌ¸é top28.txt)
	std::string path = buf;
	std::string::size_type dot = path.rfind('.');
	if ( dot != std::string::npos && path.find('/', dot) == std::string::npos ) {
		std::string stem = path.substr(0, dot), ext = path.substr(dot);
		int top = screen_rows > 99 ? 99 : screen_rows;
		for ( int n = top; n >= 24; n-- ) {
			if ( n == 25 ) break;		// 25 ÁÙÀº ÀÌ¸§¿¡ ¼ıÀÚ°¡ ¾ø´Â ±âº» ÆÄÀÏ
			char alt[9200];
			snprintf(alt, sizeof(alt), "%s%d%s", stem.c_str(), n, ext.c_str());
			if ( access(alt, R_OK) == 0 ) {
				snprintf(buf, sizeof(buf), "%s", alt);
				break;
			}
		}
	}
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
