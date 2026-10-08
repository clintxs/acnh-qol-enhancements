#include "program/pockets/features/storage_policy.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace qol_pockets::features;
    int cases=0;
    for (int count: {-1,0,1,2,7,8}) for (int action: {0,1,46,68})
    for (const char* bank: {"CmnSubWin","Other",static_cast<const char*>(nullptr)})
    for (const char* key: {"0202","0123",static_cast<const char*>(nullptr)})
    for (unsigned flags: {0u,1u}) for (unsigned extra: {0u,1u}) {
        const bool expected=count==1 && action==0 && bank && key &&
            std::string_view(bank)=="CmnSubWin" && std::string_view(key)=="0202" && !flags && !extra;
        assert(IsOnlyMoveToPockets(count,action,bank,key,flags,extra)==expected);
        ++cases;
    }
    std::cout << "PASS " << cases << " storage single-action policy cases\n";
}
