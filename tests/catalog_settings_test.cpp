#include "program/catalog/settings.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace catalog_aware_shops;
    Settings value;
    assert(value.artDonationStars);
    assert(ApplySettings("enabled=true\nversion=0.4.29\nart_donation_stars=false\n",value)==0);
    assert(!value.artDonationStars);
    assert(ApplySettings("# inherit\n",value)==0 && !value.artDonationStars);
    assert(ApplySettings("art_donation_stars=true\nart_donation_stars=false\n",value)==1);
    assert(!value.artDonationStars);
    assert(ApplySettings("art_donation_stars=maybe\n",value)==1 && !value.artDonationStars);
    assert(ApplySettings("\xef\xbb\xbf art_donation_stars = true\r\n",value)==0);
    assert(value.artDonationStars);
    assert(ApplySettings("bad_key=true\n",value)==1);
    std::cout << "PASS: Catalog art setting defaults, layering, duplicates and invalid input\n";
}
