#include <stdio.h>
#include <stddef.h>
#include "mstruct.h"
#ifdef DISK
#include "disk32.h"
#define T(s) d_##s
#else
#define T(s) s
#endif
#define F(s, f) printf(#s "." #f " %u %u\n", (unsigned)offsetof(T(s), f), (unsigned)sizeof(((T(s) *)0)->f))
int main(void)
{
	printf("lasttime %u\n", (unsigned)sizeof(T(lasttime)));
	F(lasttime, interval);
	F(lasttime, ltime);
	F(lasttime, misc);
	printf("daily %u\n", (unsigned)sizeof(T(daily)));
	F(daily, max);
	F(daily, cur);
	F(daily, ltime);
	printf("exit_ %u\n", (unsigned)sizeof(T(exit_)));
	F(exit_, name);
	F(exit_, room);
	F(exit_, flags);
	F(exit_, ltime);
	F(exit_, key);
	printf("object %u\n", (unsigned)sizeof(T(object)));
	F(object, name);
	F(object, description);
	F(object, key);
	F(object, use_output);
	F(object, value);
	F(object, weight);
	F(object, type);
	F(object, adjustment);
	F(object, shotsmax);
	F(object, shotscur);
	F(object, ndice);
	F(object, sdice);
	F(object, pdice);
	F(object, armor);
	F(object, wearflag);
	F(object, magicpower);
	F(object, magicrealm);
	F(object, special);
	F(object, flags);
	F(object, questnum);
	F(object, first_obj);
	F(object, parent_obj);
	F(object, parent_rom);
	F(object, parent_crt);
	printf("room %u\n", (unsigned)sizeof(T(room)));
	F(room, rom_num);
	F(room, name);
	F(room, short_desc);
	F(room, long_desc);
	F(room, obj_desc);
	F(room, lolevel);
	F(room, hilevel);
	F(room, special);
	F(room, trap);
	F(room, trapexit);
	F(room, track);
	F(room, flags);
	F(room, random);
	F(room, traffic);
	F(room, perm_mon);
	F(room, perm_obj);
	F(room, beenhere);
	F(room, established);
	F(room, first_ext);
	F(room, first_obj);
	F(room, first_mon);
	F(room, first_ply);
	printf("creature %u\n", (unsigned)sizeof(T(creature)));
	F(creature, name);
	F(creature, description);
	F(creature, talk);
	F(creature, password);
	F(creature, key);
	F(creature, fd);
	F(creature, level);
	F(creature, type);
	F(creature, class);
	F(creature, race);
	F(creature, numwander);
	F(creature, alignment);
	F(creature, strength);
	F(creature, dexterity);
	F(creature, constitution);
	F(creature, intelligence);
	F(creature, piety);
	F(creature, hpmax);
	F(creature, hpcur);
	F(creature, mpmax);
	F(creature, mpcur);
	F(creature, armor);
	F(creature, thaco);
	F(creature, experience);
	F(creature, gold);
	F(creature, ndice);
	F(creature, sdice);
	F(creature, pdice);
	F(creature, special);
	F(creature, proficiency);
	F(creature, realm);
	F(creature, spells);
	F(creature, flags);
	F(creature, quests);
	F(creature, questnum);
	F(creature, carry);
	F(creature, rom_num);
	F(creature, ready);
	F(creature, daily);
	F(creature, lasttime);
	F(creature, following);
	F(creature, first_fol);
	F(creature, first_obj);
	F(creature, first_enm);
	F(creature, first_tlk);
	F(creature, parent_rom);
	return 0;
}
