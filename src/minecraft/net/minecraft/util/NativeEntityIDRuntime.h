#ifndef C919_NATIVE_ENTITY_ID_RUNTIME_H
#define C919_NATIVE_ENTITY_ID_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>
/* Native process/static counter service for original Entity.nextEntityID.
   Not graph state or persistent data. Borrowed services outlive all callers;
   the default process service is never freed. Fixture services are independent.
   Atomic calls are a native concurrency boundary, not Java constructor locks. */
typedef struct NativeEntityIDRuntime NativeEntityIDRuntime;
NativeEntityIDRuntime *NativeEntityIDRuntime_process(void);
NativeEntityIDRuntime *NativeEntityIDRuntime_new(int32_t initial);
bool NativeEntityIDRuntime_free(NativeEntityIDRuntime *);
bool NativeEntityIDRuntime_next(NativeEntityIDRuntime *,int32_t *out);
#endif
