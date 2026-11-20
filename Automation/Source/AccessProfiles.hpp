// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cctype>
#include <cymcp/http.hpp>

inline cymcp::LocalAccessProfile AccessProfile(const cymcp::Json &options)
{
    using P = cymcp::LocalAccessProfile;
    if (!options.contains("access_profile"))
        return P::free;
    const auto &value = options.at("access_profile");
    if (value.is_number_integer())
    {
        const auto index = value.get<int>();
        if (index >= 0 && index <= 3)
            return static_cast<P>(index);
    }
    else if (value.is_string())
    {
        auto name = value.get<std::string>();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (name == "free" || name == "0")
            return P::free;
        if (name == "local_controlled" || name == "local-controlled" || name == "1")
            return P::local_controlled;
        if (name == "token_only" || name == "token-only" || name == "2")
            return P::token_only;
        if (name == "protected_access" || name == "protected" || name == "3")
            return P::protected_access;
    }
    // An unknown explicit selection must never silently become unrestricted.
    throw cymcp::Error("InvalidConfiguration",
                       "Unknown MCP access profile; select free, local_controlled, token_only or protected_access");
}
inline const char *AccessProfileName(cymcp::LocalAccessProfile profile)
{
    switch (profile)
    {
    case cymcp::LocalAccessProfile::free:
        return "free";
    case cymcp::LocalAccessProfile::local_controlled:
        return "local_controlled";
    case cymcp::LocalAccessProfile::token_only:
        return "token_only";
    case cymcp::LocalAccessProfile::protected_access:
        return "protected_access";
    }
    return "invalid";
}
