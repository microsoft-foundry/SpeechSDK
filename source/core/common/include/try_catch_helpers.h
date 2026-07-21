//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// try_catch_helpers.h: Helper macros for all SPXAPI methods; use these in each public API function to
//                      ensure no C++ exceptions leak out of our C API implementation
//

#pragma once

#include <speechapi_c_common.h>
#include <exception.h>

using Microsoft::CognitiveServices::Speech::Impl::StoreException;

#define SPXAPI_INIT_HR_TRY(hr)                              \
{                                                           \
    SPX_INIT_HR(hr);                                        \
    try

#define SPXAPI_TRY()                                        \
{                                                           \
    try

#define SPXAPI_CATCH_AND_STORE_EXCEPTIONS(x)                \
    catch (SPXHR hrx)                                       \
    {                                                       \
        SPX_REPORT_ON_FAIL(hrx);                            \
        x = hrx;                                            \
    }                                                       \
    catch (ExceptionWithCallStack& ex)                      \
    {                                                       \
       x = StoreException(std::move(ex));                   \
    }                                                       \
    catch (const std::exception& ex)                        \
    {                                                       \
       x = StoreException(ex);                              \
    }                                                       \
    catch (...)                                             \
    {                                                       \
        SPX_REPORT_ON_FAIL(SPXERR_UNHANDLED_EXCEPTION);     \
        x = SPXERR_UNHANDLED_EXCEPTION;                     \
    }

#define SPXAPI_CATCH_ONLY()                                 \
    catch (SPXHR hrx)                                       \
    {                                                       \
        SPX_REPORT_ON_FAIL(hrx);                            \
        error = stringify(hrx);                             \
    }                                                       \
    catch (const ExceptionWithCallStack& ex)                \
    {                                                       \
       SPX_REPORT_ON_FAIL(ex.GetErrorCode());               \
       error = stringify(ex.GetErrorCode());                \
       error += " ";                                        \
       error += ex.what();                                  \
       error += " ";                                        \
       error += ex.GetCallStack();                          \
       SPX_TRACE_ERROR("ExceptionWithCallStack: %s", error.c_str());   \
    }                                                       \
    catch (const std::exception& e)                         \
    {                                                       \
        error = e.what();                                   \
        SPX_TRACE_ERROR("Exception: %s", error.c_str());    \
    }                                                       \
    catch (...)                                             \
    {                                                       \
        SPX_TRACE_ERROR("UNHANDLED Exception.");            \
        SPX_REPORT_ON_FAIL(SPXERR_UNHANDLED_EXCEPTION);     \
        error = "SPXERR_UNHANDLED_EXCEPTION";               \
    }                                                       \
}                                                           \

#define SPXAPI_CATCH_AND_RETURN_HR(hr)                      \
    SPXAPI_CATCH_AND_STORE_EXCEPTIONS(hr);                  \
    SPX_RETURN_HR(hr);                                      \
}

#define SPXAPI_CATCH_AND_LOG(x)                             \
    catch (SPXHR hrx)                                       \
    {                                                       \
        x = hrx;                                            \
        SPX_REPORT_ON_FAIL(x);                              \
    }                                                       \
    catch (const ExceptionWithCallStack& ex)                \
    {                                                       \
        x = ex.GetErrorCode();                              \
        SPX_REPORT_ON_FAIL(x);                              \
        SPX_TRACE_ERROR("ExceptionWithCallStack: %" PRIuPTR " %s. %s", x, ex.what(), ex.GetCallStack()); \
    }                                                       \
    catch (const std::exception& e)                         \
    {                                                       \
        x = SPXERR_UNHANDLED_EXCEPTION;                     \
        SPX_REPORT_ON_FAIL(x);                              \
        SPX_TRACE_ERROR("Exception: %s", e.what());         \
    }                                                       \
    catch (...)                                             \
    {                                                       \
        x = SPXERR_UNHANDLED_EXCEPTION;                     \
        SPX_REPORT_ON_FAIL(x);                              \
        SPX_TRACE_ERROR("UNHANDLED Exception.");            \
    }                                                       \

#define SPXAPI_CATCH_AND_RETURN(hr, x)                      \
    SPXAPI_CATCH_AND_LOG(hr)                                \
    return x;                                               \
}

#define SPXAPI_CATCH_CLEANUP_AND_RETURN_HR(hr)              \
    SPXAPI_CATCH_AND_STORE_EXCEPTIONS(hr);                  \
    SPX_EXITFN_CLEANUP:                                     \
    SPX_RETURN_HR(hr);                                      \
}

#define SPXAPI_CATCH_CLEANUP_AND_RETURN(hr, x)              \
    SPXAPI_CATCH_AND_LOG(hr);                               \
    SPX_EXITFN_CLEANUP:                                     \
    return x;                                               \
}
