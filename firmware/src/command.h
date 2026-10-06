/************************************************************************
    command.h

    Host command interpreter (USB serial); see firmware/PROTOCOL.md

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
************************************************************************/

#ifndef COMMAND_H_
#define COMMAND_H_

#include <stdint.h>

// Record the TMS5220 clock frequency that main() started (for INFO)
void commandSetClock(uint32_t hz);

// Read and execute host commands forever
void commandLoop(void);

#endif /* COMMAND_H_ */
