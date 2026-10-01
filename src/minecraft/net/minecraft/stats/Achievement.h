#ifndef C919_ACHIEVEMENT_H
#define C919_ACHIEVEMENT_H
#include "stats/StatBase.h"
typedef struct Achievement {
    StatBase base;
    struct Achievement *parentAchievement;
    bool isSpecial;
} Achievement;
/* Explicit identity dependency constructor. GUI column/row/icon, chat and
   registration-list side effects of the original constructors remain pending. */
Achievement *Achievement_newIdentity(MCObjectHeap *,NBTString *id,Achievement *parent);
Achievement *Achievement_initIndependentStat(Achievement *);
Achievement *Achievement_setSpecial(Achievement *);
bool Achievement_getSpecial(const Achievement *);
bool Achievement_isAchievement(const Achievement *);
#endif
