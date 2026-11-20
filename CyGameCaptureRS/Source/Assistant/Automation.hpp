// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "CyGameCaptureAutomation.h"
#include <string>
namespace cygc
{
struct DeviceContext;
class Automation
{
    DeviceContext &device_;
    void *host_ = nullptr;
    std::string reply_;
    static const char *__cdecl Invoke(void *, const char *, const char *);

  public:
    explicit Automation(DeviceContext &);
    ~Automation();
    void Pump();
    void AllowMutations(bool allow);
    std::string Connection() const;
    std::string Profile() const;
    std::string Address() const;
    bool ControlsOperations() const;
    bool AllowsMutations() const;
};
} // namespace cygc
