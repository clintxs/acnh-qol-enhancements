#pragma once
namespace qol_pockets::game {
enum class PocketHint { Store, Drop, Use };
class PocketFooter {
    void *owner_;
    void (*append_)(void *, PocketHint);
public:
    // Callback-scoped borrow. Append may be ignored when the native footer is
    // unavailable/full or already contains this hint; it never replaces a row.
    PocketFooter(void *owner, void (*append)(void *, PocketHint)) : owner_(owner), append_(append) {}
    void Append(PocketHint hint) { append_(owner_, hint); }
};
using ExtendPocketFooter = void (*)(PocketFooter &, bool atHome);
}
