//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
#pragma once

#include <algorithm>
#include <tuple>
#include <type_traits>
#include <vector>

#include "util/either.h"
#include "util/traits.h"
#include "util/maybe.h"

namespace Microsoft {
namespace CognitiveServices {
namespace Speech {
namespace Impl {

namespace Details
{
    template<template<typename...> class TResult, typename... T>
    class ResultImpl {};

    template<template<typename...> class TResult, typename T, typename E>
    class ResultImpl<TResult, T, E>
    {
        static_assert(!std::is_same<T, E>::value, "Value and Error types should be different");
        static_assert(!std::is_void<E>::value, "Error type can't be void");

    public:
        using ValueType = T;
        using ErrorType = E;

        explicit ResultImpl(T value): m_data{ std::move(value) }
        {}

        explicit ResultImpl(E error): m_data{ std::move(error) }
        {}

        ResultImpl(const ResultImpl& other): m_data{ other.m_data }
        {}

        ResultImpl(ResultImpl&& other): m_data{ std::move(other.m_data) }
        {}

        ResultImpl& operator=(const ResultImpl& rhs)
        {
            m_data = rhs.m_data;
            return *this;
        }

        ResultImpl& operator=(ResultImpl&& rhs)
        {
            m_data = std::move(rhs.m_data);
            return *this;
        }

        ~ResultImpl() = default;

        operator bool() const &
        {
            return m_data.template Has<T>();
        }

        const T& Get() const &
        {
            return m_data.template Get<T>();
        }

        T&& Get() &&
        {
            return std::move(m_data).template Get<T>();
        }

        const E& Error() const &
        {
            return m_data.template Get<E>();
        }

        E&& Error() &&
        {
            return std::move(m_data).template Get<E>();
        }

        template<typename U, typename F, std::enable_if_t<!std::is_void<U>::value && MatchesSignature<F, TResult<U, E>, T>::value, int> = 0>
        TResult<U, E> AndThen(F&& fn)
        {
            if (m_data.template Has<T>())
            {
                return fn(std::move(m_data).template Get<T>());
            }
            return TResult<U, E>{ std::move(m_data).template Get<E>() };
        }

        template<typename U = void, typename F, std::enable_if_t<std::is_void<U>::value && MatchesSignature<F, TResult<E>, T>::value, int> = 0>
        TResult<E> AndThen(F&& fn)
        {
            if (m_data.template Has<T>())
            {
                return fn(std::move(m_data).template Get<T>());
            }
            return TResult<E>{ std::move(m_data).template Get<E>() };
        }

        template<typename U, typename F, std::enable_if_t<MatchesSignature<F, TResult<T, U>, E>::value, int> = 0>
        TResult<T, U> OrElse(F fn)
        {
            if (m_data.template Has<T>())
            {
                return TResult<T, U>{ std::move(m_data).template Get<T>() };
            }
            return fn(std::move(m_data).template Get<E>());
        }

        template<typename U, typename F, std::enable_if_t<MatchesSignature<F, U, T>::value,  int> = 0>
        TResult<U, E> Map(F fn)
        {
            if (m_data.template Has<T>())
            {
                return TResult<U, E>{ fn(std::move(m_data).template Get<T>()) };
            }
            return TResult<U, E>{ std::move(m_data).template Get<E>() };
        }

        template<typename U, typename F, std::enable_if_t<MatchesSignature<F, U, E>::value, int> = 0>
        TResult<T, U> MapErr(F fn)
        {
            if (m_data.template Has<T>())
            {
                return TResult<T, U>{ std::move(m_data).template Get<T>() };
            }
            return TResult<T, U>{ fn(std::move(m_data).template Get<E>()) };
        }

        template<typename U, typename F, std::enable_if_t<MatchesSignature<F, TResult<U, E>>::value, int> = 0>
        TResult<std::tuple<T, U>, E> Also(F fn)
        {
            using R = TResult<std::tuple<T, U>, E>;
            if (m_data.template Has<T>())
            {
                return fn().template AndThen<std::tuple<T, U>>([&](auto value)
                {
                    return R{ std::make_tuple(std::move(m_data).template Get<T>(), std::move(value)) };
                });
            }
            return R{ std::move(m_data).template Get<E>() };
        }

        template<typename Exception, typename F, std::enable_if_t<MatchesSignature<F, Exception, E>::value, int> = 0>
        ValueType Unwrap(F onError)
        {
            if (m_data.template Has<T>())
            {
                return std::move(m_data).template Get<T>();
            }
            auto exception = onError(std::move(m_data).template Get<E>());
            throw exception;
        }

    protected:
        Either<ValueType, ErrorType> m_data;
    };

    template<template<typename...> class TResult, typename E>
    class ResultImpl<TResult, E>
    {
    public:
        using ValueType = void;
        using ErrorType = E;

        explicit ResultImpl(): m_error{ nullptr }
        {}

        explicit ResultImpl(E error): m_error{ std::move(error) }
        {}

        ResultImpl(const ResultImpl& other): m_error{ other.m_error }
        {}

        ResultImpl(ResultImpl&& other): m_error{ std::move(other.m_error) }
        {}

        ResultImpl& operator=(const ResultImpl& rhs)
        {
            if (this != &rhs)
            {
                m_error = rhs.m_error;
            }
            return *this;
        }

        ResultImpl& operator=(ResultImpl&& rhs)
        {
            if (this != &rhs)
            {
                m_error = std::move(rhs.m_error);
            }
            return *this;
        }

        ~ResultImpl() = default;

        operator bool() const &
        {
            return !m_error;
        }

        const E& Error() const &
        {
            return m_error.Get();
        }

        E&& Error() &&
        {
            return std::move(m_error).Get();
        }

        template<typename U, typename F, std::enable_if_t<MatchesSignature<F, TResult<U, E>>::value, int> = 0>
        TResult<U, E> AndThen(F fn)
        {
            if (!m_error)
            {
                return fn();
            }
            return TResult<U, E>{ std::move(m_error).Get() };
        }

        template<typename U = void, typename F, std::enable_if_t<std::is_void<U>::value && MatchesSignature<F, TResult<E>>::value, int> = 0>
        TResult<E> AndThen(F fn)
        {
            if (!m_error)
            {
                return fn();
            }
            return TResult<E>{ std::move(m_error).Get() };
        }

        template<typename U = E, typename F, std::enable_if_t<MatchesSignature<F, TResult<U>, E>::value, int> = 0>
        TResult<U> OrElse(F fn)
        {
            if (!m_error)
            {
                return TResult<U>{};
            }
            return fn(std::move(m_error).Get());
        }

        template<typename U, typename F, std::enable_if_t<MatchesSignature<F, U, E>::value, int> = 0>
        TResult<U> MapErr(F mapFn)
        {
            if (!m_error)
            {
                return TResult<U>{};
            }
            return TResult<U>{ mapFn(std::move(m_error).Get()) };
        }

        template<typename Exception, typename F, std::enable_if_t<MatchesSignature<F, Exception, E>::value, int> = 0>
        ValueType Unwrap(F onError)
        {
            if (m_error)
            {
                auto exception = onError(std::move(m_error).Get());
                throw exception;
            }
        }

    protected:
        Maybe<ErrorType> m_error;
    };


    template<typename T>
    struct GetBase
    {};

    template<template<typename...> class R, typename... Ts>
    struct GetBase<R<Ts...>>
    {
        using Type = ResultImpl<R, Ts...>;
    };
}

template<typename... T>
class Result
{};


template<typename T, typename E>
class Result<T, E>: private Details::ResultImpl<Result, T, E>
{
    using Base = typename Details::GetBase<Result>::Type;
    static_assert(!IsContainer<T>::value, "Specialization not valid for containers.");
    static_assert(!std::is_same<T, E>::value, "Value and Error types should be different");
    static_assert(!std::is_void<E>::value, "Error type can't be void");
public:
    using ValueType = typename Base::ValueType;
    using ErrorType = typename Base::ErrorType;

    explicit Result(T value): Base{ std::forward<T>(value) }
    {}

    explicit Result(E error): Base{ std::forward<E>(error) }
    {}

    Result(const Result& other) = default;
    Result(Result&& other) = default;
    Result& operator=(const Result& rhs) = default;
    Result& operator=(Result&& rhs) = default;
    ~Result() = default;

    using Base::operator bool;
    using Base::Get;
    using Base::Error;
    using Base::AndThen;
    using Base::OrElse;
    using Base::Map;
    using Base::MapErr;
    using Base::Also;
    using Base::Unwrap;

    template<typename I, typename F, std::enable_if_t<
        std::is_integral<I>::value &&
        MatchesSignature<F, Result<T, E>, I>::value, int> = 0>
    static Result<std::vector<T>, E> Range(I count, F fn)
    {
        using R = Result<std::vector<T>, E>;
        std::vector<T> result{};
        for (I i{ 0 }; i < count; i++)
        {
            auto item = fn(i);
            if (item)
            {
                result.push_back(std::move(item).Get());
            }
            else
            {
                return R{ std::move(item).Error() };
            }
        }
        return R{ std::move(result) };
    }
};

template<typename T, typename E>
class Result<std::vector<T>, E>: private Details::ResultImpl<Result, std::vector<T>, E>
{
    using Base = typename Details::GetBase<Result>::Type;
    static_assert(!std::is_same<std::vector<T>, E>::value, "Value and Error types should be different");
    static_assert(!std::is_void<E>::value, "Error type can't be void");
public:
    using ValueType = typename Base::ValueType;
    using ErrorType = typename Base::ErrorType;

    explicit Result(ValueType value): Base{ std::forward<ValueType>(value) }
    {}

    explicit Result(E error): Base{ std::forward<E>(error) }
    {}

    Result(const Result& other) = default;
    Result(Result&& other) = default;
    Result& operator=(const Result& rhs) = default;
    Result& operator=(Result&& rhs) = default;
    ~Result() = default;

    using Base::operator bool;
    using Base::Get;
    using Base::Error;

    using Base::AndThen;
    using Base::OrElse;
    using Base::Map;
    using Base::MapErr;
    using Base::Also;
    using Base::Unwrap;

    template<typename U, typename F, std::enable_if_t<MatchesSignature<F, Result<U, E>, T>::value, int> = 0>
    Result<std::vector<U>, E> ForEach(F fn)
    {
        using R = Result<std::vector<U>, E>;
        if (Base::m_data.template Has<ValueType>())
        {
            std::vector<U> collection{};

            for (auto&& item : std::move(Base::m_data).template Get<ValueType>())
            {
                auto itemResult = fn(std::move(item));
                if (itemResult)
                {
                    collection.push_back(std::move(itemResult).Get());
                }
                else
                {
                    return R{ std::move(itemResult).Error() };
                }
            }
            return R{ std::move(collection) };
        }
        else
        {
            return R{ std::move(Base::m_data).template Get<ErrorType>() };
        }
    }

    template<typename F, std::enable_if_t<MatchesSignature<F, bool, const T&>::value, int> = 0>
    Result<Maybe<T>, E> FindFirst(F predicate)
    {
        using M = Maybe<T>;
        using R = Result<M, E>;
        if (Base::m_data.template Has<ValueType>())
        {
            for (auto&& item : std::move(Base::m_data).template Get<ValueType>())
            {
                auto found = predicate(item);
                if (found)
                {
                    return R{ M{ std::move(item) } };
                }
            }
            return R{ M{} };
        }
        else
        {
            return R{ std::move(Base::m_data).template Get<ErrorType>() };
        }
    }

    template<typename F, std::enable_if_t<MatchesSignature<F, bool, const T&>::value, int> = 0>
    Result<Maybe<size_t>, E> FindIndexOfFirst(F predicate)
    {
        using M = Maybe<size_t>;
        using R = Result<M, E>;
        if (Base::m_data.template Has<ValueType>())
        {
            const auto& vector = Base::m_data.template Get<ValueType>();
            for (size_t i{ 0 }; i < vector.size(); i++)
            {
                const auto& item = vector[i];
                auto found = predicate(item);
                if (found)
                {
                    return R{ M{ i } };
                }
            }
            return R{ M{} };
        }
        else
        {
            return R{ std::move(Base::m_data).template Get<ErrorType>() };
        }
    }

    template<typename F, std::enable_if_t<MatchesSignature<F, bool, const T&, const T&>::value, int> = 0>
    Result<std::vector<T>, E> Sorted(F compare)
    {
        using R = Result<std::vector<T>, E>;
        if (Base::m_data.template Has<ValueType>())
        {
            auto v = std::move(Base::m_data).template Get<ValueType>();
            std::sort(v.begin(), v.end(), compare);
            return R{ std::move(v) };
        }
        else
        {
            return R{ std::move(Base::m_data).template Get<ErrorType>() };
        }
    }

    Result<Maybe<T>, E> First()
    {
        using M = Maybe<T>;
        using R = Result<M, E>;
        if (Base::m_data.template Has<ValueType>())
        {
            auto v = std::move(Base::m_data).template Get<ValueType>();
            if (v.empty())
            {
                return R{ M{} };
            }
            else
            {
                return R{ M{ std::move(v[0]) } };
            }
        }
        else
        {
            return R{ std::move(Base::m_data).template Get<ErrorType>() };
        }
    }
};

template<typename T, typename E>
class Result<Maybe<T>, E>: private Details::ResultImpl<Result, Maybe<T>, E>
{
    using Base = typename Details::GetBase<Result>::Type;
    static_assert(!std::is_same<Maybe<T>, E>::value, "Value and Error types should be different");
    static_assert(!std::is_void<E>::value, "Error type can't be void");
public:
    using ValueType = typename Base::ValueType;
    using ErrorType = typename Base::ErrorType;

    explicit Result(ValueType value): Base{ std::forward<ValueType>(value) }
    {}

    explicit Result(E error): Base{ std::forward<E>(error) }
    {}

    Result(const Result& other) = default;
    Result(Result&& other) = default;
    Result& operator=(const Result& rhs) = default;
    Result& operator=(Result&& rhs) = default;
    ~Result() = default;

    using Base::operator bool;
    using Base::Get;
    using Base::Error;

    using Base::AndThen;
    using Base::OrElse;
    using Base::Map;
    using Base::MapErr;
    using Base::Also;
    using Base::Unwrap;

    Result<T, E> HasOr(E error)
    {
        using R = Result<T, E>;
        if (Base::m_data.template Has<ValueType>())
        {
            auto maybeValue = std::move(Base::m_data).template Get<ValueType>();
            if (maybeValue)
            {
                return R{ std::move(maybeValue).Get() };
            }
            else
            {
                return R{ std::move(error) };
            }
        }
        else
        {
            return R{ std::move(Base::m_data).template Get<ErrorType>() };
        }
    }
};

template<typename E,  typename... Ts>
class Result<std::tuple<Ts...>, E>: private Details::ResultImpl<Result, std::tuple<Ts...>, E>
{
    using Base = typename Details::GetBase<Result>::Type;
    static_assert(!std::is_same<std::tuple<Ts...>, E>::value, "Value and Error types should be different");
    static_assert(!std::is_void<E>::value, "Error type can't be void");
public:
    using ValueType = typename Base::ValueType;
    using ErrorType = typename Base::ErrorType;

    explicit Result(ValueType value): Base{ std::forward<ValueType>(value) }
    {}

    explicit Result(E error): Base{ std::forward<E>(error) }
    {}

    Result(const Result& other) = default;
    Result(Result&& other) = default;
    Result& operator=(const Result& rhs) = default;
    Result& operator=(Result&& rhs) = default;
    ~Result() = default;

    using Base::operator bool;
    using Base::Get;
    using Base::Error;

    using Base::AndThen;
    using Base::OrElse;
    using Base::Map;
    using Base::MapErr;
    using Base::Unwrap;

    template<typename U, typename F, std::enable_if_t<MatchesSignature<F, Result<U, E>>::value, int> = 0>
    Result<std::tuple<Ts..., U>, E> Also(F fn)
    {
        using T = std::tuple<Ts..., U>;
        using R = Result<T, E>;
        if (Base::m_data.template Has<ValueType>())
        {
            auto left = std::move(Base::m_data).template Get<ValueType>();
            auto newResult = fn();
            return newResult.template AndThen<T>([&, left = std::move(left)](auto value)
            {
                return R{
                    std::tuple_cat(
                        std::move(left),
                        std::make_tuple(std::move(value))
                    )
                };
            });
        }
        else
        {
            return R{ std::move(Base::m_data).template Get<ErrorType>() };
        }
    }

    template<typename I, typename F, std::enable_if_t<
        std::is_integral<I>::value&&
        MatchesSignature<F, Result<ValueType, E>, I>::value, int> = 0>
    static Result<std::vector<ValueType>, E> Range(I count, F fn)
    {
        using R = Result<std::vector<ValueType>, E>;
        std::vector<ValueType> result{};
        for (I i{ 0 }; i < count; i++)
        {
            auto item = fn(i);
            if (item)
            {
                result.push_back(std::move(item).Get());
            }
            else
            {
                return R{ std::move(item).Error() };
            }
        }
        return R{ std::move(result) };
    }
};

template<typename E>
class Result<E>: private Details::ResultImpl<Result, E>
{
    using Base = typename Details::GetBase<Result>::Type;
    static_assert(!std::is_void<E>::value, "Error type can't be void");
public:
    explicit Result(): Base{}
    {}

    explicit Result(E error): Base{ std::forward<E>(error) }
    {}

    Result(const Result& other) = default;
    Result(Result&& other) = default;
    Result& operator=(const Result& rhs) = default;
    Result& operator=(Result&& rhs) = default;
    ~Result() = default;

    using Base::operator bool;
    using Base::Error;
    using Base::AndThen;
    using Base::OrElse;
    using Base::MapErr;
    using Base::Unwrap;

    template<typename I, typename F, std::enable_if_t<
        std::is_integral<I>::value &&
        MatchesSignature<F, Result<E>, I>::value, int> = 0>
    static Result<E> Range(I count, F fn)
    {
        using R = Result<E>;
        for (I i{ 0 }; i < count; i++)
        {
            auto item = fn(i);
            if (!item)
            {
                return R{ std::move(item).Error() };
            }
        }
        return R{};
    }
};

template<typename T, typename E>
Result<T, E> ToResult(Maybe<T> maybe, E&& error)
{
    if (maybe)
    {
        return Result<T, E>{ std::move(maybe).Get() };
    }
    return Result<T, E>{ std::forward<E>(error) };
}

} } } }
