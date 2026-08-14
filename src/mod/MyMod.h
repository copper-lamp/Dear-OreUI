#pragma once

#include "runtime/IRuntime.h"

#include "ll/api/mod/NativeMod.h"

#include <memory>

namespace dearoreui {

class DearOreUI {

public:
    static DearOreUI& getInstance();

    DearOreUI() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

private:
    ll::mod::NativeMod&                mSelf;
    std::unique_ptr<runtime::IRuntime> mRuntime;
};

} // namespace dearoreui