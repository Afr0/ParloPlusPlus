#pragma once

/*IDs used by internal packets.
These IDs are reserved, I.E
should not be used by a protocol.*/
enum ParloIDs
{
    /*The ID for a Heartbeat packet.*/
    Heartbeat = 0xFD,

    /*The ID for a Goodbye packet sent by a server.*/
    SGoodbye = 0xFE,

    /*The ID for a Goodbye packet sent by a client.*/
    CGoodbye = 0xFF //Should be sufficiently large, no protocol should need this many packets.
};