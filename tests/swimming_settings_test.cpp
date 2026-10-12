#include "program/swimming/settings.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace qol::swimming;
int main() {
    Settings settings{};
    assert(settings.autoSwim && settings.cameraTiltWhileSwimming);
    for (bool prior : {false, true}) {
        settings.autoSwim = prior;
        assert(!ApplySettings("# inherit", settings) && settings.autoSwim == prior);
        assert(ApplySettings("auto_swim=yes", settings) == 1 && settings.autoSwim == prior);
        assert(ApplySettings("auto_swim=false\nauto_swim=true", settings) == 1 && settings.autoSwim == prior);
        assert(ApplySettings("auto_swim=false\n\xff", settings) == 1 && settings.autoSwim == prior);
        assert(!ApplySettings("auto_swim=false", settings) && !settings.autoSwim);
        assert(!ApplySettings("auto_swim=true", settings) && settings.autoSwim);
    }
    assert(!ApplySettings("enabled=false\nversion=0.4.21\nauto_swim=false", settings));
    assert(!settings.autoSwim);
    assert(!ApplySettings("\xef\xbb\xbf ; comment\r\nauto_swim = true\r\n", settings));
    assert(settings.autoSwim);
    assert(ApplySettings(std::string(4097, ' '), settings) == 1 && settings.autoSwim);
    assert(ApplySettings("unknown=true\nbroken", settings) == 2);
    for (bool prior : {false, true}) {
        settings.cameraTiltWhileSwimming = prior;
        assert(!ApplySettings("# inherit", settings) && settings.cameraTiltWhileSwimming == prior);
        assert(ApplySettings("camera_tilt_while_swimming=yes", settings) == 1 && settings.cameraTiltWhileSwimming == prior);
        assert(ApplySettings("camera_tilt_while_swimming=false\ncamera_tilt_while_swimming=true", settings) == 1 && settings.cameraTiltWhileSwimming == prior);
        assert(!ApplySettings("camera_tilt_while_swimming=false", settings) && !settings.cameraTiltWhileSwimming);
        assert(!ApplySettings("camera_tilt_while_swimming=true", settings) && settings.cameraTiltWhileSwimming);
    }
    assert(ApplySettings("auto_swim=false\nauto_swim=true\ncamera_tilt_while_swimming=false", settings) == 1);
    assert(settings.autoSwim && !settings.cameraTiltWhileSwimming);
    assert(ApplySettings("auto_swim=false\ncamera_tilt_while_swimming=true\ncamera_tilt_while_swimming=false", settings) == 1);
    assert(!settings.autoSwim && !settings.cameraTiltWhileSwimming);
    std::cout << "PASS: Swimming default, inheritance, boolean validation and duplicate rollback\n";
}
