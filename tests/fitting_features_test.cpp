#include "program/fitting/features/fitting_features.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
using namespace able_bag;
namespace {
const Item red{117,{0,0},7}, blue{118,{0,0},14};
unsigned invalidReports{};
void ReportInvalid() { ++invalidReports; }
template<class Base> struct Source : Base {
    std::vector<Item> outfit, send, wear;
    std::vector<std::string> events;
    game::Space space{100,10};
    bool deliveryReady = true, stockReady = true, spaceReady = true;
    int quote = 100;
    unsigned ReadOutfit(Item *out) override {
        events.push_back("outfit"); std::copy(outfit.begin(),outfit.end(),out); return outfit.size();
    }
    bool ReadDeliveryCounts(unsigned &a,unsigned &b) override {
        events.push_back("counts"); a=send.size();b=wear.size();return deliveryReady;
    }
    bool ReadDeliveries(game::DeliveryAlternatives &out) override {
        events.push_back("collect"); if (!deliveryReady) return false;
        out.sendCount=send.size();out.wearCount=wear.size();
        std::copy(send.begin(),send.end(),out.send);std::copy(wear.begin(),wear.end(),out.wear);
        return true;
    }
    bool HasStock(const Item &) override { events.push_back("stock");return stockReady; }
    int Quote(const Item &) override { events.push_back("quote");return quote; }
    bool ReadSpace(game::Space &out) override { events.push_back("space");out=space;return spaceReady; }
};
struct Selection : Source<game::FittingSelection> {
    Item item=red;bool selected=true;unsigned errors{},feedback{},consumed{};
    bool SelectedItem(Item &out) override { events.push_back("selected");out=item;return selected; }
    void ShowSpaceError() override { ++errors;events.push_back("error"); }
    void PlayCursorTap() override { ++feedback;events.push_back("feedback"); }
    void ConsumeToggle() override { ++consumed;events.push_back("consume"); }
};
struct Checkout : Source<game::Checkout> {
    Bag *bag{};PurchasePlan *plan{};int total=350;unsigned funds=1000,nativeCalls{},returned{},answerCalls{};
    unsigned lastAnswer=99,lastWear=99;bool idle=true,empty=true,reenter=false,affordable=false;
    game::Question question=game::Question::WearOrSend;
    int TotalPrice() override { events.push_back("price");return total; }
    unsigned AvailableBells() override { events.push_back("funds");return funds; }
    void StoreTotal(int value) override { assert(value==total);events.push_back("store"); }
    void OpenConfirmation(int value,unsigned money,bool canAfford) override {
        assert(value==total && money==funds);affordable=canAfford;events.push_back("confirm");
    }
    game::Question CurrentQuestion() override { events.push_back("question");return question; }
    bool IdleCommand() override { return idle; }
    bool OutfitSlotsEmpty() override { return empty; }
    void SelectSendQuestion() override { question=game::Question::WearOrSend;events.push_back("send-question"); }
    unsigned FundsError() override { events.push_back("funds-error");return 101; }
    unsigned SpaceError() override { events.push_back("space-error");return 102; }
    unsigned ReturnToBrowsing() override { events.push_back("browse");return 0; }
    unsigned NativeAnswer(unsigned answer) override { ++answerCalls;lastAnswer=answer;events.push_back("answer");return 200+answer; }
    void NativeCheckout(unsigned wearChoice) override {
        ++nativeCalls;lastWear=wearChoice;events.push_back("native");
        if (bag->count) {
            assert(plan->active && !plan->done);
            if (reenter) CommitBagCheckout(*bag,*plan,*this,wearChoice^1);
        }
    }
    void CheckoutReturned() override {
        assert(plan->done && !plan->active && !bag->count);++returned;events.push_back("returned");
    }
};
struct Choice : game::WearChoice {
    game::ChoiceFocus focus=game::ChoiceFocus::Wear;
    std::vector<std::string> events;
    game::ChoiceFocus CurrentFocus() override { events.push_back("focus?");return focus; }
    void ConsumeBack() override { events.push_back("consume"); }
    void FocusSend() override { events.push_back("send");focus=game::ChoiceFocus::Send; }
};
struct Footer : game::Footer {
    int count=3,capacity=5;bool ready=true,duplicate=false;unsigned appended{};
    int Count() const override { return count; }
    int Capacity() const override { return capacity; }
    bool Ready() const override { return ready; }
    bool Contains(const char *key) const override { assert(std::string(key)==BagHintKey);return duplicate; }
    void AppendYHint(const char *key) override { assert(std::string(key)==BagHintKey);++appended; }
};
void ToggleCases() {
    Bag bag;Selection input;BadgeMotions motions{};unsigned nextMotion{};
    auto result=ToggleBagSelection(bag,motions,nextMotion,input);
    assert(result.handled && result.result==ToggleResult::Added && motions[0].added && bag.count==1);
    assert((input.events==std::vector<std::string>{"selected","quote","outfit","counts","space","feedback","consume"}));
    input.events.clear();input.spaceReady=false;
    result=ToggleBagSelection(bag,motions,nextMotion,input);
    assert(result.result==ToggleResult::Removed && bag.count==0 && !motions[0].added);
    assert((input.events==std::vector<std::string>{"selected","feedback","consume"}));
    input.spaceReady=true;input.space={0,0};input.send={blue};
    result=ToggleBagSelection(bag,motions,nextMotion,input);
    assert(result.result==ToggleResult::Full && !bag.count && input.errors==1);
    input.events.clear();input.selected=false;const auto consumed=input.consumed;
    assert(!ToggleBagSelection(bag,motions,nextMotion,input).handled && input.consumed==consumed);
    for (const auto flags : {std::array<bool,5>{false,true,false,false,true},
                            {true,false,false,false,true},{true,true,true,false,true},
                            {true,true,false,true,true},{true,true,false,false,false}})
        assert(!BagInputEligible(flags[0],flags[1],flags[2],flags[3],flags[4]));
    assert(BagInputEligible(true,true,false,false,true));
}
void CheckoutCases() {
    Bag bag;bag.Toggle(red,1,true);bag.Toggle(blue,1,true);
    PurchasePlan plan;Checkout checkout;checkout.bag=&bag;checkout.plan=&plan;
    checkout.outfit={red};checkout.send={red};checkout.wear={Item{120,{0,0},77}};
    assert(PrepareBagPurchase(bag,plan,checkout)==Preflight::Ready);
    assert(plan.sendCount==2 && plan.wearCount==2 && plan.send[0]==red && plan.send[1]==blue);
    assert(plan.wear[1]==blue && plan.extraPrice==100 && bag.entries[0].price==100);
    assert(PriceWithBag(250,bag,checkout.outfit.data(),checkout.outfit.size())==350);
    assert(GrossPriceWithBag(250,plan,true)==250);
    plan.active=true;assert(GrossPriceWithBag(250,plan,true)==350);
    assert(GrossPriceWithBag(250,plan,false)==250);plan.active=false;
    assert((checkout.events==std::vector<std::string>{"collect","outfit","stock","quote","stock","quote","space"}));
    checkout.events.clear();checkout.send={blue};
    assert(PrepareBagPurchase(bag,plan,checkout,ReportInvalid)==Preflight::Invalid);
    assert(invalidReports==1);
    plan.done=true;assert(PrepareBagPurchase(bag,plan,checkout,ReportInvalid)==Preflight::Invalid);
    assert(invalidReports==2 && plan.done);plan={};
    assert((checkout.events==std::vector<std::string>{"collect","outfit"}));
    checkout.send={red};checkout.stockReady=false;checkout.funds=0;
    assert(AnswerBagConfirmation(bag,plan,checkout,0)==101); // Funds error retains precedence.
    assert(checkout.events.back()=="funds-error" && !checkout.nativeCalls);
    checkout.funds=1000;
    assert(AnswerBagConfirmation(bag,plan,checkout,0)==0 && checkout.events.back()=="browse");
    checkout.stockReady=true;checkout.space={0,0};
    assert(AnswerBagConfirmation(bag,plan,checkout,0)==102 && checkout.events.back()=="space-error");
    checkout.space={100,10};checkout.question=game::Question::Purchase;
    assert(AnswerBagConfirmation(bag,plan,checkout,1)==200 && checkout.lastAnswer==0);
    assert(!checkout.nativeCalls); // Confirmation only queues native send; never pays.
    checkout.question=game::Question::Purchase;checkout.empty=false;
    assert(AnswerBagConfirmation(bag,plan,checkout,1)==201 && checkout.lastAnswer==1);
    checkout.deliveryReady=false;
    assert(OpenBagConfirmation(bag,plan,checkout,true) && checkout.affordable);
    assert(checkout.events.back()=="confirm"); // Final answer/commit still revalidate.
    checkout.deliveryReady=true;checkout.events.clear();checkout.reenter=true;
    CommitBagCheckout(bag,plan,checkout,1);
    assert(checkout.nativeCalls==1 && checkout.returned==1 && checkout.lastWear==1 && !bag.count);
    assert(plan.done && !plan.active && checkout.events.back()=="returned");
    const auto events=checkout.events;
    CommitBagCheckout(bag,plan,checkout,0);assert(checkout.events==events);
    plan={};checkout.events.clear();
    CommitBagCheckout(bag,plan,checkout,0);assert(checkout.nativeCalls==2 && !plan.done);
    assert((checkout.events==std::vector<std::string>{"native"}));
    checkout.events.clear();assert(!OpenBagConfirmation(bag,plan,checkout,true));assert(checkout.events.empty());
    assert(AnswerBagConfirmation(bag,plan,checkout,1)==201);
    assert((checkout.events==std::vector<std::string>{"answer"}));
    for (auto reason : {0,1,2,3}) {
        bag.Toggle(red,1,true);plan={};checkout.events.clear();checkout.total=350;checkout.funds=1000;
        checkout.stockReady=reason!=0;checkout.spaceReady=true;checkout.space={100,10};
        if (reason==1) checkout.space={0,0};
        if (reason==2) checkout.funds=349;
        if (reason==3) checkout.total=-1;
        const auto calls=checkout.nativeCalls;
        CommitBagCheckout(bag,plan,checkout,0);
        assert(checkout.nativeCalls==calls && bag.count==1 && !plan.done && !plan.active);
        bag.Clear();
    }
}
void ChoiceAndVisualCases() {
    Choice choice;
    assert(HandleBackShortcut(true,true,false,true,choice)==BackAction::FocusSend);
    assert((choice.events==std::vector<std::string>{"focus?","consume","send"}));
    choice.events.clear();assert(HandleBackShortcut(true,true,false,true,choice)==BackAction::ConfirmSend);
    assert((choice.events==std::vector<std::string>{"focus?","consume"}));
    choice.events.clear();assert(HandleBackShortcut(false,true,false,true,choice)==BackAction::Ignore);
    assert(choice.events.empty());
    choice.focus=game::ChoiceFocus::Unavailable;
    assert(HandleBackShortcut(true,true,false,true,choice)==BackAction::Ignore);
    assert(choice.events==std::vector<std::string>{"focus?"});
    Footer footer;AppendBagHint(footer);assert(footer.appended==1);
    for (auto reason : {0,1,2,3}) {
        Footer rejected;if (reason==0) rejected.count=4;if (reason==1) rejected.capacity=4;
        if (reason==2) rejected.ready=false;if (reason==3) rejected.duplicate=true;
        AppendBagHint(rejected);assert(!rejected.appended);
    }
    Bag bag;BadgeMotions motions{};unsigned next{};
    assert(!BagBadgeVisual(bag,motions,&red).shown);
    bag.Toggle(red,1,true);StartBagBadgeMotion(motions,next,red,true);
    auto visual=BagBadgeVisual(bag,motions,&red);
    assert(visual.shown && visual.scale==.5f && visual.alpha==0);
    for (unsigned i=0;i<7;++i) AdvanceBagBadgeMotions(motions);
    assert(std::fabs(BagBadgeVisual(bag,motions,&red).scale-1)<.00001);
    bag.Toggle(red,0,false);StartBagBadgeMotion(motions,next,red,false);
    assert(BagBadgeVisual(bag,motions,&red).shown);
    AdvanceBagBadgeMotions(motions);assert(!BagBadgeVisual(bag,motions,&red).shown);
    AdvanceBagBadgeMotions(motions);assert(!HasBagBadgeMotion(motions));
    assert(!BagBadgeVisual(bag,motions,nullptr).shown);
}
}
int main() {
    ToggleCases();CheckoutCases();ChoiceAndVisualCases();
    std::cout << "PASS Fitting Room feature policy: native observation/order, exact items, plan revalidation, bag-only confirmation, repeat guard, lazy B choice, badge reset and footer limits\n";
}
