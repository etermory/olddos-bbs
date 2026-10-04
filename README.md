
소개
-
1990년대 한국 PC통신 시절의 BBS(전자게시판)를 재현하는 프로그램입니다. 현재 CentOS 6.9 에서 개발하고 운영하고 있습니다. (https://bbsweb.oscc.kr/)

라이선스
-
이 프로그램은 LGPL 라이선스를 따릅니다.

`src/jurassic/driver` 의 머드 엔진(MudOS)과 쥬라기공원 2 복원판 머드 라이브러리(`src/jurassic/jp2_v15.tgz`)는 LGPL 대상이 아닙니다. 아래 머드 게임 항목을 참고하세요.

설치
-
설치 방법은 INSTALL.TXT 파일을 참고하세요.

설정
-
설치 전에 hanulso.cfg 파일이 필요합니다. 아래와 같이 만드세요.

```xml
<?xml version="1.0" encoding="euc-kr"?>
<hanulso>
	<name>TITLE</name>
	<database>
		<name>db_name</name>
		<host>db_host</host>
		<user>db_user</user>
		<password>db_user_password</password>
	</database>
	<mailserver>
		<host>smtp.naver.com</host>
		<port>587</port>
		<user>user@naver.com</user>
		<password>user_password</password>
	</mailserver>
	<sysop>
		<user>admin</user>	<!-- 운영자 -->
		<user>admin2</user>	<!-- 다른 운영자 -->
	</sysop>
	<article>
		<show_max_line>15</show_max_line>
		<ks_conversion>true</ks_conversion>
	</article>
	<level>
		<alias>1,일반회원</alias>
		<alias>2,특별회원</alias>
	</level>
</hanulso>
```

머드 게임
-
대문 메뉴의 **7. 머드 게임** (`go game`) 에서 두 가지 텍스트 게임을 즐길 수 있습니다.

### 1. 용사의 전설 — `go hero`

옛 BBS 도어 게임 방식의 1인용 텍스트 RPG 로, 이 BBS 를 위해 새로 만든 게임입니다. 여럿이 같은 세계에서 만나는 머드와 달리 혼자 진행하고, 다른 용사와는 순위와 마을 소식으로 이어집니다. (`src/hero.cpp`, `bin/hero`)

- 마을: 숲 사냥, 사부님과의 결투(레벨업), 대장간/갑옷 가게, 잡화점, 약방, 전장(돈 맡기기)
- 숲 사냥은 하루 15 번. 쓰러지면 지니고 있던 돈을 잃고 다음 날 깨어납니다
- 레벨 1~12, 몬스터 66 종, 무기 20 종, 갑옷 20 종, 장신구 10 종, 소모품 4 종
- 12 레벨에 붉은 용을 쓰러뜨리면 영웅 칭호를 얻고 처음부터 다시 시작합니다
- 장면마다 아스키 그림, 체력 막대가 있는 2 단 전투 화면, 함께 보는 용사 순위와 마을 소식 (MySQL `game_hero`, `game_news` 테이블)

<img width="850" alt="용사의 전설 타이틀 화면" src="docs/screenshots/hero.png" />

### 2. 쥬라기공원 2 — `go jurassic`

쥬라기공원은 1994 년 천리안에서 서비스를 시작한 국내 초기 상용 머드입니다.
이 게임은 **쥬라기공원 2 의 HanLP 복원판** (JuDessic Park 1.5, 2001, MaGuN) 을 개조된 HanLP 드라이버 대신 원본 **MudOS v22.2b14** 에서 돌아가도록 옮긴 것입니다.

- `src/jurassic/driver` — [maldorne/mudos](https://github.com/maldorne/mudos) 의 MudOS v22.2b14 + 한글 수정
  - `packages/hangul.c` — HanLP 라이브러리가 쓰는 한글 조사 함수 (`han_iga` 이/가, `han_obj` 을/를, `han_tool` 으로/로 등). 완성형 2,350 자의 받침 표 사용
  - `add_action.c` — 한국어 어순: 마지막 단어를 명령으로 (`칼 가져`)
  - `backend.c` — glibc 의 `ualarm()` 이 1 초 이상 값을 거부해 heart_beat 가 돌지 않던 문제를 `setitimer()` 로 해결
  - `local_options` — HanLP 라이브러리가 기대하는 옵션 (`PACKAGE_HANGUL`, `INTERACTIVE_CATCH_TELL` 등)
- `src/jurassic/jp2_v15.tgz` — 원본 머드 라이브러리 (EUC-KR)
- `src/jurassic/libpatch` — 설치할 때 원본 위에 덮어쓰는 파일: 원래 HanLP 드라이버에 있던 함수(`ktime`, 전투 메시지 등)와 운영자 설정
- `build.sh` 로 엔진을 빌드하고 (기본 64 비트, `BITS=32` 로 32 비트), `deploy.sh` 로 라이브러리와 함께 설치하며, `bin/startmud` 가 머드를 계속 띄워 둡니다
- 머드는 `127.0.0.1:4444` 에서 접속을 받습니다. BBS 는 `bin/mudlink` (`src/mudlink.cpp`) 로 사용자를 연결합니다. 이 프로그램은 로컬 머드 포트에만 접속하고, 줄 입력과 한글 백스페이스, 비밀번호 숨김을 처리하며, `끝` 이나 `/x` 로 BBS 에 돌아옵니다.

빌드, 설치, 업데이트, 운영자 설정은 INSTALL.TXT 의 **머드 게임 연결** 을 참고하세요.

<img width="850" alt="쥬라기공원 2 접속 화면" src="docs/screenshots/jurassic.png" />

출처와 라이선스:
- MudOS 의 저작권은 Lars Pensjö, Erik Kay, Adam Beeman, Stephan Iannce, John Garnett, Tim Hollebeek 에게 있으며 **금전적 이익을 위해 사용할 수 없습니다** (`src/jurassic/driver/Copyright`). 비상업 용도로만 운영하세요.
- 쥬라기공원 2 복원판은 MaGuN (HanLP) 이 만들었고, 크루젼(이상신)님과 꼬마기사(김진태)님이 나우누리 머드동호회에 공개한 구공원 라이브러리의 지역 데이터를 사용했습니다. 원작 쥬라기공원은 송재경, 김성배 님이 만들었습니다. 원작의 권리 관계는 확인되지 않았습니다.

화면
-
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/7e9b6b06-dbe8-4587-a79e-c77295dfa7db" />
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/c1919b16-0487-4b2c-8026-2b2dafb58ec1" />
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/df5e2bb2-b220-4701-916e-d28b977600e2" />
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/a678b5da-d84f-4b62-908c-a467c53ffc83" />
