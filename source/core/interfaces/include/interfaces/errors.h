//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <string>

#include <speechapi_cxx_enums.h>
#include "interfaces/base.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

class ISpxErrorInformation : public ISpxInterfaceBaseFor<ISpxErrorInformation>
{
public:
    enum class RetryMode { Allowed, NotAllowed };
    virtual const std::string& GetDetails() const = 0;
    virtual CancellationReason GetCancellationReason() const = 0;
    virtual CancellationErrorCode GetCancellationCode() const = 0;
    virtual int GetCategoryCode() const = 0;
    virtual int GetStatusCode() const = 0;
    virtual void SetRetryMode(RetryMode retryMode) = 0;
    virtual RetryMode GetRetryMode() const = 0;
};

} } } }
