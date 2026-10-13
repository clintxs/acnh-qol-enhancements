#pragma once
namespace qol::core {
enum class RegistrationStatus { Ready, Identical, Invalid, Conflict, Capacity, Unavailable };
struct RegistrationResult {
    RegistrationStatus status;
    constexpr explicit operator bool() const {
        return status == RegistrationStatus::Ready || status == RegistrationStatus::Identical;
    }
};
}
