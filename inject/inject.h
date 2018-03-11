#pragma once

#include <base/config.h>
#include <base/win/process.h>

_BASE_EXPORT void     inject_start(base::win::process& p);
_BASE_EXPORT uint16_t inject_wait(base::win::process& p);
