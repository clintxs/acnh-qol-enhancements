#include "program/core/settings_schema.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <string>

namespace schema = qol::core::settings;
struct Preferences { bool flag = true; };
struct Settings { Preferences nested{}; unsigned count = 7; };
inline bool ParseCount(std::string_view value, unsigned &output) {
    return schema::ParseUnsigned(value, output,
        {0, 100, schema::Overflow::Reject, schema::LeadingZeros::Allow});
}
constexpr std::array fields{
    schema::Boolean<Settings, &Settings::nested, &Preferences::flag>("flag"),
    schema::Number<Settings, &Settings::count, ParseCount>("count")
};
static_assert(schema::ValidSchema(fields));
constexpr std::array duplicateNames{fields[0], fields[0]};
static_assert(!schema::ValidSchema(duplicateNames));
constexpr std::array emptyName{schema::Boolean<Settings, &Settings::nested, &Preferences::flag>("")};
static_assert(!schema::ValidSchema(emptyName));
constexpr std::array missingParser{schema::Field<Settings>{"broken", nullptr, nullptr}};
static_assert(!schema::ValidSchema(missingParser));

int main() {
    Settings output;
    assert(schema::Apply("\xef\xbb\xbf # comment\r\n; comment\nflag=false\ncount=002", output, fields) == 0);
    assert(!output.nested.flag && output.count == 2);
    assert(schema::Apply("flag=true\ncount=bad", output, fields) == 1);
    assert(output.nested.flag && output.count == 2);
    for (auto entries : {"count=3\ncount=4", "count=bad\ncount=4", "count=3\ncount=bad",
                         "count=bad\ncount=bad", "count=3\ncount=4\ncount=5"}) {
        Settings prior;
        const unsigned warnings = std::string_view(entries).starts_with("count=bad") ? 2 :
                                  std::string_view(entries).ends_with("count=5") ? 2 : 1;
        assert(schema::Apply(entries, prior, fields) == warnings);
        assert(prior.count == 7);
    }
    assert(schema::Apply("flag=false\nflag=true\ncount=9", output, fields) == 1);
    assert(output.nested.flag && output.count == 9);
    assert(schema::Apply("unknown=true\nunknown=false\nFLAG=true\nflag\n=1\ncount=1=2", output, fields) == 6);
    assert(output.count == 9);
    assert(schema::Apply("enabled=anything\nversion=anything", output, fields) == 0);
    assert(schema::Apply("enabled=true\nversion=1.0.0", output, fields, schema::Metadata::None) == 2);
    for (auto input : {std::string("count=1\n\xff"), std::string("count=1\n") + std::string(4096, 'x'),
                       std::string("flag=false\n\0", 12)}) {
        assert(schema::Apply(input, output, fields) == 1);
        assert(output.nested.flag && output.count == 9);
    }
    std::string exactLimit = "count=3\n#";
    exactLimit.resize(qol::core::MaxConfigBytes, 'x');
    assert(schema::Apply(exactLimit, output, fields) == 0 && output.count == 3);

    const auto maximum = std::numeric_limits<unsigned>::max();
    unsigned number = 11;
    const schema::NumericPolicy fullRange{0, maximum, schema::Overflow::Reject, schema::LeadingZeros::Allow};
    assert(schema::ParseUnsigned(std::to_string(maximum), number, fullRange) && number == maximum);
    number = 11;
    assert(!schema::ParseUnsigned(std::to_string(maximum) + "0", number, fullRange) && number == 11);
    const schema::NumericPolicy clamp{0, 30, schema::Overflow::Clamp, schema::LeadingZeros::Reject};
    assert(schema::ParseUnsigned(std::string(4000, '9'), number, clamp) && number == 30);
    number = 11;
    assert(!schema::ParseUnsigned(std::string(3999, '9') + "x", number, clamp) && number == 11);
    assert(!schema::ParseUnsigned("00", number, clamp) && number == 11);
    const schema::NumericPolicy domain{1, 30, schema::Overflow::Reject, schema::LeadingZeros::Reject};
    for (auto invalid : {"0", "01", "31", "42949672960", "+1", "1.0"})
        assert(!schema::ParseUnsigned(invalid, number, domain) && number == 11);
    const schema::NumericPolicy smallMaximum{0, 3, schema::Overflow::Clamp, schema::LeadingZeros::Allow};
    assert(schema::ParseUnsigned("9", number, smallMaximum) && number == 3);
    std::cout << "PASS: declarative schemas, nested destinations, shared overlay inheritance, duplicate restoration, metadata modes, atomic rejection and numeric policies\n";
}
