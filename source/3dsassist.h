#ifndef _3DSASSIST_H_
#define _3DSASSIST_H_

#include <3ds.h>

enum class Assist3dsMode {
    Inactive,
    Host,
    Controller,
};

enum class Assist3dsControllerConnection {
    Connected,
    HostEndedSession,
    ConnectionLost,
};

bool assist3dsStartHost();
bool assist3dsJoinHost();
void assist3dsStop();

void assist3dsPollHost();
u32 assist3dsGetRemoteKeys();
bool assist3dsSendControllerKeys(u32 keys);
Assist3dsControllerConnection assist3dsPollControllerConnection();

Assist3dsMode assist3dsGetMode();
Result assist3dsGetLastResult();
const char* assist3dsGetStatusText();

#endif
