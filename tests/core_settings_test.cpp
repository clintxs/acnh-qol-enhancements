#include "program/core/settings.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace qol::core;

int main() {
    Settings settings{};
    assert(settings.amiiboTimeoutFrames == 9000);
    assert(settings.logging.versions && settings.logging.settings && !settings.logging.debug);
    for (unsigned flags = 0; flags < 8; ++flags) {
        Settings parsed{};
        const bool versions = flags & 1, summaries = flags & 2, debug = flags & 4;
        const auto config = std::string("log_versions=") + (versions ? "true" : "false") +
            "\nlog_settings=" + (summaries ? "true" : "false") + "\nlog_debug=" + (debug ? "true" : "false");
        assert(!ApplyCoreSettings(config, parsed));
        assert(parsed.logging.versions == versions && parsed.logging.settings == summaries && parsed.logging.debug == debug);
        assert(qol::logging::Enabled(parsed.logging, qol::logging::Category::Version) == versions);
        assert(qol::logging::Enabled(parsed.logging, qol::logging::Category::Settings) == summaries);
        assert(qol::logging::Enabled(parsed.logging, qol::logging::Category::Debug) == debug);
        assert(qol::logging::Enabled(parsed.logging, qol::logging::Category::Warning));
        assert(qol::logging::Enabled(parsed.logging, qol::logging::Category::Error));
        assert(!ApplyCoreSettings("# inherit", parsed));
        for (const auto *key : {"log_versions", "log_settings", "log_debug"}) {
            const auto prior = parsed.logging;
            assert(ApplyCoreSettings(std::string(key) + "=false\n" + key + "=true", parsed) == 1);
            assert(parsed.logging.versions == prior.versions && parsed.logging.settings == prior.settings && parsed.logging.debug == prior.debug);
            for (const auto *bad : {"", "TRUE", "0", "yes", "true # comment"}) {
                assert(ApplyCoreSettings(std::string(key) + "=" + bad, parsed) == 1);
                assert(parsed.logging.versions == prior.versions && parsed.logging.settings == prior.settings && parsed.logging.debug == prior.debug);
            }
        }
        assert(!ApplyCoreSettings("log_versions=true\nlog_settings=true\nlog_debug=false", parsed));
        assert(parsed.logging.versions && parsed.logging.settings && !parsed.logging.debug);
        assert(ApplyCoreSettings("log_debug=true\n\xff", parsed) == 1 && !parsed.logging.debug);
        assert(ApplyCoreSettings("log_debug=true\n" + std::string(MaxConfigBytes, ' '), parsed) == 1 && !parsed.logging.debug);
    }
    for (unsigned frames = 0; frames <= MaxAmiiboTimeoutFrames; ++frames) {
        assert(!ApplyCoreSettings("amiibo_timeout_frames=" + std::to_string(frames), settings));
        assert(settings.amiiboTimeoutFrames == frames);
    }
    for (const auto *value : {"", "-1", "+1", "1.5", "9,000", "true", "65536", "999999999999999999999999999999", "1x"}) {
        assert(ApplyCoreSettings(std::string("amiibo_timeout_frames=") + value, settings) == 1);
        assert(settings.amiiboTimeoutFrames == 65535);
    }
    assert(!ApplyCoreSettings("\xef\xbb\xbf# defaults\r\n; comment\r\n amiibo_timeout_frames = 9000\r\n", settings));
    assert(settings.amiiboTimeoutFrames == 9000);
    assert(!ApplyCoreSettings("# omitted\n", settings) && settings.amiiboTimeoutFrames == 9000);
    assert(!ApplyCoreSettings("amiibo_timeout_frames=0", settings) && !settings.amiiboTimeoutFrames);
    assert(ApplyCoreSettings("amiibo_timeout_frames=450\namiibo_timeout_frames=9000", settings) == 1);
    assert(!settings.amiiboTimeoutFrames);
    assert(ApplyCoreSettings("amiibo_timeout_frames=450\namiibo_timeout_frames=bad", settings) == 1);
    assert(!settings.amiiboTimeoutFrames);
    assert(ApplyCoreSettings("enabled=false\nversion=0.3.19\nunknown=1\nbroken", settings) == 4);
    assert(!settings.amiiboTimeoutFrames);
    for (const auto &input : {std::string(MaxConfigBytes + 1, ' '),
                              std::string("amiibo_timeout_frames=1\n\xff"),
                              std::string("amiibo_timeout_frames=1\0", 24)}) {
        assert(ApplyCoreSettings(input, settings) == 1 && !settings.amiiboTimeoutFrames);
    }
    std::cout << "PASS: all 65536 Core timeout values, layered defaults/zero/inheritance, overflow, duplicates, BOM/CRLF/comments, atomic encoding/size failures\n";
}
