#ifndef C919_NATIVE_COLLECTION_H
#define C919_NATIVE_COLLECTION_H
#include "util/NativePrimitiveArray.h"
/* Reached JDK Collection.toArray boundary, not a complete Collection class.
   NativeReferenceList uses its captured storage/size; the actual translated
   ClassInheritanceMultiMap uses size then iterator. Snapshots are shallow,
   managed fixed Object arrays. Unknown receiver types fail, never become an
   empty collection. Caller retains the returned borrowed reference. */
NativeObjectArray *NativeCollection_toArray(MCObject *collection);
#endif
