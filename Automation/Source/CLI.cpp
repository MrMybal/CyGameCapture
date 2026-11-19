// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#include "CyGameCaptureAutomationClient.hpp"
#include <iostream>
int wmain(int argc, wchar_t **argv)
{
    if (argc < 2)
    {
        wchar_t *args[] = {argv[0], const_cast<wchar_t *>(L"--help")};
        return cygc::AutomationMain(2, args);
    }
    std::wstring first = argv[1];
    if (first == L"--cli" || first == L"--mcp" || first == L"--help" || first == L"--version")
        return cygc::AutomationMain(argc, argv);
    std::vector<wchar_t *> args = {argv[0], const_cast<wchar_t *>(L"--cli")};
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);
    return cygc::AutomationMain(static_cast<int>(args.size()), args.data());
}
