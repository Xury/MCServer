
#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "Cow.h"





// TODO: Milk Cow





cCow::cCow(void) :
	super("Cow", 92, "mob.cow.hurt", "mob.cow.hurt", 0.9, 1.3)
{
}





void cCow::GetDrops(cItems & a_Drops, cEntity * a_Killer)
{
	AddRandomDropItem(a_Drops, 0, 2, E_ITEM_LEATHER);
	AddRandomDropItem(a_Drops, 1, 3, IsOnFire() ? E_ITEM_STEAK : E_ITEM_RAW_BEEF);
}




