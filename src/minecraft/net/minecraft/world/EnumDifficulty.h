#ifndef C919_SOURCE_ENUM_DIFFICULTY_H
#define C919_SOURCE_ENUM_DIFFICULTY_H
#include "nbt/NBTString.h"
typedef struct EnumDifficulty {
    MCObject object;
    int32_t difficultyId;
    NBTString *difficultyResourceKey;
} EnumDifficulty;
typedef struct EnumDifficultyStatics {
    MCObject object;
    EnumDifficulty *PEACEFUL,*EASY,*NORMAL,*HARD;
    EnumDifficulty *difficultyEnums[4];
} EnumDifficultyStatics;
/* Original declared fields/constructor/getters over native managed enum
   identities. java.lang.Enum and generated values/valueOf remain adapters. */
EnumDifficultyStatics *EnumDifficulty_getStatics(MCObjectHeap *);
bool EnumDifficulty_isInstance(const MCObject *);
int32_t EnumDifficulty_getDifficultyId(EnumDifficulty *);
NBTString *EnumDifficulty_getDifficultyResourceKey(EnumDifficulty *);
EnumDifficulty *EnumDifficulty_getDifficultyEnum(MCObjectHeap *,int32_t);
#endif
