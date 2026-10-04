
Introduction
-
This program aims to implement a BBS (Bulletin Board System) in the style popular in Korea during the 1990s. Currently, the program is being developed and operated on CentOS 6.9 (https://bbsweb.oscc.kr/).

License
-
This program is licensed under the LGPL.

The MUD driver under `src/jurassic/driver` (MudOS) and the restored Jurassic Park 2 mudlib (`src/jurassic/jp2_v15.tgz`) are not covered by the LGPL; see the MUD Games section below.

MUD Games
-
The front page menu **7. 머드 게임** (`go game`) offers two text games.

### 1. 용사의 전설 (Legend of the Hero) — `go hero`

A single-player door game in the style of old BBS door games, written from scratch for this BBS (`src/hero.cpp`, built as `bin/hero`).

- A village with a forest, master duels for leveling up, weapon/armor shops, an item store, a healer and a bank
- 15 forest fights per day; if you fall, you lose the gold you carry and wake up the next day
- Levels 1–12, 66 monsters, 20 weapons, 20 armors, 10 accessories and 4 consumables
- Defeat the red dragon at level 12 to earn a hero title and start over
- ASCII art for every scene, a two-pane battle screen with HP bars, shared rankings and village news (MySQL tables `game_hero`, `game_news`)

### 2. 쥬라기공원 2 (Jurassic Park 2) — `go jurassic`

쥬라기공원 was one of the first commercial Korean MUDs, serviced on PC 통신 (Chollian) from 1994.
This is the **HanLP restoration of Jurassic Park 2** (JuDessic Park 1.5, 2001, by MaGuN), ported from the modified HanLP driver to the original **MudOS v22.2b14**.

- `src/jurassic/driver` — MudOS v22.2b14 from [maldorne/mudos](https://github.com/maldorne/mudos) with Korean patches:
  - `packages/hangul.c` — Korean particle efuns used by the HanLP mudlib (`han_iga` 이/가, `han_obj` 을/를, `han_tool` 으로/로, ...), using a table of the 2,350 KS X 1001 syllables
  - `add_action.c` — Korean word order: the last word is the verb (`칼 가져` = "get sword")
  - `backend.c` — heart beats never fired with glibc's `ualarm()` for intervals of one second or more; uses `setitimer()` instead
  - `local_options` — options the HanLP mudlib expects (`PACKAGE_HANGUL`, `INTERACTIVE_CATCH_TELL`, ...)
- `src/jurassic/jp2_v15.tgz` — the original mudlib (EUC-KR)
- `src/jurassic/libpatch` — files laid over the original mudlib on install: functions that used to live in the HanLP driver (`ktime`, combat messages, ...) and the admin character
- `build.sh` builds the driver (64-bit by default, `BITS=32` for 32-bit), `deploy.sh` installs it with the mudlib, `bin/startmud` keeps it running
- The MUD listens on `127.0.0.1:4444`. The BBS connects users through `bin/mudlink` (`src/mudlink.cpp`), a small relay that only talks to that local port, handles line editing and Korean backspace, hides passwords, and returns to the BBS on `끝` or `/x`.

See **머드 게임 연결** in INSTALL.TXT for build, install, update and admin instructions.

Credits and license notes:
- MudOS is copyright Lars Pensjö, Erik Kay, Adam Beeman, Stephan Iannce, John Garnett and Tim Hollebeek, and **may not be used for monetary gain** (see `src/jurassic/driver/Copyright`). Run it for non-commercial purposes only.
- The Jurassic Park 2 restoration is by MaGuN (HanLP), using area data from the 구공원 library released by 크루젼 (이상신) and 꼬마기사 (김진태) on the Nownuri MUD club. The original 쥬라기공원 was created by 송재경 and 김성배. Rights to the original game have not been confirmed.

Install
-
For installation instructions, please refer to the INSTALL.txt file.

Configuration
-
Before installation, you will need the hanulso.cfg file. Please create it as follows.

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
		<user>admin</user>	<!-- admin -->
		<user>admin2</user>	<!-- another admin -->		
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

Screenshots
-
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/7e9b6b06-dbe8-4587-a79e-c77295dfa7db" />
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/c1919b16-0487-4b2c-8026-2b2dafb58ec1" />
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/df5e2bb2-b220-4701-916e-d28b977600e2" />
<img width="850" height="739" alt="image" src="https://github.com/user-attachments/assets/a678b5da-d84f-4b62-908c-a467c53ffc83" />

