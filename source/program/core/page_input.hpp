#pragma once

namespace qol::core {
// Return true when this page owns input and native dispatch must be suppressed.
// Registration occurs once during module activation, before pages receive input.
using PageInputHandler = bool (*)(void *page, void *snapshot, unsigned blocked);
bool RegisterPageInput(PageInputHandler handler);
using PageInputCallback = void (*)(void *page, void *snapshot, unsigned blocked);
// Observers bracket native dispatch. Suppression sends only a blocked before
// callback, allowing observers to cancel gestures. Capture eligibility before
// dispatch: native processing blanket-consumes input rows.
bool RegisterPageInputObserver(PageInputCallback before, PageInputCallback after);
}
