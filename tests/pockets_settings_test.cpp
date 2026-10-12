#include "program/pockets/settings.hpp"
#include <cassert>
#include <string>
#include <iostream>
using namespace qol_pockets;
int main() {
    Settings s{};
    assert(s.bypassMenuOpenContainers && s.bypassMenuClaimRecycleBox && s.bypassMenuWithdrawHomeStorage && s.enableMultiselect);
    const char *keys[] = {"bypass_menu_open_containers", "bypass_menu_claim_recycle_box", "bypass_menu_withdraw_home_storage", "enable_multiselect"};
    bool Settings::*fields[] = {&Settings::bypassMenuOpenContainers, &Settings::bypassMenuClaimRecycleBox, &Settings::bypassMenuWithdrawHomeStorage, &Settings::enableMultiselect};
    for (unsigned mask=0; mask<16; ++mask) {
        std::string input="enabled=true\nversion=0.4.17\n";
        for (unsigned i=0;i<4;++i) input+=std::string(keys[i])+"="+((mask&(1<<i))?"true\n":"false\n");
        qol::core::Config metadata{};
        assert(qol::core::ParseConfig(input, metadata, qol::core::ConfigLayer::Module, true));
        assert(!ApplySettings(input,s));
        for (unsigned i=0;i<4;++i) assert(s.*fields[i]==bool(mask&(1<<i)));
        for (unsigned i=0;i<4;++i) {
            const bool prior=s.*fields[i];
            assert(!ApplySettings("# inherit\n",s));
            assert(ApplySettings(std::string(keys[i])+"=false\n"+keys[i]+"=true",s)==1);
            assert(s.*fields[i]==prior);
            for (auto value : {"", "TRUE", "1", "yes", "false # comment"}) {
                assert(ApplySettings(std::string(keys[i])+"="+value,s)==1);
                assert(s.*fields[i]==prior);
            }
            assert(!ApplySettings(std::string("\xef\xbb\xbf ; comment\r\n")+keys[i]+" = "+(prior?"false":"true")+"\r\n",s));
            for(unsigned j=0;j<4;++j) assert(s.*fields[j]==(j==i?!prior:bool(mask&(1<<j))));
            s.*fields[i]=prior;
        }
    }
    assert(ApplySettings("unknown=true\nbroken",s)==2);
    assert(ApplySettings("bypass_menu_open_containers=false\n\xff",s)==1 && s.bypassMenuOpenContainers);
    assert(ApplySettings(std::string(4097,' '),s)==1);
    std::cout<<"PASS: Pockets defaults, sixteen combinations, inheritance, metadata, invalid/duplicate values\n";
}
