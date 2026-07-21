//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once
#include "ispxinterfaces.h"
#include "interface_helpers.h"


namespace Microsoft {
    namespace CognitiveServices {
        namespace Speech {
            namespace Impl {


                class CSpxTokenReqeustEventArgs :
                    public ISpxSessionEventArgs,
                    public ISpxSessionEventArgsInit
                {
                public:

                    CSpxTokenReqeustEventArgs();

                    SPX_INTERFACE_MAP_BEGIN()
                        SPX_INTERFACE_MAP_ENTRY(ISpxSessionEventArgs)
                        SPX_INTERFACE_MAP_ENTRY(ISpxSessionEventArgsInit)
                    SPX_INTERFACE_MAP_END()


                    // --- ISpxSessionEventArgs
                    virtual const std::wstring& GetSessionId() override;

                    // --- ISpxSessionEventArgsInit
                    virtual void Init(const std::wstring& sessionId) override;


                private:

                    CSpxTokenReqeustEventArgs(const CSpxTokenReqeustEventArgs&) = delete;
                    CSpxTokenReqeustEventArgs(const CSpxTokenReqeustEventArgs&&) = delete;

                    CSpxTokenReqeustEventArgs& operator=(const CSpxTokenReqeustEventArgs&) = delete;

                    std::wstring m_sessionId;
                };


            }
        }
    }
}
