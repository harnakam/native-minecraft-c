#ifndef C919_NATIVE_PACKET_THREAD_H
#define C919_NATIVE_PACKET_THREAD_H
/* Native completion adapter for PacketThreadUtil and its quick-exit exception.
   A queued packet performs no remaining handler body on the calling thread. */
typedef enum {
    MC_PACKET_THREAD_EXECUTE,
    MC_PACKET_THREAD_QUEUED,
    MC_PACKET_THREAD_FAILED
} MCPacketThreadResult;
#endif
