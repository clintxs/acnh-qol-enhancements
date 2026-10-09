#pragma once

namespace qol::core {
// Return true when this page owns input and native dispatch must be suppressed.
// Registration occurs once during module activation, before pages receive input.
using PageInputHandler = bool (*)(void *page, void *snapshot, unsigned blocked);
bool RegisterPageInput(PageInputHandler handler);
}
