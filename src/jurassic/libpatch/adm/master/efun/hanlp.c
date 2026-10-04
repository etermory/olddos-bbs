// hanlp.c : 원래 HanLP 드라이버에 들어 있던 함수들 (원본 MudOS 로 옮기면서 LPC 로 다시 만듦)
// 한글 조사 함수(han_iga 등)는 드라이버의 hangul 패키지에 있다.

// 파일 내용을 str 로 바꾼다 (덮어쓰기)
int save_file(string file, string str)
{
	return write_file(file, str, 1);
}

// 두 배열로 매핑 만들기 (keys[i] : values[i])
mapping merge(mixed *keys, mixed *values)
{
	mapping m;
	int i, n;

	m = ([]);
	if( !keys || !values ) return m;
	n = sizeof(keys);
	if( sizeof(values) < n ) n = sizeof(values);
	for( i = 0; i < n; i++ ) m[keys[i]] = values[i];
	return m;
}

// 메모리에 올라온 오브젝트 수
int all_object_size(mixed flag)
{
	return sizeof(objects());
}

// 2005년 10월 4일(화) 오후 3시 5분 7초
string ktime(int t)
{
	mixed *lt;
	string *wday, ampm;
	int hour;

	wday = ({ "일", "월", "화", "수", "목", "금", "토" });
	lt = localtime(t);
	hour = lt[2];
	ampm = (hour < 12) ? "오전" : "오후";
	if( hour == 0 ) hour = 12;
	else if( hour > 12 ) hour -= 12;
	return sprintf("%4d년 %d월 %d일(%s) %s %d시 %d분 %d초",
		lt[5], lt[4] + 1, lt[3], wday[lt[6]], ampm, hour, lt[1], lt[0]);
}

// 공격할 상대가 없을 때
string no_combat_msg()
{
	string *msgs;

	msgs = ({
		"* 상대가 없습니다.",
		"* 적이 없습니다.",
		"* 당신은 주위를 둘러 보았지만 공격할만한 대상이 없습니다.",
		"* 공격할 대상이 없습니다.",
		"* 당신은 허공을 향해 주먹질을 합니다.",
		"* 당신의 공격이 허공으로 흩어집니다.",
		"* 누구를 공격할까요?",
		"* 적이 이곳에 없습니다.",
		"* 공격할 대상을 찾을 수 없습니다.",
		"* 적을 발견하지 못했습니다.",
	});
	return "\n" + msgs[random(sizeof(msgs))] + "\n\n";
}

// 피해 정도를 말로 (피해가 클수록 뒤쪽 말)
string han_damage_msg(int damage)
{
	string *msgs;
	int i;

	msgs = ({
		"하나도 안아프게", "아주 살짝", "건드리듯이", "살짝 빗겨", "살짝",
		"가볍게", "조금 약하게", "약하게", "조금 아프게", "나즈막한 소리를 내며",
		"소리를 지르며", "크게 소리를 지르며", "아프게", "재빠르게", "조금 강하게",
		"강하게", "아주 강하게", "강하게 두 번", "강하게 여러번", "엄청 강하게",
		"무지 강하게", "무지무지 강하게", "묵직하게", "체중을 실어", "아찔하게",
		"무식하게", "눈물이 핑 돌정도로", "별이 보일정도로", "꿰 뚫어 버릴 정도로", "뭉개버릴 정도로",
		"혼이 나갈 정도로", "뇌성과 함께", "말하기 싫을 정도로", "무지막지하게", "호흡이 곤란할 정도로",
		"번개처럼", "살기 싫을 정도로", "혜성같이", "뼈가 시릴 정도로", "혼이 빠지도록",
		"혼신의 힘으로", "뼈가 부서지도록", "영혼이 훌쩍일만큼", "소멸시켜 버릴듯이",
	});
	if( damage < 10 ) i = damage / 2;
	else i = 5 + (damage - 10) / 4;
	if( i < 0 ) i = 0;
	if( i >= sizeof(msgs) ) i = sizeof(msgs) - 1;
	return msgs[i];
}
