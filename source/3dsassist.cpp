#include <cstdlib>
#include <cstring>

#include <3ds.h>

#include "3dsassist.h"

namespace {
    const u32 ASSIST_WLAN_COMM_ID = 0x53445041;
    const u8 ASSIST_NETWORK_ID = 1;
    const u8 ASSIST_DATA_CHANNEL = 1;
    const u8 ASSIST_MAX_NODES = 2;
    const u32 ASSIST_PACKET_MAGIC = 0x41535431;
    const u16 ASSIST_PROTOCOL_VERSION = 1;
    const u64 ASSIST_INPUT_TIMEOUT_MS = 250;
    const size_t ASSIST_SCAN_BUFFER_SIZE = 0x4000;
    const int ASSIST_SCAN_ATTEMPTS = 20;

    const char ASSIST_PASSPHRASE[] = "snes9x-dp-assist-v1";
    const char ASSIST_APPLICATION_DATA[] = "SNES9X-DP ASSIST 1";

    struct AssistInputPacket {
        u32 magic;
        u16 version;
        u16 reserved;
        u32 sequence;
        u32 keys;
    };

    Assist3dsMode mode = Assist3dsMode::Inactive;
    udsBindContext bindContext;
    bool udsInitialized = false;
    bool bindInitialized = false;
    Result lastResult = 0;
    u32 remoteKeys = 0;
    u32 sendSequence = 0;
    u64 lastInputTime = 0;

    bool initializeUds()
    {
        if (udsInitialized)
            return true;

        lastResult = udsInit(0x4000, NULL);
        if (R_FAILED(lastResult))
            return false;

        udsInitialized = true;
        return true;
    }
}

void assist3dsStop()
{
    remoteKeys = 0;
    lastInputTime = 0;
    sendSequence = 0;

    if (mode == Assist3dsMode::Host)
        udsDestroyNetwork();
    else if (mode == Assist3dsMode::Controller)
        udsDisconnectNetwork();

    if (bindInitialized)
    {
        udsUnbind(&bindContext);
        bindInitialized = false;
    }

    if (udsInitialized)
    {
        udsExit();
        udsInitialized = false;
    }

    mode = Assist3dsMode::Inactive;
}

bool assist3dsStartHost()
{
    assist3dsStop();

    if (!initializeUds())
        return false;

    udsNetworkStruct network;
    udsGenerateDefaultNetworkStruct(
        &network,
        ASSIST_WLAN_COMM_ID,
        ASSIST_NETWORK_ID,
        ASSIST_MAX_NODES
    );

    lastResult = udsCreateNetwork(
        &network,
        ASSIST_PASSPHRASE,
        sizeof(ASSIST_PASSPHRASE),
        &bindContext,
        ASSIST_DATA_CHANNEL,
        UDS_DEFAULT_RECVBUFSIZE
    );

    if (R_FAILED(lastResult))
    {
        assist3dsStop();
        return false;
    }

    bindInitialized = true;
    mode = Assist3dsMode::Host;

    lastResult = udsSetApplicationData(
        ASSIST_APPLICATION_DATA,
        sizeof(ASSIST_APPLICATION_DATA)
    );

    if (R_FAILED(lastResult))
    {
        assist3dsStop();
        return false;
    }

    return true;
}

bool assist3dsJoinHost()
{
    assist3dsStop();

    if (!initializeUds())
        return false;

    void* scanBuffer = std::malloc(ASSIST_SCAN_BUFFER_SIZE);
    if (!scanBuffer)
    {
        lastResult = MAKERESULT(RL_FATAL, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY);
        assist3dsStop();
        return false;
    }

    udsNetworkStruct selectedNetwork;
    bool networkFound = false;

    for (int attempt = 0; attempt < ASSIST_SCAN_ATTEMPTS && !networkFound; attempt++)
    {
        std::memset(scanBuffer, 0, ASSIST_SCAN_BUFFER_SIZE);

        udsNetworkScanInfo* networks = NULL;
        size_t totalNetworks = 0;
        lastResult = udsScanBeacons(
            scanBuffer,
            ASSIST_SCAN_BUFFER_SIZE,
            &networks,
            &totalNetworks,
            ASSIST_WLAN_COMM_ID,
            ASSIST_NETWORK_ID,
            NULL,
            false
        );

        if (R_SUCCEEDED(lastResult) && totalNetworks > 0)
        {
            selectedNetwork = networks[0].network;
            networkFound = true;
        }

        std::free(networks);

        if (!networkFound)
        {
            gspWaitForVBlank();
            svcSleepThread(50 * 1000 * 1000LL);
        }
    }

    std::free(scanBuffer);

    if (!networkFound)
    {
        lastResult = MAKERESULT(RL_STATUS, RS_NOTFOUND, RM_APPLICATION, RD_NOT_FOUND);
        assist3dsStop();
        return false;
    }

    lastResult = udsConnectNetwork(
        &selectedNetwork,
        ASSIST_PASSPHRASE,
        sizeof(ASSIST_PASSPHRASE),
        &bindContext,
        UDS_BROADCAST_NETWORKNODEID,
        UDSCONTYPE_Client,
        ASSIST_DATA_CHANNEL,
        UDS_DEFAULT_RECVBUFSIZE
    );

    if (R_FAILED(lastResult))
    {
        assist3dsStop();
        return false;
    }

    bindInitialized = true;
    mode = Assist3dsMode::Controller;
    return true;
}

void assist3dsPollHost()
{
    if (mode != Assist3dsMode::Host)
        return;

    while (true)
    {
        AssistInputPacket packet;
        size_t receivedSize = 0;
        u16 sourceNode = 0;

        lastResult = udsPullPacket(
            &bindContext,
            &packet,
            sizeof(packet),
            &receivedSize,
            &sourceNode
        );

        if (R_FAILED(lastResult) || receivedSize == 0)
            break;

        if (sourceNode != UDS_HOST_NETWORKNODEID &&
            receivedSize == sizeof(packet) &&
            packet.magic == ASSIST_PACKET_MAGIC &&
            packet.version == ASSIST_PROTOCOL_VERSION)
        {
            remoteKeys = packet.keys;
            lastInputTime = osGetTime();
        }
    }

    if (lastInputTime == 0 || osGetTime() - lastInputTime > ASSIST_INPUT_TIMEOUT_MS)
        remoteKeys = 0;
}

u32 assist3dsGetRemoteKeys()
{
    if (mode != Assist3dsMode::Host)
        return 0;

    return remoteKeys;
}

bool assist3dsSendControllerKeys(u32 keys)
{
    if (mode != Assist3dsMode::Controller)
        return false;

    AssistInputPacket packet;
    packet.magic = ASSIST_PACKET_MAGIC;
    packet.version = ASSIST_PROTOCOL_VERSION;
    packet.reserved = 0;
    packet.sequence = sendSequence++;
    packet.keys = keys;

    lastResult = udsSendTo(
        UDS_HOST_NETWORKNODEID,
        ASSIST_DATA_CHANNEL,
        UDS_SENDFLAG_Default,
        &packet,
        sizeof(packet)
    );

    return !UDS_CHECK_SENDTO_FATALERROR(lastResult);
}

Assist3dsMode assist3dsGetMode()
{
    return mode;
}

Result assist3dsGetLastResult()
{
    return lastResult;
}

const char* assist3dsGetStatusText()
{
    switch (mode)
    {
        case Assist3dsMode::Host:
            return "Hosting";
        case Assist3dsMode::Controller:
            return "Connected as controller";
        default:
            return "Off";
    }
}
