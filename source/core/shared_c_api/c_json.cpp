//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"
#include "speechapi_c_json.h"
#include <cstring>
#include <memory>
#include <ajv.h>
#include <stdint.h>
#include "spxdebug.h"
#include "handle_helpers.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

// NOTE: All of the functions that use SPXAPI_CATCH_AND_RETURN(hr, defaultValue); here are leaking
//       memory when exceptions happen. Internally SPXAPI_CATCH_AND_RETURN calls SPXAPI_CATCH
//       which captures and stores exception. The handle to that stored exception is then thrown away
//       and the "default" value is returned.

SPXAPI__(const char*) ai_core_string_create(const char* ptr, size_t size)
{
    if (ptr == nullptr)
    {
        return nullptr;
    }

    try
    {
        auto result = new char[size + 1];
        PAL::strcpy(result, size + 1, ptr, size, true);
        result[size] = '\0';
        return result;
    }
    catch (...)
    {
        SPX_REPORT_ON_FAIL(SPXERR_UNHANDLED_EXCEPTION);
    }
    return nullptr;
}

SPXAPI_(void) ai_core_string_free(const char* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    try
    {
        delete[] ptr;
    }
    catch (...)
    {
        SPX_REPORT_ON_FAIL(SPXERR_UNHANDLED_EXCEPTION);
    }
}

int operator*(const ajv::JsonReader& reader)
{
    int item = -1;
    reader.View(&item);
    return item;
}

int operator*(const ajv::JsonBuilder::JsonWriter& writer)
{
    int item = -1;
    writer.Builder(&item);
    return item;
}

template <class T, class U, class V>
T ai_core_json_helper_reader_writer(SPXHANDLE parserOrBuilder, int item,
    U (*rfn)(const ajv::JsonReader&, V),
    U (*wfn)(const ajv::JsonBuilder::JsonWriter&, V),
    V parameter2,
    T defaultValue)
{
    if (parserOrBuilder == nullptr)
    {
        return defaultValue;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto parser = SpxTryGetPtrFromHandle<ajv::JsonParser>(parserOrBuilder);
        if (parser != nullptr)
        {
            return (T)rfn(parser->Reader(item), parameter2);
        }

        auto builder = SpxTryGetPtrFromHandle<ajv::JsonBuilder>(parserOrBuilder);
        if (builder == nullptr)
        {
            return defaultValue;
        }

        return (T)wfn(builder->Writer(item), parameter2);
    }
    SPXAPI_CATCH_AND_RETURN(hr, defaultValue);
}

template <class T, class U, class V>
T ai_core_json_helper_writer(SPXHANDLE builder, int item,
    U (*wfn)(ajv::JsonBuilder::JsonWriter&, V),
    V parameter2,
    T defaultValue)
{
    if (builder == nullptr)
    {
        return defaultValue;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto ptr = SpxTryGetPtrFromHandle<ajv::JsonBuilder>(builder);
        if (ptr == nullptr)
        {
            return defaultValue;
        }

        auto writer = ptr->Writer(item);
        return (T)wfn(writer, parameter2);
    }
    SPXAPI_CATCH_AND_RETURN(hr, defaultValue);
}

template <class T>
int ai_core_json_helper_create(SPXHANDLE* parserOrBuilder, const char* json, size_t jsize,
    int (*fn)(T&),
    int defaultValue = -1)
{
    if (parserOrBuilder == nullptr)
    {
        return -1;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        *parserOrBuilder = SPXHANDLE_INVALID;

        auto ptr = std::make_shared<T>(std::string(json, jsize));
        auto item = fn(*ptr.get());

        *parserOrBuilder = CSpxSharedPtrHandleTableManager::TrackHandle<T, SPXHANDLE>(ptr);
        return item;
    }
    SPXAPI_CATCH_AND_RETURN(hr, defaultValue);
}

SPXAPI_(int) ai_core_json_parser_create(SPXHANDLE* parser, const char* json, size_t jsize)
{
    return ai_core_json_helper_create<ajv::JsonParser>(parser, json, jsize,
        [](auto& parser){ int item = -1; parser.View(&item); return item; },
        -1);
}

SPXAPI_(bool) ai_core_json_parser_handle_is_valid(SPXHANDLE parser)
{
    return CSpxApiManager::IsValid<SPXHANDLE, ajv::JsonParser>(parser);
}

SPXAPI ai_core_json_parser_handle_release(SPXHANDLE parser)
{
    return CSpxApiManager::Release<SPXHANDLE, ajv::JsonParser>(parser);
}

SPXAPI_(int) ai_core_json_builder_create(SPXHANDLE* builder, const char* json, size_t jsize)
{
    return ai_core_json_helper_create<ajv::JsonBuilder>(builder, json, jsize,
        [](auto& builder){ int item = -1; builder.Writer().Builder(&item); return item; },
        -1);
}

SPXAPI_(bool) ai_core_json_builder_handle_is_valid(SPXHANDLE builder)
{
    return CSpxApiManager::IsValid<SPXHANDLE, ajv::JsonBuilder>(builder);
}

SPXAPI ai_core_json_builder_handle_release(SPXHANDLE builder)
{
    return CSpxApiManager::Release<SPXHANDLE, ajv::JsonBuilder>(builder);
}

SPXAPI_(int) ai_core_json_item_count(SPXHANDLE parserOrBuilder, int item)
{
    return ai_core_json_helper_reader_writer<int, int, int>(parserOrBuilder, item,
        [](auto& r, auto){ return r.ValueCount(); },
        [](auto& w, auto){ return w.ValueCount(); },
        0,
        0);
}

SPXAPI_(int) ai_core_json_item_at(SPXHANDLE parserOrBuilder, int item, int index, const char* find)
{
    return find != nullptr
        ? ai_core_json_helper_reader_writer<int, int, const char*>(parserOrBuilder, item,
            [](auto& r, auto find){ return *r.ValueAt(find); },
            [](auto& w, auto find){ return *w.ValueAt(find); },
            find,
            -1)
        : ai_core_json_helper_reader_writer<int, int, int>(parserOrBuilder, item,
            [](auto& r, auto index){ return *r.ValueAt(index); },
            [](auto& w, auto index){ return *w.ValueAt(index); },
            index,
            -1);
}

SPXAPI_(int) ai_core_json_item_next(SPXHANDLE parserOrBuilder, int item)
{
    return ai_core_json_helper_reader_writer<int, int, int>(parserOrBuilder, item,
        [](auto& r, auto){ return *r.Next(); },
        [](auto& w, auto){ return *w.Next(); },
        0,
        0);
}

SPXAPI_(int) ai_core_json_item_name(SPXHANDLE parserOrBuilder, int item)
{
    return ai_core_json_helper_reader_writer<int, int, int>(parserOrBuilder, item,
        [](auto& r, auto){ return *r.Name(); },
        [](auto& w, auto){ return *w.Name(); },
        0,
        0);
}

SPXAPI_(int) ai_core_json_value_kind(SPXHANDLE parserOrBuilder, int item)
{
    return ai_core_json_helper_reader_writer<int, int, int>(parserOrBuilder, item,
        [](auto& r, auto){ return (int)r.Kind(); },
        [](auto& w, auto){ return (int)w.Kind(); },
        -1,
        -1);
}

SPXAPI_(bool) ai_core_json_value_as_bool(SPXHANDLE parserOrBuilder, int item, bool defaultValue)
{
    return ai_core_json_helper_reader_writer<bool, bool, bool>(parserOrBuilder, item,
        [](auto& r, auto defaultValue){ return r.AsBool(defaultValue); },
        [](auto& w, auto defaultValue){ return w.AsBool(defaultValue); },
        defaultValue,
        defaultValue);
}

SPXAPI_(double) ai_core_json_value_as_double(SPXHANDLE parserOrBuilder, int item, double defaultValue)
{
    return ai_core_json_helper_reader_writer<double, double, double>(parserOrBuilder, item,
        [](auto& r, auto defaultValue){ return r.AsNumber(defaultValue); },
        [](auto& w, auto defaultValue){ return w.AsNumber(defaultValue); },
        defaultValue,
        defaultValue);
}

SPXAPI_(int64_t) ai_core_json_value_as_int(SPXHANDLE parserOrBuilder, int item, int64_t defaultValue)
{
    return ai_core_json_helper_reader_writer<int64_t, int64_t, int64_t>(parserOrBuilder, item,
        [](auto& r, auto defaultValue){ return r.AsInt64(defaultValue); },
        [](auto& w, auto defaultValue){ return w.AsInt64(defaultValue); },
        defaultValue,
        defaultValue);
}

SPXAPI_(uint64_t) ai_core_json_value_as_uint(SPXHANDLE parserOrBuilder, int item, uint64_t defaultValue)
{
    return ai_core_json_helper_reader_writer<uint64_t, uint64_t, uint64_t>(parserOrBuilder, item,
        [](auto& r, auto defaultValue){ return r.AsUint64(defaultValue); },
        [](auto& w, auto defaultValue){ return w.AsUint64(defaultValue); },
        defaultValue,
        defaultValue);
}

// This function returns the string unchanged from it's JSON value.
SPXAPI__(const char*) ai_core_json_value_as_string_ptr(SPXHANDLE parserOrBuilder, int item, size_t* size)
{
    auto defaultValue = nullptr;
    SPX_IFTRUE(size != nullptr, *size = 0);

    return ai_core_json_helper_reader_writer<const char*, const char*, size_t*>(parserOrBuilder, item,
        [](auto& r, auto size){ return r.AsStringPtr(size); },
        [](auto& w, auto size){ return w.AsStringPtr(size); },
        size,
        defaultValue);
}

// This function not only makes a copy but will convert ascii unicode values
// (json compatible) to UTF8 encoded strings (not json compatible).
SPXAPI__(const char*) ai_core_json_value_as_string_copy(SPXHANDLE parserOrBuilder, int item, const char* defaultValue)
{
    return ai_core_json_helper_reader_writer<const char*, const char*, const char*>(parserOrBuilder, item,
        [](auto& r, auto defaultValue){
            if (!r.IsString())
            {
                return defaultValue;
            }

            auto str = r.AsString();
            return ai_core_string_create(str.c_str(), str.length());
        },
        [](auto& w, auto defaultValue){
            if (!w.IsString())
            {
                return defaultValue;
            }

            auto str = w.AsString();
            return ai_core_string_create(str.c_str(), str.length());
        },
        defaultValue,
        defaultValue);
}

SPXAPI__(const char*) ai_core_json_value_as_json_copy(SPXHANDLE parserOrBuilder, int item)
{
    return ai_core_json_helper_reader_writer<const char*, const char*, size_t*>(parserOrBuilder, item,
        [](auto& r, auto){
            size_t size = 0;
            auto ptr = r.AsJsonPtr(&size);
            return ai_core_string_create(ptr, size);
        },
        [](auto& w, auto){
            auto str = w.AsJson();
            auto ptr = str.c_str();
            return *ptr == '\0' ? nullptr : ai_core_string_create(ptr, str.length());
        },
        nullptr,
        nullptr);
}

SPXAPI_(int) ai_core_json_builder_item_add(SPXHANDLE builder, int item, int index, const char* find)
{
    return find == nullptr
        ? ai_core_json_helper_writer<int, int, int>(builder, item,
            [](auto& w, auto index){ return (int)*w.ValueAt(index, true); },
            index,
            -1)
        : ai_core_json_helper_writer<int, int, const char*>(builder, item,
            [](auto& w, auto find){ return *w.ValueAt(find, true); },
            find,
            -1);
}

SPXAPI ai_core_json_builder_item_set(SPXHANDLE builder, int item, const char* json, size_t jsize, int kind, const char* str, size_t ssize, bool boolean, int integer, double number)
{
    if (builder == nullptr)
    {
        return SPXERR_INVALID_ARG;
    }

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto jsonBuilder = SpxTryGetPtrFromHandle<ajv::JsonBuilder>(builder);
        SPX_IFTRUE_RETURN_HR(jsonBuilder == nullptr, SPXERR_INVALID_HANDLE);

        auto writer = jsonBuilder->Writer(item);
        if (kind == (int)ajv::JsonKind::String)
        {
            writer = std::string(str, ssize);
        }
        else if (kind == (int)ajv::JsonKind::Boolean)
        {
            writer = boolean;
        }
        else if (kind == (int)ajv::JsonKind::Number)
        {
            if (number != 0)
            {
                writer = number;
            }
            else if (integer != 0)
            {
                writer = integer;
            }
            else
            {
                writer = 0;
            }
        }
        else if (kind == (int)ajv::JsonKind::Array || kind == (int)ajv::JsonKind::Object || json != nullptr)
        {
            hr = writer.Parse(std::string(json, jsize)).IsOk() ? SPX_NOERROR : SPXERR_INVALID_ARG;
        }
        else
        {
            hr = SPXERR_INVALID_ARG;
        }
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
