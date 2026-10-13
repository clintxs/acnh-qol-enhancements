#pragma once
#include "lib.hpp"
#include "executable_view.hpp"
#include "program/core/feature_activation.hpp"

namespace qol::native {
inline ExecutableView MainExecutable() {
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    return {text.m_Start, text.m_Size};
}
inline bool MainDataContains(std::uintptr_t moduleOffset, std::size_t bytes) {
    const auto &main = exl::util::GetMainModuleInfo();
    if (moduleOffset > std::numeric_limits<std::uintptr_t>::max() - main.m_Text.m_Start)
        return false;
    const auto address = main.m_Text.m_Start + moduleOffset;
    return address >= main.m_Data.m_Start &&
           ContainsRange(main.m_Data.m_Start, main.m_Data.m_Size,
                         address - main.m_Data.m_Start, bytes);
}
inline void ReportMismatch(const CodeResult &result) {
    core::ReportActivationFailure(core::FeatureState::Unsupported, "native instruction mismatch/range");
    Logging.Error("[QoL Enhancements] native binding=%s main+0x%lx: %s\n",
                  result.binding, result.offset,
                  result.status == CodeStatus::OutOfRange ? "outside executable text" : "instruction mismatch");
    if (result.actualReadable) {
        for (std::size_t i = 0; i < result.wordCount; ++i)
            Logging.Error("[QoL Enhancements]   word[%u] expected=%08x actual=%08x\n",
                          static_cast<unsigned>(i), result.expected[i], result.actual[i]);
    } else {
        for (std::size_t i = 0; i < result.wordCount; ++i)
            Logging.Error("[QoL Enhancements]   word[%u] expected=%08x actual=unreadable\n",
                          static_cast<unsigned>(i), result.expected[i]);
    }
}
} // namespace qol::native
