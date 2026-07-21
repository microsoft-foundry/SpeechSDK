//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#pragma once

#include <algorithm>
#include <limits>
#include <vector>
#include <unordered_set>
#include <string>

#include "ajv.h"
#include "util/json_serializable.h"
#include "util/either.h"
#include "util/maybe.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {
namespace Message {

    /// <summary>
    /// A wrapper around a JSON reader to simplify de-serialization. This also tracks which elements
    /// in the JSON have been read, and provides a way to retrieve all unvisited elements. Please
    /// note that it only tracks elements read at the current level (not recursively)
    /// </summary>
    class VisitingJsonReaderView
    {
    private:
        ajv::JsonReader m_reader;
        std::unordered_set<std::string> m_visited;

    public:
        /// <summary>
        /// Creates a new view around the specified JSON reader
        /// </summary>
        /// <param name="reader">The JSON reader to read values from</param>
        VisitingJsonReaderView(const ajv::JsonReader& reader) :
            m_reader{ reader },
            m_visited{}
        {}

        /// <summary>
        /// Creates a new view around the specified JSON reader
        /// </summary>
        /// <param name="reader">The JSON reader to read values from</param>
        VisitingJsonReaderView(ajv::JsonReader&& reader) :
            m_reader{ std::move(reader) },
            m_visited{}
        {}

        /// <summary>
        /// Move constructor
        /// </summary>
        /// <param name="other">The view to move</param>
        VisitingJsonReaderView(VisitingJsonReaderView&& other) noexcept :
            m_reader{ other.m_reader },
            m_visited{ std::move(other.m_visited) }
        {}

        /// <summary>
        /// Move assignment operator
        /// </summary>
        /// <param name="other">The view to move</param>
        /// <returns>Reference to self</returns>
        VisitingJsonReaderView& operator=(VisitingJsonReaderView&& other) noexcept
        {
            if (this != &other)
            {
                m_reader = std::move(other.m_reader);
                m_visited = std::move(other.m_visited);
            }

            return *this;
        }

        /// <summary>
        /// Gets a JSON reader for the specified element
        /// </summary>
        /// <param name="name">The name of the JSON element to read</param>
        /// <returns>The JSON reader for that key. If the key does not exist, the JSON reader will
        /// return false for .IsOk()</returns>
        ajv::JsonReader operator[](const char* name)
        {
            m_visited.insert(name);
            return m_reader[name];
        }

        /// <summary>
        /// Gets a JSON reader for the specified element
        /// </summary>
        /// <param name="name">The name of the JSON element to read</param>
        /// <returns>The JSON reader for that key. If the key does not exist, the JSON reader will
        /// return false for .IsOk()</returns>
        ajv::JsonReader operator[](const std::string& name)
        {
            m_visited.insert(name);
            return m_reader[name.c_str()];
        }

        /// <summary>
        /// Gets a JSON reader for the element at the specified index. This can be used to retrieve
        /// values from a JSON array
        /// </summary>
        /// <param name="index">The index of the JSON element to read</param>
        /// <returns>The JSON reader for that index. If the index does not exist, the JSON reader will
        /// return false for .IsOk()</returns>
        ajv::JsonReader operator[](int index)
        {
            m_visited.insert(IndexToKey(index));
            return m_reader[index];
        }

        /// <summary>
        /// Reads a string value from the JSON
        /// </summary>
        /// <param name="name">The name of the JSON element to read</param>
        /// <param name="defaultValue">The value to return if the value doesn't exist</param>
        /// <returns>The string value at the specified key, or the default value if none exists</returns>
        std::string GetString(const char* name, const char* defaultValue = nullptr)
        {
            auto jsonVal = (*this)[name];
            return jsonVal.AsString(defaultValue ? defaultValue : "");
        }

        /// <summary>
        /// Gets a signed integer value from the JSON. Please note that if the value stored exceeds
        /// the max value for the type you are deserializing to, the value returned will be the max
        /// value for the type rather than a simple truncation to the right number of bits.
        /// </summary>
        /// <typeparam name="TInt">The type of signed integer to retrieve (e.g. int, int16_t)</typeparam>
        /// <param name="name">The name of the JSON element to read</param>
        /// <param name="defaultValue">The value to return if element does not exist</param>
        /// <returns>The signed integer value at the specified key, or the default value</returns>
        template<typename TInt, class = std::enable_if_t<std::is_integral<TInt>::value && std::is_signed<TInt>::value>>
        TInt GetInt(const char* name, TInt defaultValue = 0)
        {
            int64_t val = GetInt<int64_t>(name, defaultValue);
            return static_cast<TInt>(
                std::min(
                    val,
                    (int64_t)std::numeric_limits<TInt>::max()
                )
            );
        }

        /// <summary>
        /// Gets an unsigned integer value from the JSON. Please note that if the value stored exceeds
        /// the max value for the type you are deserializing to, the value returned will be the max
        /// value for the type rather than a simple truncation to the right number of bits.
        /// </summary>
        /// <typeparam name="TUint">The type of the unsigned integer to retrieve (e.g. unsigned int, uint32_t)</typeparam>
        /// <param name="name">The name of the JSON element to read</param>
        /// <param name="defaultValue">The value to return if element does not exist</param>
        /// <returns>The unsigned integer at the specified key, or the default value</returns>
        template<typename TUint, class = std::enable_if_t<std::is_integral<TUint>::value && std::is_unsigned<TUint>::value>>
        TUint GetUint(const char* name, TUint defaultValue = 0)
        {
            uint64_t val = GetUint<uint64_t>(name, defaultValue);
            return static_cast<TUint>(
                std::min(
                    val,
                    (uint64_t)std::numeric_limits<TUint>::max()
                )
            );
        }

        /// <summary>
        /// Gets a Maybe value from the JSON
        /// </summary>
        /// <typeparam name="TVal">The type of the value to parse</typeparam>
        /// <param name="name">The name of the JSON element to read</param>
        /// <returns>If the specified key did not exist, returns an empty Maybe. Otherwise returns a Maybe
        /// instantiated with the deserialized value</returns>
        template<typename TVal>
        Maybe<TVal> GetMaybe(const char* name)
        {
            auto json = (*this)[name];
            if (json.IsObject())
            {
                return Maybe<TVal>{ TVal{ json } };
            }
            else
            {
                return Maybe<TVal>{};
            }
        }

        /// <summary>
        /// Gets an Either from the JSON
        /// </summary>
        /// <typeparam name="TLeft">The type of the first value in the either</typeparam>
        /// <typeparam name="TRight">The type of the second value in the either</typeparam>
        /// <param name="name">The name of the JSON element to read</param>
        /// <param name="isLeft">Whether or not we are deserializing a left or right value</param>
        /// <returns>The deserialized Either value</returns>
        template<typename TLeft, typename TRight>
        Either<TLeft, TRight> GetEither(const char* name, bool isLeft)
        {
            if (isLeft)
            {
                return TLeft{ VisitingJsonReaderView{ m_reader[name] } };
            }
            else
            {
                return TRight{ VisitingJsonReaderView{ m_reader[name] } };
            }
        }

        /// <summary>
        /// Deserializes an array of items in JSON into a vector
        /// </summary>
        /// <typeparam name="TItem">The type of the items in the list</typeparam>
        /// <param name="name">The name of the JSON element to read</param>
        /// <returns>The deserialized vector of items at the specified key. If key didn't exist, or
        /// there were no items, then an empty vector will be returned</returns>
        template<typename TItem, class = std::enable_if_t<!std::is_integral<TItem>::value>>
        std::vector<TItem> GetVector(const char* name)
        {
            std::vector<TItem> items;

            auto arrayJson = (*this)[name];
            if (arrayJson.IsArray())
            {
                for (int i = 0; i < arrayJson.ValueCount(); i++)
                {
                    items.emplace_back(VisitingJsonReaderView{ arrayJson.ValueAt(i) });
                }
            }

            return items;
        }

        /// <summary>
        /// Gets a view for the specified key in the JSON
        /// </summary>
        /// <param name="name">The name of the JSON element to read</param>
        /// <returns>The view for that key</returns>
        VisitingJsonReaderView GetView(const char* name)
        {
            auto json = (*this)[name];
            return VisitingJsonReaderView{ json };
        }

        /// <summary>
        /// Returns the JSON for all elements that have not been visited yet
        /// </summary>
        /// <returns>The unvisited elements. If all elements were visited, or there are no elements,
        /// an empty JSON builder will be returned</returns>
        ajv::JsonBuilder GetUnvisitedElements() const
        {
            ajv::JsonBuilder builder;
            auto kind = m_reader.Kind();

            switch (kind)
            {
            case ajv::JsonKind::Error:
            case ajv::JsonKind::End:
            case ajv::JsonKind::Null:
            case ajv::JsonKind::Unspecified:
            case ajv::JsonKind::String:
            case ajv::JsonKind::Number:
            case ajv::JsonKind::Boolean:
                // Don't know how to handle these so skip them
                break;

            case ajv::JsonKind::Array:
            {
                for (auto i = 0; i < m_reader.ValueCount(); i++)
                {
                    if (m_visited.end() == m_visited.find(IndexToKey(i)))
                    {
                        auto value = m_reader.ValueAt(i);
                        builder[i] = value;
                    }
                }
            }
                break;

            case ajv::JsonKind::Object:
            {
                for (auto i = 0; i < m_reader.ValueCount(); i++)
                {
                    auto key = m_reader.NameAt(i).AsString();
                    if (m_visited.end() == m_visited.find(key))
                    {
                        auto value = m_reader.ValueAt(i);
                        builder[key] = value;
                    }
                }
            }
                break;
            }

            return builder;
        }

    private:
        VisitingJsonReaderView(const VisitingJsonReaderView&) = delete;
        VisitingJsonReaderView& operator=(const VisitingJsonReaderView&) = delete;

        class RoundTripSerializable
        {
        private:
            ajv::JsonBuilder m_builder;

        public:
            RoundTripSerializable(const ajv::JsonReader& reader) :
                m_builder{ reader.AsJson() }
            {}

            RoundTripSerializable(ajv::JsonBuilder&& builder) :
                m_builder{ std::move(builder) }
            {}

            ajv::JsonBuilder Serialize() const
            {
                return m_builder;
            }
        };

        static std::string IndexToKey(int index)
        {
            return "__<<#" + std::to_string(index) + ">>__";
        }
    };

    template<>
    AJV_FN_NO_INLINE_(uint64_t) VisitingJsonReaderView::GetUint<uint64_t>(const char* name, uint64_t defaultValue)
    {
        auto val = (*this)[name];
        return val.AsUint64(defaultValue);
    }

    template<>
    AJV_FN_NO_INLINE_(int64_t) VisitingJsonReaderView::GetInt<int64_t>(const char* name, int64_t defaultValue)
    {
        auto jsonVal = (*this)[name];
        return jsonVal.AsInt64(defaultValue);
    }

    template<>
    AJV_FN_NO_INLINE_(Maybe<std::string>) VisitingJsonReaderView::GetMaybe<std::string>(const char* name)
    {
        auto jsonVal = (*this)[name];
        if (jsonVal.IsString())
        {
            return Maybe<std::string>{ jsonVal.AsString() };
        }
        else
        {
            return Maybe<std::string>{};
        }
    }

    template<>
    AJV_FN_NO_INLINE_(Maybe<JSONSerializable>) VisitingJsonReaderView::GetMaybe<JSONSerializable>(const char* name)
    {
        auto jsonConfig = (*this)[name];
        if (jsonConfig.IsOk())
        {
            return JSONSerializable{ RoundTripSerializable{ jsonConfig } };
        }
        else
        {
            return Maybe<JSONSerializable>{};
        }
    }

}}}}}
