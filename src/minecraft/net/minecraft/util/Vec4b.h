#ifndef C919_VEC4B_H
#define C919_VEC4B_H
#include "util/NativeJavaClass.h"

/* Source Vec4b's complete declared state and methods. The native allocator and
   Class literal are explicit lifetime/reflection adapters, not Object or JDK
   class-initialization ports. Only this concrete translated runtime class is
   recognized; arbitrary unported Java subclasses/overrides are not inferred
   from a name or matching layout. */
typedef struct Vec4b {
    MCObject object;
    int8_t field_176117_a;
    int8_t field_176115_b;
    int8_t field_176116_c;
    int8_t field_176114_d;
} Vec4b;

typedef enum Vec4bResult { VEC4B_OK, VEC4B_EXCEPTION, VEC4B_FAILURE } Vec4bResult;

/* Immutable process-lifetime class fact with the canonical Object ancestor.
   nativeClass retains one managed literal per heap. Call it before requesting
   getClass through the existing native reflection adapter for a Vec4b. */
extern const NativeJavaClassDescriptor Vec4b_Class;
NativeJavaClass *Vec4b_nativeClass(MCObjectHeap *);
bool Vec4b_isInstance(const MCObject *);

/* Separate allocation permits callers to preserve Java new-before-argument
   evaluation. Constructors never allocate a replacement receiver. Borrowed
   results must be retained by their caller before GC/adoption. */
Vec4b *Vec4b_nativeAllocate(MCObjectHeap *);
bool Vec4b_construct(Vec4b *, int8_t, int8_t, int8_t, int8_t);
Vec4bResult Vec4b_constructCopy(Vec4b *, const Vec4b *);
Vec4b *Vec4b_new(MCObjectHeap *, int8_t, int8_t, int8_t, int8_t);
/* NULL source throws before the first field assignment: EXCEPTION leaves the
   heap healthy. newCopy allocates first, updates *out only on success, and
   keeps any failed constructor's allocation until native collection. */
Vec4bResult Vec4b_newCopy(MCObjectHeap *, const Vec4b *, Vec4b **out);

int8_t Vec4b_func_176110_a(const Vec4b *);
int8_t Vec4b_func_176112_b(const Vec4b *);
int8_t Vec4b_func_176113_c(const Vec4b *);
int8_t Vec4b_func_176111_d(const Vec4b *);
bool Vec4b_equals(Vec4b *, MCObject *);
int32_t Vec4b_hashCode(const Vec4b *);
/* Native invalid receiver/type/size/heap or allocation failure is sticky
   FAILURE; scalar defaults are only unwind values. equals accepts nullable
   or tracked same-heap Object refs, and a different runtime class is false. */
#endif
