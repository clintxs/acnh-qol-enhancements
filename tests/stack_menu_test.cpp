#include "program/pockets/features/stack_menu.hpp"
#include <cassert>
#include <vector>
#include <iostream>

int main() {
    using namespace qol_pockets::features;
    int cases=0;
    for(int length=1;length<=7;++length) for(int selected=0;selected<length;++selected)
    for(int count: {1,5,6,10,11,30,99}) for(bool free: {false,true}) {
        ActionList list{};
        list.count=length;
        for(int i=0;i<length;++i)list.items[i]=static_cast<Action>(i);
        list.items[selected]=Action::GrabOne;
        std::vector<Action> expected(list.items,list.items+length);
        if(free && count>5 && length<7) {
            expected.insert(expected.begin()+selected+1,Action::GrabFive);
            if(count>10 && length<=5)expected.insert(expected.begin()+selected+2,Action::GrabTen);
        }
        AddStackActions(list,count,free);
        assert(list.count==static_cast<int>(expected.size()));
        assert(std::equal(expected.begin(),expected.end(),list.items));
        AddStackActions(list,count,free);
        assert(list.count==static_cast<int>(expected.size()));
        ++cases;
    }
    for(int invalid: {-1,0,8}) {
        ActionList list{};list.count=invalid;
        AddStackActions(list,30,true);assert(list.count==invalid);
    }
    ActionList turnips{{Action::TurnipGrabTen},1};
    AddStackActions(turnips,100,true);assert(turnips.count==1);
    std::cout << "PASS " << cases << " menu cases, idempotence, invalid lists, turnip exclusion\n";
}
