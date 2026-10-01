#include "stats/Achievement.h"
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    Achievement *achievement=(Achievement *)object;
    StatBase_trace(object,visitor,context);
    achievement->parentAchievement=(Achievement *)visitor((MCObject *)achievement->parentAchievement,context);
}
const MCObjectClass c919_achievement_class={"net.minecraft.stats.Achievement",MCObjectHeap_plainClone,trace,NULL};
Achievement *Achievement_newIdentity(MCObjectHeap *heap,NBTString *id,Achievement *parent) {
    if ((id && ((MCObject *)id)->heap!=heap) ||
        (parent && (parent->base.object.heap!=heap || !StatBase_isAchievement(&parent->base)))) {
        MCObjectHeap_fail(heap); return NULL;
    }
    Achievement *achievement=(Achievement *)MCObjectHeap_alloc(heap,sizeof(*achievement),&c919_achievement_class);
    if (achievement) { achievement->base.statId=id; achievement->parentAchievement=parent; }
    return achievement;
}
Achievement *Achievement_initIndependentStat(Achievement *achievement) {
    if (achievement) StatBase_initIndependentStat(&achievement->base);
    return achievement;
}
Achievement *Achievement_setSpecial(Achievement *achievement) {
    if (achievement) { achievement->isSpecial=true; MCObjectHeap_touch(achievement->base.object.heap); }
    return achievement;
}
bool Achievement_getSpecial(const Achievement *achievement) { return achievement && achievement->isSpecial; }
bool Achievement_isAchievement(const Achievement *achievement) { (void)achievement; return true; }
