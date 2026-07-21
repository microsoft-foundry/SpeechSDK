//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <memory>

#include <interfaces/base.h>
#include <interfaces/keyword.h>
#include <interfaces/results.h>

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {


SPX_INTERFACE(ISpxRecognizer)
{
public:
    virtual void SetDisposing() = 0;

    virtual CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync() = 0;
    virtual CSpxAsyncOp<void> StartContinuousRecognitionAsync() = 0;
    virtual CSpxAsyncOp<void> StopContinuousRecognitionAsync() = 0;

    virtual CSpxAsyncOp<std::shared_ptr<ISpxRecognitionResult>> RecognizeAsync(std::shared_ptr<ISpxKwsModel> model) = 0;
    virtual CSpxAsyncOp<void> StartKeywordRecognitionAsync(std::shared_ptr<ISpxKwsModel> model) = 0;
    virtual CSpxAsyncOp<void> StopKeywordRecognitionAsync() = 0;

    virtual void OpenConnection(bool forContinuousRecognition) = 0;
    virtual void CloseConnection() = 0;
};

} } } }
