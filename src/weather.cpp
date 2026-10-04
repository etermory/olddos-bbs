#include "main.h"

struct termio sys_term;

char title[1024] = "날씨 정보";

char tty[10];

char host_name[256];

// 예보 지역 (기상청 동네예보 서비스가 폐지되어 Open-Meteo 를 사용)
struct region {
	const char *sido;
	const char *name;
	double lat;
	double lon;
};

static const region regions[] = {
	{ "서울특별시", "종로구", 37.57, 126.98 },
	{ "서울특별시", "마포구", 37.57, 126.90 },
	{ "서울특별시", "강서구", 37.55, 126.85 },
	{ "서울특별시", "노원구", 37.65, 127.06 },
	{ "서울특별시", "강남구", 37.52, 127.05 },
	{ "서울특별시", "송파구", 37.51, 127.11 },
	{ "부산광역시", "중구", 35.10, 129.03 },
	{ "부산광역시", "해운대구", 35.16, 129.16 },
	{ "부산광역시", "사하구", 35.10, 128.97 },
	{ "부산광역시", "북구", 35.20, 128.99 },
	{ "대구광역시", "중구", 35.87, 128.60 },
	{ "대구광역시", "달서구", 35.83, 128.53 },
	{ "대구광역시", "수성구", 35.86, 128.63 },
	{ "인천광역시", "중구", 37.47, 126.62 },
	{ "인천광역시", "남동구", 37.45, 126.73 },
	{ "인천광역시", "부평구", 37.51, 126.72 },
	{ "인천광역시", "강화군", 37.75, 126.49 },
	{ "인천광역시", "백령도", 37.97, 124.71 },
	{ "광주광역시", "동구", 35.15, 126.92 },
	{ "광주광역시", "북구", 35.17, 126.91 },
	{ "광주광역시", "광산구", 35.14, 126.79 },
	{ "대전광역시", "동구", 36.31, 127.45 },
	{ "대전광역시", "중구", 36.33, 127.42 },
	{ "대전광역시", "서구", 36.36, 127.38 },
	{ "대전광역시", "유성구", 36.36, 127.36 },
	{ "대전광역시", "대덕구", 36.35, 127.42 },
	{ "울산광역시", "남구", 35.54, 129.33 },
	{ "울산광역시", "동구", 35.50, 129.42 },
	{ "울산광역시", "울주군", 35.52, 129.24 },
	{ "세종특별자치시", "세종", 36.48, 127.29 },
	{ "경기도", "수원", 37.26, 127.03 },
	{ "경기도", "성남", 37.42, 127.13 },
	{ "경기도", "고양", 37.66, 126.83 },
	{ "경기도", "용인", 37.24, 127.18 },
	{ "경기도", "부천", 37.50, 126.78 },
	{ "경기도", "안산", 37.32, 126.83 },
	{ "경기도", "안양", 37.39, 126.92 },
	{ "경기도", "의정부", 37.74, 127.03 },
	{ "경기도", "평택", 36.99, 127.11 },
	{ "경기도", "파주", 37.76, 126.78 },
	{ "경기도", "이천", 37.27, 127.44 },
	{ "경기도", "가평", 37.83, 127.51 },
	{ "경기도", "양평", 37.49, 127.49 },
	{ "강원도", "춘천", 37.88, 127.73 },
	{ "강원도", "원주", 37.34, 127.92 },
	{ "강원도", "강릉", 37.75, 128.88 },
	{ "강원도", "속초", 38.21, 128.59 },
	{ "강원도", "동해", 37.52, 129.11 },
	{ "강원도", "태백", 37.16, 128.99 },
	{ "강원도", "철원", 38.15, 127.31 },
	{ "강원도", "평창", 37.37, 128.39 },
	{ "충청북도", "청주", 36.64, 127.49 },
	{ "충청북도", "충주", 36.99, 127.93 },
	{ "충청북도", "제천", 37.13, 128.19 },
	{ "충청북도", "보은", 36.49, 127.73 },
	{ "충청북도", "영동", 36.18, 127.78 },
	{ "충청남도", "천안", 36.81, 127.11 },
	{ "충청남도", "아산", 36.79, 127.00 },
	{ "충청남도", "공주", 36.45, 127.12 },
	{ "충청남도", "보령", 36.33, 126.61 },
	{ "충청남도", "서산", 36.78, 126.45 },
	{ "충청남도", "논산", 36.19, 127.10 },
	{ "충청남도", "홍성", 36.60, 126.66 },
	{ "전라북도", "전주", 35.82, 127.15 },
	{ "전라북도", "군산", 35.97, 126.74 },
	{ "전라북도", "익산", 35.95, 126.96 },
	{ "전라북도", "정읍", 35.57, 126.86 },
	{ "전라북도", "남원", 35.42, 127.39 },
	{ "전라북도", "무주", 36.01, 127.66 },
	{ "전라남도", "목포", 34.81, 126.39 },
	{ "전라남도", "여수", 34.76, 127.66 },
	{ "전라남도", "순천", 34.95, 127.49 },
	{ "전라남도", "나주", 35.02, 126.71 },
	{ "전라남도", "광양", 34.94, 127.70 },
	{ "전라남도", "해남", 34.57, 126.60 },
	{ "전라남도", "완도", 34.31, 126.76 },
	{ "경상북도", "포항", 36.02, 129.34 },
	{ "경상북도", "경주", 35.86, 129.22 },
	{ "경상북도", "안동", 36.57, 128.73 },
	{ "경상북도", "구미", 36.12, 128.34 },
	{ "경상북도", "김천", 36.14, 128.11 },
	{ "경상북도", "영주", 36.81, 128.62 },
	{ "경상북도", "상주", 36.41, 128.16 },
	{ "경상북도", "울진", 36.99, 129.40 },
	{ "경상북도", "울릉도", 37.48, 130.90 },
	{ "경상남도", "창원", 35.23, 128.68 },
	{ "경상남도", "진주", 35.18, 128.11 },
	{ "경상남도", "김해", 35.23, 128.89 },
	{ "경상남도", "통영", 34.85, 128.43 },
	{ "경상남도", "거제", 34.88, 128.62 },
	{ "경상남도", "양산", 35.34, 129.04 },
	{ "경상남도", "밀양", 35.50, 128.75 },
	{ "경상남도", "거창", 35.69, 127.91 },
	{ "제주특별자치도", "제주시", 33.50, 126.53 },
	{ "제주특별자치도", "서귀포", 33.25, 126.56 },
	{ "제주특별자치도", "성산", 33.46, 126.93 },
	{ "제주특별자치도", "고산", 33.29, 126.16 },
};

static const int region_count = sizeof(regions) / sizeof(regions[0]);

// 시간별 예보
struct forecast {
	std::string time;	// YYYY-MM-DDTHH:MM
	double temp;
	int pop;
	int code;
	double ws;
	int wd;
};

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

/* 프로그램 종료 루틴 */
int host_close (void)
{
    ioctl(0, TCSETAF, &sys_term);
    exit(1);
}

void prompt(char *cmd)
{
	printf(ESC_ENG);
	printf("이동(번호) 상위메뉴(P) 종료(X)\r\n");
	printf("선택 >> ");
	line_input(cmd, 30);

	std::vector<std::string> args = split_string(std::string(cmd), ' ');
	if ( args.size() == 0 ) return;

	/* 입력이 명령 코드 */
	if( !is_number(cmd) ) {
		// 종료 명령
		if ( !strcasecmp(args[0].c_str(), "x") ) {
			host_close();
		}
	}
}

// 화면 상단 헤더 출력
void print_header(const char *head_title)
{
    printf("\033[1;1H");
	printf("\033[=9F\033[=1G%s\033[=15F\033[=1G", repeat("─", 40).c_str());
    printf("\033[1;1H");
	printf("\033[1A\033[7m%s\033[0m", host_name);
	// 타이틀 출력
	int center = (80-strlen(strip_ansi_codes(head_title)))/2;
	if ( center < 0 ) center = 0;
    printf("\033[2;1H");
	printf("\r\033[%dC%s", center, head_title);
    printf("\033[3;1H");
	printf("\033[=0F\033[=1G%s\033[=15F\033[=1G", repeat("━", 40).c_str());
}

// 목록에서 하나를 고른다. 선택한 번호(0 부터), 상위메뉴(P)면 -1
int select_menu(const std::string &head_title, const std::vector<std::string> &names)
{
	int n = names.size();

	int col = 2;
	int width = 30;
	if ( n <= 15 ) {
		col = 1;
		width = 30;
	} else if ( n > 40 ) {
		col = 3;
		width = 20;
	}
	int rows = (n + col - 1) / col;

	while (1) {
		printf(ESC_CLEAR);
		print_header(head_title.c_str());

        printf("\033[4;1H");
		for (int i=0; i<n; i++) {
			printf("%5d%s", i+1, centered(names[i], width).c_str());
			if ( (i+1) % col == 0 && i+1 < n ) {
				printf("\r\n");
			}
		}

        printf("\033[%d;1H", rows+4);
		printf("%s\r\n", repeat("━", 40).c_str());

		char cmd[1024];
		prompt(cmd);

		/* 입력이 숫자 */
		if( is_number(cmd) ) {
			int no = atoi(cmd);
			if ( no >= 1 && no <= n ) {
				return no - 1;
			}
		} else {
			if (!strcasecmp(cmd, "p")) {
				return -1;
			}
		}
	}
}

// 날씨 코드 (WMO) 를 우리말로
std::string weather_name(int code)
{
	switch (code) {
		case 0: return "맑음";
		case 1: return "구름조금";
		case 2: return "구름많음";
		case 3: return "흐림";
		case 45: case 48: return "안개";
		case 51: case 53: case 55: return "이슬비";
		case 56: case 57: return "어는 이슬비";
		case 61: return "약한 비";
		case 63: return "비";
		case 65: return "강한 비";
		case 66: case 67: return "어는 비";
		case 71: return "약한 눈";
		case 73: return "눈";
		case 75: return "강한 눈";
		case 77: return "싸락눈";
		case 80: case 81: return "소나기";
		case 82: return "강한 소나기";
		case 85: case 86: return "소낙눈";
		case 95: return "뇌우";
		case 96: case 99: return "뇌우/우박";
	}
	return "-";
}

// 풍향 (도) 을 16 방위로
std::string wind_direction(int deg)
{
	static const char *dirs[] = {
		"북", "북북동", "북동", "동북동", "동", "동남동", "남동", "남남동",
		"남", "남남서", "남서", "서남서", "서", "서북서", "북서", "북북서"
	};
	if ( deg < 0 ) deg = 0;
	int idx = (int)((deg % 360) / 22.5 + 0.5) % 16;
	return dirs[idx];
}

int round_int(double v)
{
	return (v < 0) ? (int)(v - 0.5) : (int)(v + 0.5);
}

// 오늘부터 days 일 뒤의 날짜 (YYYY-MM-DD)
std::string date_after(int days)
{
	time_t t = time(NULL) + (time_t)days * 86400;
	struct tm *tm = localtime(&t);
	char buf[32];
	strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
	return buf;
}

// Open-Meteo 에서 예보를 받아 온다 (CSV 형식)
bool get_forecast(const region &r, std::vector<forecast> &hourly,
		std::map<std::string, std::pair<double, double> > &daily)
{
	char url[1024];
	snprintf(url, sizeof(url), "http://api.open-meteo.com/v1/forecast"
			"?latitude=%.2f&longitude=%.2f"
			"&hourly=temperature_2m,precipitation_probability,weather_code,wind_speed_10m,wind_direction_10m"
			"&daily=temperature_2m_max,temperature_2m_min"
			"&timezone=Asia%%2FSeoul&forecast_days=3&wind_speed_unit=ms&format=csv",
			r.lat, r.lon);

    char tmpdir[1024];
    char tmpname[1024];
	snprintf(tmpdir, sizeof(tmpdir), "%s/tmp", getenv("HANULSO"));
	snprintf(tmpname, sizeof(tmpname), "%s.csv", tempnam(tmpdir, "weather"));

	char cmd[2048];
	snprintf(cmd, sizeof(cmd), "wget -q -T 10 -t 2 -O %s %s",
			shell_quote(tmpname).c_str(), shell_quote(url).c_str());
	int a = system(cmd);

	std::string csv = read_file(tmpname);
	unlink(tmpname);

	if ( WEXITSTATUS(a) != 0 || csv.empty() ) {
		return false;
	}

	// 시간별 표와 일별 표가 빈 줄로 나뉘어 온다
	std::vector<std::string> lines = split_string(csv, '\n');
	int section = 0;	// 1: 시간별, 2: 일별
	for (unsigned int i=0; i<lines.size(); i++) {
		std::string line = trim(lines[i]);
		if ( line.compare(0, 5, "time,") == 0 ) {
			section = ( line.find("_max") != std::string::npos ) ? 2 : 1;
			continue;
		}
		if ( line.empty() || !isdigit((unsigned char)line[0]) ) continue;

		std::vector<std::string> cols = split_string(line, ',');
		if ( section == 1 && cols.size() >= 6 ) {
			forecast f;
			f.time = cols[0];
			f.temp = atof(cols[1].c_str());
			f.pop = atoi(cols[2].c_str());
			f.code = cols[3].empty() ? -1 : atoi(cols[3].c_str());
			f.ws = atof(cols[4].c_str());
			f.wd = atoi(cols[5].c_str());
			hourly.push_back(f);
		} else if ( section == 2 && cols.size() >= 3 ) {
			daily[cols[0]] = std::make_pair(atof(cols[1].c_str()), atof(cols[2].c_str()));
		}
	}

	return hourly.size() > 0;
}

void show_info(const region &r)
{
	std::string head_title = std::string(title) + " : " + r.sido + " " + r.name;

    printf(ESC_CLEAR);
	print_header(head_title.c_str());

    printf("\033[4;1H");
    printf("자료를 받아오는 중입니다...");
    fflush(stdout);

	std::vector<forecast> hourly;
	std::map<std::string, std::pair<double, double> > daily;
	if ( !get_forecast(r, hourly, daily) ) {
		printf("\r\n\r\n날씨 정보를 받아오지 못했습니다.\r\n");
		printf("\r\n[Enter] 를 누르세요.");
		press_enter();
		return;
	}

	// -----------------------------------------------------------
    printf("\033[4;1H\033[K");
    printf("%5s %8s %13s %15s %10s %10s %10s\r\n",
            "시간", "날짜", "온도(고/저)", "날씨", "강수확률", "풍향", "풍속(m/s)");
    printf("\033[5;1H");
    printf("%s", repeat("─", 40).c_str());

	// 지금 시각 이후, 3 시간 간격으로 16 개 (이틀치)
	char now[32];
	time_t t = time(NULL);
	strftime(now, sizeof(now), "%Y-%m-%dT%H", localtime(&t));

	std::string today = date_after(0);
	std::string tomorrow = date_after(1);
	std::string after = date_after(2);

    printf("\033[6;1H");
	int shown = 0;
	for (unsigned int i=0; i<hourly.size() && shown < 16; i++) {
		const forecast &f = hourly[i];
		if ( f.time.size() < 13 ) continue;
		if ( f.time.compare(0, 13, now) < 0 ) continue;

		int hour = atoi(f.time.substr(11, 2).c_str());
		if ( hour % 3 != 0 ) continue;

		std::string date = f.time.substr(0, 10);
		std::string day;
		if ( date == today ) day = "오늘";
		else if ( date == tomorrow ) day = "내일";
		else if ( date == after ) day = "모레";
		else day = date.substr(5);

		char hour_s[16];
		snprintf(hour_s, sizeof(hour_s), "%d시", hour);

		char temp[64];
		if ( daily.count(date) ) {
			snprintf(temp, sizeof(temp), "%2d (%2d/%2d)", round_int(f.temp),
					round_int(daily[date].first), round_int(daily[date].second));
		} else {
			snprintf(temp, sizeof(temp), "%2d", round_int(f.temp));
		}

		char pop[16];
		snprintf(pop, sizeof(pop), "%d%%", f.pop);

		printf("%5s %8s %13s %15s %10s %10s %10.1f\r\n",
				hour_s,
				day.c_str(),
				temp,
				weather_name(f.code).c_str(),
				pop,
				wind_direction(f.wd).c_str(),
				f.ws);
		shown++;
	}

    printf("%s", repeat("─", 40).c_str());
    printf("\r\n 자료: Open-Meteo.com   [Enter] 를 누르세요.");
    press_enter();
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

	// 시/도 목록
	std::vector<std::string> sidos;
	for (int i=0; i<region_count; i++) {
		if ( std::find(sidos.begin(), sidos.end(), regions[i].sido) == sidos.end() ) {
			sidos.push_back(regions[i].sido);
		}
	}

    while (1) {
		int s = select_menu(title, sidos);
		if ( s < 0 ) break;

		// 선택한 시/도의 지역 목록
		std::vector<std::string> names;
		std::vector<int> index;
		for (int i=0; i<region_count; i++) {
			if ( sidos[s] == regions[i].sido ) {
				names.push_back(regions[i].name);
				index.push_back(i);
			}
		}

		std::string sub_title = std::string(title) + " : " + sidos[s];
		while (1) {
			int n = select_menu(sub_title, names);
			if ( n < 0 ) break;

			show_info(regions[index[n]]);
		}
    }

	host_close();
}
