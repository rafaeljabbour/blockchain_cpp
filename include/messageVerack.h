#ifndef MESSAGEVERACK_H
#define MESSAGEVERACK_H

#include "message.h"

// acknowledgement of a received version message.
inline Message CreateVerackMessage() { return Message(MAGIC_CUSTOM, CMD_VERACK, {}); }

#endif