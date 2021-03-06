// items.h - X macro item data (see https://wikipedia.org/wiki/X_Macro)

X(NO_ITEM,        none,   "None",                 "",             0)
X(HEART2,         none,   "",                     "",             2)
X(HEART3,         none,   "",                     "",             3)
X(BOMB_1,         none,   "",                     "●",            1)
X(BOMB_3,         none,   "",                     "●",            3)
X(SHOVEL_BASIC,   shovel, "Base Shovel",          BROWN "(",   DIRT)
X(SHOVEL_TIT,     shovel, "Titanium Shovel",      "(",        STONE)
X(DAGGER,         weapon, "Base Dagger",          ")",            1)
X(DAGGER_JEWELED, weapon, "Jeweled Dagger",       BLUE ")",       5)
X(HEAD_MINERS,    head,   "Miner’s Cap",          BLACK "[",     16)
X(HEAD_CIRCLET,   head,   "Circlet of Telepathy", YELLOW "[",     0)
X(BOOTS_LUNGING,  feet,   "Lunging",              "[",            0)
X(BOOTS_LEAPING,  feet,   "Frog Socks",           GREEN "[",      0)
X(TRANSPLANT,     usable, "Heart Transplant",     RED "ღ",        0)
X(SCROLL_FREEZE,  usable, "Freeze Scroll",        "?",            0)

#undef X
