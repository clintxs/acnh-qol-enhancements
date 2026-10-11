#include "program/pockets/text/catalog.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc,char** argv) {
    using namespace qol_pockets::text;
    assert(argc==2);
    std::ifstream file(argv[1]);
    assert(file);
    std::string content{std::istreambuf_iterator<char>(file),{}};
    Catalog catalog;
    assert(catalog.Load(content) && catalog.Size()==28);
    assert(catalog.Text("qol_pockets.drop_release",1)==nullptr); // Native caption fallback.
    assert(catalog.Text("qol_pockets.hold_wear",7)==nullptr);
    for (unsigned locale=0;locale<16;++locale) {
        assert(catalog.Text(GrabFiveKey,locale));
        assert(catalog.Text(GrabTenKey,locale));
    }
    assert(std::u16string(catalog.Text(GrabFiveKey,0))==u"5コわける");
    assert(std::u16string(catalog.Text(GrabTenKey,14))==u"分出10个");
    assert(std::u16string(catalog.Text(GrabFiveKey,12))==u"Взять 5");
    assert(std::u16string(catalog.Text(GrabFiveKey,4))==u"Grab 5");
    assert(catalog.Text("unowned.key",1)==nullptr);
    assert(catalog.Load("\xef\xbb\xbf[USen]\r\noptimized_ui.grab_five= Pick 5 \r\n"));
    assert(std::u16string(catalog.Text(GrabFiveKey,999))==u"Pick 5");
    assert(std::u16string(catalog.Text(GrabTenKey,999))==u"Grab 10");
    for (auto invalid: {"", "[Bad]\nx=y", "x=y", "[USen]\nx=y\nx=z", "[USen]\nx=",
                        "[USen]\nx=\xc0\x80", "[USen]\nx=\xed\xa0\x80", "[USen]\nx=\xf4\x90\x80\x80",
                        "[USen]\nx=\xe2\x82", "[USen]\nx=\x0e", "[USen]\nUpper=hello"}) {
        assert(!catalog.Load(invalid) && catalog.Size()==0);
        assert(std::u16string(catalog.Text(GrabFiveKey,1))==u"Grab 5");
    }
    assert(!catalog.Load(std::string("[USen]\nx=a\0b",13)));
    assert(!catalog.Load("[USen]\nx="+std::string(128,'a')));
    assert(!catalog.Load(std::string(MaxFileBytes+1,'#')));
    assert(!catalog.Load("[USen]\n"+std::string(64,'a')+"=x"));
    std::string tooMany="[USen]\n";
    for (int i=0;i<65;++i) tooMany+="key"+std::to_string(i)+"=x\n";
    assert(!catalog.Load(tooMany) && catalog.Size()==0);
    char16_t buffer[8]{u'x',u'x',u'x',u'x',u'x',u'x',u'x',u'x'};
    assert(Decode("\xf0\x9f\x8c\xb3",buffer,8));
    assert(std::u16string(buffer)==u"\U0001f333");
    assert(!CopyCaption(buffer,2,u"Grab 5"));
    assert(std::u16string(buffer)==u"\U0001f333");
    assert(CopyCaption(buffer,8,u"Grab 5") && buffer[7]==u'x');
    assert(!CopyCaption(nullptr,8,u"Grab 5"));
    std::cout << "PASS catalog: 14 locales, fallbacks, malformed input, UTF-8, bounds, atomic rejection\n";
}
