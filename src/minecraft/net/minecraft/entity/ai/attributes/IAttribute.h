#ifndef C919_SOURCE_I_ATTRIBUTE_H
#define C919_SOURCE_I_ATTRIBUTE_H
#include "nbt/NBTString.h"
typedef struct IAttribute IAttribute;
typedef struct {
    NBTString *(*getAttributeUnlocalizedName)(IAttribute *);
    double (*clampValue)(IAttribute *,double);
    double (*getDefaultValue)(IAttribute *);
    bool (*getShouldWatch)(IAttribute *);
    IAttribute *(*func_180372_d)(IAttribute *);
} IAttributeMethods;
struct IAttribute { MCObject object; const IAttributeMethods *methods; };
NBTString *IAttribute_getAttributeUnlocalizedName(IAttribute *);
double IAttribute_clampValue(IAttribute *,double);
double IAttribute_getDefaultValue(IAttribute *);
bool IAttribute_getShouldWatch(IAttribute *);
IAttribute *IAttribute_func_180372_d(IAttribute *);
bool IAttribute_isInstance(const MCObject *);
bool IAttribute_equals(IAttribute *,IAttribute *);
int32_t IAttribute_hashCode(IAttribute *);
#endif
