#include "program/crafting/settings.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace qol::crafting;

int main() {
    Settings settings{};
    assert(settings.diyAppPreview && settings.workbenchPreviewTilt && settings.craftingQuantityWrap);
    assert(settings.craftingSpeed == 6);
    assert(ApplySettings("enabled=true\nversion=0.3.49\ndiy_app_preview=false\n"
                         "workbench_preview_tilt=false\ncrafting_quantity_wrap=false\ncrafting_speed=2", settings) == 0);
    assert(!settings.diyAppPreview && !settings.workbenchPreviewTilt && !settings.craftingQuantityWrap);
    assert(settings.craftingSpeed == 2);
    assert(ApplySettings("\xef\xbb\xbf# overlay\r\n;comment\r\ndiy_app_preview=true\r\ncrafting_speed=1", settings) == 0);
    assert(settings.diyAppPreview && !settings.workbenchPreviewTilt && !settings.craftingQuantityWrap);
    assert(settings.craftingSpeed == 1);
    for (auto key : {"diy_app_preview", "workbench_preview_tilt", "crafting_quantity_wrap"}) {
        for (auto value : {"true", "false"}) {
            const std::string line = std::string(key) + "=" + value;
            Settings parsed{};
            assert(ApplySettings(line, parsed) == 0);
            const bool actual = std::string(key) == "diy_app_preview" ? parsed.diyAppPreview :
                                std::string(key) == "workbench_preview_tilt" ? parsed.workbenchPreviewTilt : parsed.craftingQuantityWrap;
            assert(actual == (std::string(value) == "true"));
            assert(ApplySettings(line + "\n" + line, parsed) == 1);
        }
        Settings parsed{};
        assert(ApplySettings(std::string(key) + "=false\n" + key + "=true", parsed) == 1);
        assert(parsed.diyAppPreview && parsed.workbenchPreviewTilt && parsed.craftingQuantityWrap);
    }
    for (unsigned speed = 0; speed <= 30; ++speed) {
        Settings parsed{};
        assert(!ApplySettings("crafting_speed=" + std::to_string(speed), parsed));
        assert(parsed.craftingSpeed == speed);
    }
    for (const auto &clamped : {std::string("31"), std::string("100"), std::string("4294967295"),
                               std::string("999999999999999999"), std::string(4000, '9')}) {
        Settings parsed = settings;
        assert(!ApplySettings("crafting_speed=" + clamped, parsed));
        assert(parsed.craftingSpeed == 30);
        assert(ApplySettings("crafting_speed=" + clamped + "\ncrafting_speed=1", parsed) == 1);
        assert(parsed.craftingSpeed == 30);
        parsed = settings;
        assert(ApplySettings("crafting_speed=" + clamped + "\ncrafting_speed=1", parsed) == 1);
        assert(parsed.craftingSpeed == settings.craftingSpeed);
    }
    for (auto invalid : {"", "00", "-1", "1.5", "03", "NaN", "inf", "999999999999999999x", "6x", "+6"}) {
        Settings parsed = settings;
        assert(ApplySettings(std::string("crafting_speed=") + invalid, parsed) == 1);
        assert(parsed.craftingSpeed == settings.craftingSpeed);
    }
    assert(ApplySettings("crafting_speed=2\ncrafting_speed=3\ndiy_app_preview=false", settings) == 1);
    assert(settings.craftingSpeed == 1 && !settings.diyAppPreview);
    assert(ApplySettings("diy_app_preview=YES\nworkbench_preview_tilt=0\ncrafting_quantity_wrap=1", settings) == 3);
    assert(!settings.diyAppPreview && !settings.workbenchPreviewTilt && !settings.craftingQuantityWrap);
    assert(ApplySettings("unknown=true\nmalformed", settings) == 2);
    for (auto invalid : {std::string("crafting_speed=3\n\xff"), std::string(4097, 'x'), std::string("crafting_speed=3\0", 17)}) {
        Settings parsed = settings;
        assert(ApplySettings(invalid, parsed) == 1);
        assert(parsed.craftingSpeed == settings.craftingSpeed && parsed.diyAppPreview == settings.diyAppPreview);
    }
    std::cout << "PASS: Crafting defaults, independent feature settings, 0..30 speed range (zero preserves native speed), overflow-safe maximum clamping, layered inheritance, duplicates, invalid values and atomic encoding/size errors\n";
}
