#pragma once
#include "FreeRTOS.h"

enum {
    PRIORITY_stdioTask = configMAX_PRIORITIES - 2
};

enum {
	NOTIFICATION_IX_reserved,
    NOTIFICATION_IX_STDIO,
};
