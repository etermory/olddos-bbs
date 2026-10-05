/* 자동 생성: tools/gen_disk32.py (손으로 고치지 말 것) */
#ifndef DISK32_H
#define DISK32_H
#include <stdint.h>
/* mstruct.h 다음에 포함할 것 (mstruct.h 에는 중복 포함 방지가 없다) */

typedef struct d_lasttime {
	int32_t interval;
	int32_t ltime;
	short misc;
} d_lasttime;

typedef struct d_daily {
	char max;
	char cur;
	int32_t ltime;
} d_daily;

typedef struct d_exit_ {
	char name[20];
	short room;
	char flags[4];
	d_lasttime ltime;
	char key;
} d_exit_;

typedef struct d_object {
	char name[80];
	char description[80];
	char key[3][20];
	char use_output[80];
	int32_t value;
	short weight;
	char type;
	char adjustment;
	short shotsmax;
	short shotscur;
	short ndice;
	short sdice;
	short pdice;
	char armor;
	char wearflag;
	char magicpower;
	char magicrealm;
	short special;
	char flags[8];
	char questnum;
	uint32_t first_obj;
	uint32_t parent_obj;
	uint32_t parent_rom;
	uint32_t parent_crt;
} d_object;

typedef struct d_room {
	short rom_num;
	char name[80];
	uint32_t short_desc;
	uint32_t long_desc;
	uint32_t obj_desc;
	char lolevel;
	char hilevel;
	short special;
	char trap;
	short trapexit;
	char track[80];
	char flags[8];
	short random[10];
	char traffic;
	d_lasttime perm_mon[10];
	d_lasttime perm_obj[10];
	int32_t beenhere;
	int32_t established;
	uint32_t first_ext;
	uint32_t first_obj;
	uint32_t first_mon;
	uint32_t first_ply;
} d_room;

typedef struct d_creature {
	char name[80];
	char description[80];
	char talk[80];
	char password[15];
	char key[3][20];
	short fd;
	unsigned char level;
	char type;
	char class;
	char race;
	char numwander;
	short alignment;
	char strength;
	char dexterity;
	char constitution;
	char intelligence;
	char piety;
	short hpmax;
	short hpcur;
	short mpmax;
	short mpcur;
	char armor;
	char thaco;
	int32_t experience;
	int32_t gold;
	short ndice;
	short sdice;
	short pdice;
	short special;
	int32_t proficiency[5];
	int32_t realm[4];
	char spells[16];
	char flags[8];
	char quests[16];
	char questnum;
	short carry[10];
	short rom_num;
	uint32_t ready[MAXWEAR];
	d_daily daily[10];
	d_lasttime lasttime[45];
	uint32_t following;
	uint32_t first_fol;
	uint32_t first_obj;
	uint32_t first_enm;
	uint32_t first_tlk;
	uint32_t parent_rom;
} d_creature;

void lasttime_from_disk(lasttime *m, const d_lasttime *d);
void lasttime_to_disk(d_lasttime *d, const lasttime *m);
void daily_from_disk(daily *m, const d_daily *d);
void daily_to_disk(d_daily *d, const daily *m);
void exit__from_disk(exit_ *m, const d_exit_ *d);
void exit__to_disk(d_exit_ *d, const exit_ *m);
void object_from_disk(object *m, const d_object *d);
void object_to_disk(d_object *d, const object *m);
void room_from_disk(room *m, const d_room *d);
void room_to_disk(d_room *d, const room *m);
void creature_from_disk(creature *m, const d_creature *d);
void creature_to_disk(d_creature *d, const creature *m);
int disk_read_object(int fd, object *m);
int disk_write_object(int fd, const object *m);
int mem_read_object(const char *buf, object *m);
int mem_write_object(char *buf, const object *m);
int disk_read_creature(int fd, creature *m);
int disk_write_creature(int fd, const creature *m);
int mem_read_creature(const char *buf, creature *m);
int mem_write_creature(char *buf, const creature *m);
int disk_read_room(int fd, room *m);
int disk_write_room(int fd, const room *m);
int mem_read_room(const char *buf, room *m);
int mem_write_room(char *buf, const room *m);
int disk_read_exit_(int fd, exit_ *m);
int disk_write_exit_(int fd, const exit_ *m);
int mem_read_exit_(const char *buf, exit_ *m);
int mem_write_exit_(char *buf, const exit_ *m);

#endif
