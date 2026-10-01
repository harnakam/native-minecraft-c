#ifndef C919_SOURCE_SCORE_CRITERIA_H
#define C919_SOURCE_SCORE_CRITERIA_H
#include "nbt/NBTString.h"
#include "util/NativeReferenceList.h"
typedef struct IScoreObjectiveCriteria IScoreObjectiveCriteria;
/* Immutable native enum identity adapter; not the generated JDK enum class. */
typedef struct {
  int32_t ordinal;
  const char *name;
} ScoreRenderType;
extern const ScoreRenderType ScoreRenderType_INTEGER, ScoreRenderType_HEARTS;
typedef struct {
  NBTString *(*getName)(MCObject *, IScoreObjectiveCriteria *);
  bool (*setScore)(MCObject *, IScoreObjectiveCriteria *, NativeReferenceList *,
                   int32_t *);
  bool (*isReadOnly)(MCObject *, IScoreObjectiveCriteria *, bool *);
  bool (*getRenderType)(MCObject *, IScoreObjectiveCriteria *,
                        const ScoreRenderType **);
} IScoreObjectiveCriteriaMethods;
struct IScoreObjectiveCriteria {
  MCObject object;
  const IScoreObjectiveCriteriaMethods *methods;
  MCObject *context;
};
bool IScoreObjectiveCriteria_isInstance(const MCObject *);
IScoreObjectiveCriteria *IScoreObjectiveCriteria_nativeNew(
    MCObjectHeap *, const IScoreObjectiveCriteriaMethods *, MCObject *);
void IScoreObjectiveCriteria_traceFields(IScoreObjectiveCriteria *,
                                         MCObjectVisitor, void *);
NBTString *IScoreObjectiveCriteria_getName(IScoreObjectiveCriteria *);
bool IScoreObjectiveCriteria_isReadOnly(IScoreObjectiveCriteria *);
const ScoreRenderType *
IScoreObjectiveCriteria_getRenderType(IScoreObjectiveCriteria *);
bool IScoreObjectiveCriteria_setScore(IScoreObjectiveCriteria *,
                                      NativeReferenceList *, int32_t *);
/* Mutable per-heap INSTANCES dependency, direct refs/overwrite semantics. Full
   interface class initialization/health/color classes are not claimed. */
bool IScoreObjectiveCriteria_register(IScoreObjectiveCriteria *,
                                      NBTString *name);
IScoreObjectiveCriteria *IScoreObjectiveCriteria_find(MCObjectHeap *,
                                                      NBTString *name);
#endif
