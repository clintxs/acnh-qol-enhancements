#include "program/core/resource_requirements.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace qol::core;
int main() {
    ResourceRequirements<1> requirements;
    std::string path = "Layout/Owned.Nin_NX_NVN";
    assert(requirements.Require(path, 100));
    path[0] = 'X';
    assert(requirements.MinimumFor("Layout/Owned.Nin_NX_NVN") == 100);
    assert(requirements.MinimumFor("Layout/Owned.Nin_NX_NVN.zs") == 0);
    assert(requirements.MinimumFor("layout/Owned.Nin_NX_NVN") == 0);
    assert(requirements.Require("Layout/Owned.Nin_NX_NVN", 50).status == RegistrationStatus::Identical);
    assert(requirements.Require("Layout/Owned.Nin_NX_NVN", 200));
    assert(requirements.MinimumFor("Layout/Owned.Nin_NX_NVN") == 200);
    assert(requirements.Require("other", 1).status == RegistrationStatus::Capacity);
    assert(requirements.Require("", 1).status == RegistrationStatus::Invalid);
    assert(requirements.Require(std::string(96,'x'), 1).status == RegistrationStatus::Invalid);
    assert(requirements.Require(std::string_view("a\0b",3), 1).status == RegistrationStatus::Invalid);
    assert(requirements.Require("valid", 0).status == RegistrationStatus::Invalid);
    assert(requirements.MinimumFor("Layout/Owned.Nin_NX_NVN") == 200);
    std::cout << "PASS: bounded owned exact resource paths, monotonic floors, invalid declarations and full registry\n";
}
