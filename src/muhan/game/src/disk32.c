/* 자동 생성: tools/gen_disk32.py (손으로 고치지 말 것) */
#include <string.h>
#include <unistd.h>
#include "mstruct.h"
#include "disk32.h"

void lasttime_from_disk(lasttime *m, const d_lasttime *d)
{

	m->interval = (long)d->interval;
	m->ltime = (long)d->ltime;
	m->misc = d->misc;
}

void lasttime_to_disk(d_lasttime *d, const lasttime *m)
{

	d->interval = (int32_t)m->interval;
	d->ltime = (int32_t)m->ltime;
	d->misc = m->misc;
}

void daily_from_disk(daily *m, const d_daily *d)
{

	m->max = d->max;
	m->cur = d->cur;
	m->ltime = (long)d->ltime;
}

void daily_to_disk(d_daily *d, const daily *m)
{

	d->max = m->max;
	d->cur = m->cur;
	d->ltime = (int32_t)m->ltime;
}

void exit__from_disk(exit_ *m, const d_exit_ *d)
{
	int i, j;
	memcpy(m->name, d->name, sizeof(d->name));
	m->room = d->room;
	memcpy(m->flags, d->flags, sizeof(d->flags));
	lasttime_from_disk(&m->ltime, &d->ltime);
	m->key = d->key;
}

void exit__to_disk(d_exit_ *d, const exit_ *m)
{
	int i, j;
	memcpy(d->name, m->name, sizeof(d->name));
	d->room = m->room;
	memcpy(d->flags, m->flags, sizeof(d->flags));
	lasttime_to_disk(&d->ltime, &m->ltime);
	d->key = m->key;
}

void object_from_disk(object *m, const d_object *d)
{
	int i, j;
	memcpy(m->name, d->name, sizeof(d->name));
	memcpy(m->description, d->description, sizeof(d->description));
	memcpy(m->key, d->key, sizeof(d->key));
	memcpy(m->use_output, d->use_output, sizeof(d->use_output));
	m->value = (long)d->value;
	m->weight = d->weight;
	m->type = d->type;
	m->adjustment = d->adjustment;
	m->shotsmax = d->shotsmax;
	m->shotscur = d->shotscur;
	m->ndice = d->ndice;
	m->sdice = d->sdice;
	m->pdice = d->pdice;
	m->armor = d->armor;
	m->wearflag = d->wearflag;
	m->magicpower = d->magicpower;
	m->magicrealm = d->magicrealm;
	m->special = d->special;
	memcpy(m->flags, d->flags, sizeof(d->flags));
	m->questnum = d->questnum;
	m->first_obj = 0;
	m->parent_obj = 0;
	m->parent_rom = 0;
	m->parent_crt = 0;
}

void object_to_disk(d_object *d, const object *m)
{
	int i, j;
	memcpy(d->name, m->name, sizeof(d->name));
	memcpy(d->description, m->description, sizeof(d->description));
	memcpy(d->key, m->key, sizeof(d->key));
	memcpy(d->use_output, m->use_output, sizeof(d->use_output));
	d->value = (int32_t)m->value;
	d->weight = m->weight;
	d->type = m->type;
	d->adjustment = m->adjustment;
	d->shotsmax = m->shotsmax;
	d->shotscur = m->shotscur;
	d->ndice = m->ndice;
	d->sdice = m->sdice;
	d->pdice = m->pdice;
	d->armor = m->armor;
	d->wearflag = m->wearflag;
	d->magicpower = m->magicpower;
	d->magicrealm = m->magicrealm;
	d->special = m->special;
	memcpy(d->flags, m->flags, sizeof(d->flags));
	d->questnum = m->questnum;
	d->first_obj = 0;
	d->parent_obj = 0;
	d->parent_rom = 0;
	d->parent_crt = 0;
}

void room_from_disk(room *m, const d_room *d)
{
	int i, j;
	m->rom_num = d->rom_num;
	memcpy(m->name, d->name, sizeof(d->name));
	m->short_desc = 0;
	m->long_desc = 0;
	m->obj_desc = 0;
	m->lolevel = d->lolevel;
	m->hilevel = d->hilevel;
	m->special = d->special;
	m->trap = d->trap;
	m->trapexit = d->trapexit;
	memcpy(m->track, d->track, sizeof(d->track));
	memcpy(m->flags, d->flags, sizeof(d->flags));
	memcpy(m->random, d->random, sizeof(d->random));
	m->traffic = d->traffic;
	for (i = 0; i < 10; i++) lasttime_from_disk(&m->perm_mon[i], &d->perm_mon[i]);
	for (i = 0; i < 10; i++) lasttime_from_disk(&m->perm_obj[i], &d->perm_obj[i]);
	m->beenhere = (long)d->beenhere;
	m->established = (long)d->established;
	m->first_ext = 0;
	m->first_obj = 0;
	m->first_mon = 0;
	m->first_ply = 0;
}

void room_to_disk(d_room *d, const room *m)
{
	int i, j;
	d->rom_num = m->rom_num;
	memcpy(d->name, m->name, sizeof(d->name));
	d->short_desc = 0;
	d->long_desc = 0;
	d->obj_desc = 0;
	d->lolevel = m->lolevel;
	d->hilevel = m->hilevel;
	d->special = m->special;
	d->trap = m->trap;
	d->trapexit = m->trapexit;
	memcpy(d->track, m->track, sizeof(d->track));
	memcpy(d->flags, m->flags, sizeof(d->flags));
	memcpy(d->random, m->random, sizeof(d->random));
	d->traffic = m->traffic;
	for (i = 0; i < 10; i++) lasttime_to_disk(&d->perm_mon[i], &m->perm_mon[i]);
	for (i = 0; i < 10; i++) lasttime_to_disk(&d->perm_obj[i], &m->perm_obj[i]);
	d->beenhere = (int32_t)m->beenhere;
	d->established = (int32_t)m->established;
	d->first_ext = 0;
	d->first_obj = 0;
	d->first_mon = 0;
	d->first_ply = 0;
}

void creature_from_disk(creature *m, const d_creature *d)
{
	int i, j;
	memcpy(m->name, d->name, sizeof(d->name));
	memcpy(m->description, d->description, sizeof(d->description));
	memcpy(m->talk, d->talk, sizeof(d->talk));
	memcpy(m->password, d->password, sizeof(d->password));
	memcpy(m->key, d->key, sizeof(d->key));
	m->fd = d->fd;
	m->level = d->level;
	m->type = d->type;
	m->class = d->class;
	m->race = d->race;
	m->numwander = d->numwander;
	m->alignment = d->alignment;
	m->strength = d->strength;
	m->dexterity = d->dexterity;
	m->constitution = d->constitution;
	m->intelligence = d->intelligence;
	m->piety = d->piety;
	m->hpmax = d->hpmax;
	m->hpcur = d->hpcur;
	m->mpmax = d->mpmax;
	m->mpcur = d->mpcur;
	m->armor = d->armor;
	m->thaco = d->thaco;
	m->experience = (long)d->experience;
	m->gold = (long)d->gold;
	m->ndice = d->ndice;
	m->sdice = d->sdice;
	m->pdice = d->pdice;
	m->special = d->special;
	for (i = 0; i < 5; i++) m->proficiency[i] = (long)d->proficiency[i];
	for (i = 0; i < 4; i++) m->realm[i] = (long)d->realm[i];
	memcpy(m->spells, d->spells, sizeof(d->spells));
	memcpy(m->flags, d->flags, sizeof(d->flags));
	memcpy(m->quests, d->quests, sizeof(d->quests));
	m->questnum = d->questnum;
	memcpy(m->carry, d->carry, sizeof(d->carry));
	m->rom_num = d->rom_num;
	for (i = 0; i < MAXWEAR; i++) m->ready[i] = 0;
	for (i = 0; i < 10; i++) daily_from_disk(&m->daily[i], &d->daily[i]);
	for (i = 0; i < 45; i++) lasttime_from_disk(&m->lasttime[i], &d->lasttime[i]);
	m->following = 0;
	m->first_fol = 0;
	m->first_obj = 0;
	m->first_enm = 0;
	m->first_tlk = 0;
	m->parent_rom = 0;
}

void creature_to_disk(d_creature *d, const creature *m)
{
	int i, j;
	memcpy(d->name, m->name, sizeof(d->name));
	memcpy(d->description, m->description, sizeof(d->description));
	memcpy(d->talk, m->talk, sizeof(d->talk));
	memcpy(d->password, m->password, sizeof(d->password));
	memcpy(d->key, m->key, sizeof(d->key));
	d->fd = m->fd;
	d->level = m->level;
	d->type = m->type;
	d->class = m->class;
	d->race = m->race;
	d->numwander = m->numwander;
	d->alignment = m->alignment;
	d->strength = m->strength;
	d->dexterity = m->dexterity;
	d->constitution = m->constitution;
	d->intelligence = m->intelligence;
	d->piety = m->piety;
	d->hpmax = m->hpmax;
	d->hpcur = m->hpcur;
	d->mpmax = m->mpmax;
	d->mpcur = m->mpcur;
	d->armor = m->armor;
	d->thaco = m->thaco;
	d->experience = (int32_t)m->experience;
	d->gold = (int32_t)m->gold;
	d->ndice = m->ndice;
	d->sdice = m->sdice;
	d->pdice = m->pdice;
	d->special = m->special;
	for (i = 0; i < 5; i++) d->proficiency[i] = (int32_t)m->proficiency[i];
	for (i = 0; i < 4; i++) d->realm[i] = (int32_t)m->realm[i];
	memcpy(d->spells, m->spells, sizeof(d->spells));
	memcpy(d->flags, m->flags, sizeof(d->flags));
	memcpy(d->quests, m->quests, sizeof(d->quests));
	d->questnum = m->questnum;
	memcpy(d->carry, m->carry, sizeof(d->carry));
	d->rom_num = m->rom_num;
	for (i = 0; i < MAXWEAR; i++) d->ready[i] = 0;
	for (i = 0; i < 10; i++) daily_to_disk(&d->daily[i], &m->daily[i]);
	for (i = 0; i < 45; i++) lasttime_to_disk(&d->lasttime[i], &m->lasttime[i]);
	d->following = 0;
	d->first_fol = 0;
	d->first_obj = 0;
	d->first_enm = 0;
	d->first_tlk = 0;
	d->parent_rom = 0;
}

/* 파일에서 하나 읽기: 성공하면 1 */
int disk_read_object(int fd, object *m)
{
	d_object d;
	if (read(fd, &d, sizeof(d)) != (ssize_t)sizeof(d)) { memset(m, 0, sizeof(*m)); return 0; }
	object_from_disk(m, &d);
	return 1;
}

/* 파일에 하나 쓰기: 성공하면 1 */
int disk_write_object(int fd, const object *m)
{
	d_object d;
	memset(&d, 0, sizeof(d));
	object_to_disk(&d, m);
	return write(fd, &d, sizeof(d)) == (ssize_t)sizeof(d);
}

/* 메모리에서 하나 읽기: 읽은 바이트 수 */
int mem_read_object(const char *buf, object *m)
{
	d_object d;
	memcpy(&d, buf, sizeof(d));
	object_from_disk(m, &d);
	return sizeof(d);
}

/* 메모리에 하나 쓰기: 쓴 바이트 수 */
int mem_write_object(char *buf, const object *m)
{
	d_object d;
	memset(&d, 0, sizeof(d));
	object_to_disk(&d, m);
	memcpy(buf, &d, sizeof(d));
	return sizeof(d);
}

/* 파일에서 하나 읽기: 성공하면 1 */
int disk_read_creature(int fd, creature *m)
{
	d_creature d;
	if (read(fd, &d, sizeof(d)) != (ssize_t)sizeof(d)) { memset(m, 0, sizeof(*m)); return 0; }
	creature_from_disk(m, &d);
	return 1;
}

/* 파일에 하나 쓰기: 성공하면 1 */
int disk_write_creature(int fd, const creature *m)
{
	d_creature d;
	memset(&d, 0, sizeof(d));
	creature_to_disk(&d, m);
	return write(fd, &d, sizeof(d)) == (ssize_t)sizeof(d);
}

/* 메모리에서 하나 읽기: 읽은 바이트 수 */
int mem_read_creature(const char *buf, creature *m)
{
	d_creature d;
	memcpy(&d, buf, sizeof(d));
	creature_from_disk(m, &d);
	return sizeof(d);
}

/* 메모리에 하나 쓰기: 쓴 바이트 수 */
int mem_write_creature(char *buf, const creature *m)
{
	d_creature d;
	memset(&d, 0, sizeof(d));
	creature_to_disk(&d, m);
	memcpy(buf, &d, sizeof(d));
	return sizeof(d);
}

/* 파일에서 하나 읽기: 성공하면 1 */
int disk_read_room(int fd, room *m)
{
	d_room d;
	if (read(fd, &d, sizeof(d)) != (ssize_t)sizeof(d)) { memset(m, 0, sizeof(*m)); return 0; }
	room_from_disk(m, &d);
	return 1;
}

/* 파일에 하나 쓰기: 성공하면 1 */
int disk_write_room(int fd, const room *m)
{
	d_room d;
	memset(&d, 0, sizeof(d));
	room_to_disk(&d, m);
	return write(fd, &d, sizeof(d)) == (ssize_t)sizeof(d);
}

/* 메모리에서 하나 읽기: 읽은 바이트 수 */
int mem_read_room(const char *buf, room *m)
{
	d_room d;
	memcpy(&d, buf, sizeof(d));
	room_from_disk(m, &d);
	return sizeof(d);
}

/* 메모리에 하나 쓰기: 쓴 바이트 수 */
int mem_write_room(char *buf, const room *m)
{
	d_room d;
	memset(&d, 0, sizeof(d));
	room_to_disk(&d, m);
	memcpy(buf, &d, sizeof(d));
	return sizeof(d);
}

/* 파일에서 하나 읽기: 성공하면 1 */
int disk_read_exit_(int fd, exit_ *m)
{
	d_exit_ d;
	if (read(fd, &d, sizeof(d)) != (ssize_t)sizeof(d)) { memset(m, 0, sizeof(*m)); return 0; }
	exit__from_disk(m, &d);
	return 1;
}

/* 파일에 하나 쓰기: 성공하면 1 */
int disk_write_exit_(int fd, const exit_ *m)
{
	d_exit_ d;
	memset(&d, 0, sizeof(d));
	exit__to_disk(&d, m);
	return write(fd, &d, sizeof(d)) == (ssize_t)sizeof(d);
}

/* 메모리에서 하나 읽기: 읽은 바이트 수 */
int mem_read_exit_(const char *buf, exit_ *m)
{
	d_exit_ d;
	memcpy(&d, buf, sizeof(d));
	exit__from_disk(m, &d);
	return sizeof(d);
}

/* 메모리에 하나 쓰기: 쓴 바이트 수 */
int mem_write_exit_(char *buf, const exit_ *m)
{
	d_exit_ d;
	memset(&d, 0, sizeof(d));
	exit__to_disk(&d, m);
	memcpy(buf, &d, sizeof(d));
	return sizeof(d);
}
