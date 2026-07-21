//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "test_PAL.h"

bool TestUtils::m_debugFlag = false;

bool TestUtils::IsDebuggerAttached()
{
    return false;
}

bool TestUtils::GetSystemProxy(ProxyServerInfo&)
{
    return false;
}

void TestUtils::SetDebugFlag(bool debugFlag)
{
    m_debugFlag = debugFlag;
}

bool TestUtils::GetDebugFlag()
{
    return m_debugFlag;
}