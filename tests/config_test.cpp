#include "program/core/config.hpp"
#include "program/core/release.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace qol::core;
int main() {
    Config config{};
    for (const auto *text : {"enabled = true\nversion = 0.1.0\n", "\xef\xbb\xbf# comment\r\nversion=0.1.0\r\nenabled=true\r\n", ";comment\nenabled=true\nversion=0.1.0"}) {
        assert(ParseConfig(text, config) && config.enabled && config.version == CoreVersion);
    }
    assert(ParseConfig("enabled=false\nversion=0.1.0", config) && !config.enabled);
    for (const auto *text : {"", "enabled=true", "version=0.1.0", "enabled=yes\nversion=0.1.0", "enabled=true\nversion=1.0", "enabled=true\nversion=0.1.0-test", "enabled=true\nversion=0.1.0\nversion=1.1.0", "enabled=true\nenabled=false\nversion=0.1.0", "enabled=true\nversion=0.1.0\nunknown=1", "enabled=true\nversion=65536.0.0", "enabled=true\nversion=00.1.0", "enabled=true\nversion=-0.1.0", "enabled=true\nversion=1..0", "enabled=true\nversion=0.1.0.0", "enabled=true\nversion=999999999999999999999999.0.0"}) {
        const Config before = config;
        assert(!ParseConfig(text, config));
        assert(config.enabled == before.enabled && config.version == before.version);
    }
    assert(!ParseConfig(std::string("enabled=true\nversion=0.1.0\0", 27), config));
    assert(!ParseConfig(std::string(MaxConfigBytes + 1, ' '), config));
    Version version{};
    assert(ParseVersion("65535.65535.65535", version));
    assert((version == Version{65535,65535,65535}));
    for (unsigned major = 0; major < 4; ++major) {
        for (unsigned minor = 0; minor < 12; ++minor) {
            for (unsigned patch = 0; patch < 4; ++patch) {
                Version installed{static_cast<std::uint16_t>(major),static_cast<std::uint16_t>(minor),static_cast<std::uint16_t>(patch)};
                const auto result = CheckCompatibility(installed, {0,1,0}, {0,9,0});
                if (installed < Version{0,1,0}) assert(result == Compatibility::TooOld);
                else if (installed > Version{0,9,0}) assert(result == Compatibility::NewerThanCore);
                else assert(result == Compatibility::Supported);
            }
        }
    }
    assert(CheckCompatibility({0,10,0},{0,1,0},{0,9,0}) == Compatibility::NewerThanCore);
    Version ninth{}, tenth{};
    assert(ParseVersion("0.9.0", ninth) && ParseVersion("0.10.0", tenth));
    assert(tenth > ninth && tenth.major == 0);
    Version release{};
    assert(ParseVersion(ReleaseVersion, release) && release == CoreVersion);
    std::cout << "PASS: config parsing, rejected records, 192 version combinations and numeric ordering\n";
}
