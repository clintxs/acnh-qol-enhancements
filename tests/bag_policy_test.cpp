#include "../source/program/fitting/features/bag_policy.hpp"
#include <cassert>
#include <cstdio>
using namespace able_bag;
int main() {
    Bag bag;
    Item red{117,{0,0},0}, blue{118,{0,0},0}, flagged{117,{1,0},0}, extra{117,{0,0},42};
    for (Item item : {red,blue,flagged,extra}) assert(bag.Toggle(item,100,true)==ToggleResult::Added);
    assert(bag.count==4);
    Item outfit[]{red,blue};
    assert(bag.ExtraCount(outfit,2)==2 && bag.ExtraPrice(outfit,2)==200);
    assert(bag.Toggle(red,100,false)==ToggleResult::Removed); // Removal while full.
    assert(bag.Find(blue)>=0 && bag.Find(flagged)>=0 && bag.Find(extra)>=0);
    assert(bag.Toggle(red,100,false)==ToggleResult::Full);
    bag.Clear();
    for(unsigned i=0;i<Bag::Limit;++i) assert(bag.Toggle(Item{std::uint16_t(i),{0,0},0},100,true)==ToggleResult::Added);
    assert(bag.Toggle(Item{300,{0,0},0},100,true)==ToggleResult::Full);
    assert(bag.Toggle(Item{0,{0,0},0},100,false)==ToggleResult::Removed);
    assert(bag.Toggle(Item{300,{0,0},0},100,true)==ToggleResult::Added);
    assert(bag.Toggle(Item{0xffff,{0,0},0},100,true)==ToggleResult::Invalid);
    unsigned cases=0;
    for(unsigned storage=0;storage<15;++storage) for(unsigned pockets=0;pockets<=40;++pockets)
    for(unsigned old=0;old<=10;++old) for(unsigned purchases=0;purchases<=10;++purchases)
    for(unsigned extra=0;extra<5;++extra) {
        assert(Fits(storage,pockets,old,purchases,extra)==
               (old+extra<=storage+pockets && purchases+extra<=storage+pockets));++cases;
    }
    PressEdge edge;
    assert(!edge.Update(true));assert(!edge.Update(true));assert(!edge.Update(false));
    assert(edge.Update(true));assert(!edge.Update(true));edge.Reset();assert(!edge.Update(true));
    std::printf("PASS: %u capacity cases; variants, deduplication, limits, removal and input edges\n",cases);
}
