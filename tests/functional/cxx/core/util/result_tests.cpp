//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <string>

#include "test_utils.h"

#include "util/result.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

struct MoveCopyTest
{
    MoveCopyTest(): m_wasCopied{ false }, m_wasMoved{ false }
    {}

    MoveCopyTest(const MoveCopyTest& other): m_wasCopied{ true }, m_wasMoved{ other.m_wasMoved }
    {}

    MoveCopyTest(MoveCopyTest&& other): m_wasCopied{ other.m_wasCopied }, m_wasMoved{ true }
    {}

    MoveCopyTest& operator=(const MoveCopyTest& rhs)
    {
        m_wasCopied = true;
        m_wasMoved = rhs.m_wasMoved;
        return *this;
    }

    MoveCopyTest& operator=(MoveCopyTest&& rhs)
    {
        m_wasCopied = rhs.m_wasCopied;
        m_wasMoved = true;
        return *this;
    }

    bool WasCopied() const
    {
        return m_wasCopied;
    }

    bool WasMoved() const
    {
        return m_wasMoved;
    }

    void Reset() const
    {
        m_wasCopied = false;
        m_wasMoved = false;
    }
protected:
    mutable bool m_wasCopied{};
    mutable bool m_wasMoved{};
};

struct ErrorType: public MoveCopyTest
{
    ErrorType(std::string message): MoveCopyTest{}, Message{ message }
    {}

    ErrorType(const ErrorType& other): MoveCopyTest{ other }, Message{ other.Message }
    {}

    ErrorType(ErrorType&& other): MoveCopyTest{ std::move(other) }, Message{ std::move(other.Message) }
    {}

    ErrorType& operator=(const ErrorType& rhs)
    {
        m_wasCopied = true;
        Message = rhs.Message;
        return *this;
    }

    ErrorType& operator=(ErrorType&& rhs)
    {
        m_wasMoved = true;
        Message = std::move(rhs.Message);
        return *this;
    }

    std::string Message;

    bool operator==(const ErrorType& other) const
    {
        return Message == other.Message;
    }
};

struct DestructorTest
{
public:
    explicit DestructorTest(bool& flag): m_flag{ flag }
    {}

    DestructorTest(const DestructorTest& other): m_flag{ other.m_flag }
    {}

    DestructorTest(DestructorTest&& other): m_flag{ other.m_flag }
    {}

    DestructorTest& operator=(const DestructorTest& other)
    {
        m_flag = other.m_flag;
        return *this;
    }

    DestructorTest& operator=(DestructorTest&& other)
    {
        m_flag = other.m_flag;
        return *this;
    }

    ~DestructorTest()
    {
        m_flag.get() = true;
    }

    void Reset() const
    {
        m_flag.get() = false;
    }
private:
    std::reference_wrapper<bool> m_flag;
};

using StringResult = Result<std::string, ErrorType>;
using IntResult = Result<int, ErrorType>;

template<typename T, typename E> struct TestValues {};

#define DEFINE_TEST_VALUES(ResultType, TVal, EVal) \
    template<> struct TestValues<typename ResultType::ValueType, typename ResultType::ErrorType>{ static std::tuple<typename ResultType::ValueType, typename ResultType::ErrorType> Values; }; \
    std::tuple<typename ResultType::ValueType, typename ResultType::ErrorType> TestValues<typename ResultType::ValueType, typename ResultType::ErrorType>::Values{ TVal, EVal };

DEFINE_TEST_VALUES(StringResult, "success", ErrorType{ "error"})
DEFINE_TEST_VALUES(IntResult, 3, ErrorType{ "message" })

SPXTEST_TEMPLATE_CASE_BEGIN("Result Basic Operations (Result<T, E>)", "[util][result]", StringResult, IntResult)
{
    using V = typename TestType::ValueType;
    using E = typename TestType::ErrorType;
    using R = Result<V, E>;

    auto& values = TestValues<V, E>::Values;

    SPXTEST_SECTION("Constructor & Get (success)")
    {
        auto value = std::get<V>(values);
        auto result = R{ value };
        SPXTEST_REQUIRE(result);
        SPXTEST_REQUIRE(result.Get() == value);
    }
    SPXTEST_SECTION("Constructor & Error (error)")
    {
        auto error = std::get<E>(values);
        auto result = R{ error };
        SPXTEST_REQUIRE_FALSE(result);
        SPXTEST_REQUIRE(result.Error() == error);
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Destructor (Result<T, E>)", "[util][result]")
{
    SPXTEST_SECTION("With value")
    {
        using R = Result<DestructorTest, int>;
        bool wasDestroyed{ false };

        SPXTEST_SECTION("Result destructor")
        {
            {
                R result{ DestructorTest{ wasDestroyed } };
                result.Get().Reset();
            }
            SPXTEST_REQUIRE(wasDestroyed);
        }

        SPXTEST_SECTION("Copy assignment")
        {
            R result{ DestructorTest{ wasDestroyed } };
            result.Get().Reset();
            R other{ 3 };
            result = other;
            SPXTEST_REQUIRE(wasDestroyed);
        }

        SPXTEST_SECTION("Move assignment")
        {
            R result{ DestructorTest{ wasDestroyed } };
            result.Get().Reset();
            result = R{ 3 };
            SPXTEST_REQUIRE(wasDestroyed);
        }
    }

    SPXTEST_SECTION("With error")
    {
        using R = Result<int, DestructorTest>;
        bool wasDestroyed{ false };

        SPXTEST_SECTION("Result destructor")
        {
            {
                R result{ DestructorTest{ wasDestroyed } };
                result.Error().Reset();
            }
            SPXTEST_REQUIRE(wasDestroyed);
        }

        SPXTEST_SECTION("Copy assignment")
        {
            R result{ DestructorTest{ wasDestroyed } };
            result.Error().Reset();
            R other{ 3 };
            result = other;
            SPXTEST_REQUIRE(wasDestroyed);
        }

        SPXTEST_SECTION("Move assignment")
        {
            R result{ DestructorTest{ wasDestroyed } };
            result.Error().Reset();
            result = R{ 3 };
            SPXTEST_REQUIRE(wasDestroyed);
        }
    }

}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Destructor (Result<E>)", "[util][result]")
{
    bool wasDestroyed{ false };
    {
        Result<int, DestructorTest> result{ DestructorTest{ wasDestroyed} };
    }
    SPXTEST_REQUIRE(wasDestroyed);
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Basic Operations (Result<E>)", "[util][result]")
{
    using R = Result<ErrorType>;

    SPXTEST_SECTION("Constructor & Get (success)")
    {
        auto result = R{};
        SPXTEST_REQUIRE(result);
    }
    SPXTEST_SECTION("Constructor & Error (error)")
    {
        auto error = ErrorType{ "message" };
        auto result = R{ error };
        SPXTEST_REQUIRE_FALSE(result);
        SPXTEST_REQUIRE(result.Error() == error);
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Copy & Move Value (Result<T, E>)", "[util][result]")
{
    using R = Result<MoveCopyTest, ErrorType>;
    R result{ MoveCopyTest{} };
    /* Reset to not count the move on construction */
    result.Get().Reset();
    SPXTEST_SECTION("Copy Result")
    {
        auto otherResult = result;
        SPXTEST_REQUIRE(otherResult);
        auto& value = otherResult.Get();
        SPXTEST_REQUIRE(value.WasCopied());
        SPXTEST_REQUIRE_FALSE(value.WasMoved());
    }
    SPXTEST_SECTION("Copy Value")
    {
        auto value = result.Get();
        SPXTEST_REQUIRE(value.WasCopied());
        SPXTEST_REQUIRE_FALSE(value.WasMoved());
    }
    SPXTEST_SECTION("Move Result")
    {
        auto otherResult = std::move(result);
        SPXTEST_REQUIRE(otherResult);
        auto& value = otherResult.Get();
        SPXTEST_REQUIRE(value.WasMoved());
        SPXTEST_REQUIRE_FALSE(value.WasCopied());
    }
    SPXTEST_SECTION("Move Value")
    {
        auto value = std::move(result).Get();
        SPXTEST_REQUIRE(value.WasMoved());
        SPXTEST_REQUIRE_FALSE(value.WasCopied());
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Copy & Move Error (Result<T, E>)", "[util][result]")
{
    using R = Result<int, MoveCopyTest>;
    R result{ MoveCopyTest{} };
    /* Reset to not count the move on construction */
    result.Error().Reset();
    SPXTEST_SECTION("Copy Result")
    {
        auto otherResult = result;
        SPXTEST_REQUIRE_FALSE(otherResult);
        auto& error = otherResult.Error();
        SPXTEST_REQUIRE(error.WasCopied());
        SPXTEST_REQUIRE_FALSE(error.WasMoved());
    }
    SPXTEST_SECTION("Copy Error")
    {
        auto error = result.Error();
        SPXTEST_REQUIRE(error.WasCopied());
        SPXTEST_REQUIRE_FALSE(error.WasMoved());
    }
    SPXTEST_SECTION("Move Result")
    {
        auto otherResult = std::move(result);
        SPXTEST_REQUIRE_FALSE(otherResult);
        auto& error = otherResult.Error();
        SPXTEST_REQUIRE(error.WasMoved());
        SPXTEST_REQUIRE_FALSE(error.WasCopied());
    }
    SPXTEST_SECTION("Move Error")
    {
        auto error = std::move(result).Error();
        SPXTEST_REQUIRE(error.WasMoved());
        SPXTEST_REQUIRE_FALSE(error.WasCopied());
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Copy & Move Error (Result<E>)", "[util][result]")
{
    using R = Result<MoveCopyTest>;
    R result{ MoveCopyTest{} };
    /* Reset to not count the move on construction */
    result.Error().Reset();
    SPXTEST_SECTION("Copy Result")
    {
        auto otherResult = result;
        SPXTEST_REQUIRE_FALSE(otherResult);
        auto& error = otherResult.Error();
        SPXTEST_REQUIRE(error.WasCopied());
        SPXTEST_REQUIRE_FALSE(error.WasMoved());
    }
    SPXTEST_SECTION("Copy Error")
    {
        auto error = result.Error();
        SPXTEST_REQUIRE(error.WasCopied());
        SPXTEST_REQUIRE_FALSE(error.WasMoved());
    }
    SPXTEST_SECTION("Move Result")
    {
        auto otherResult = std::move(result);
        SPXTEST_REQUIRE_FALSE(otherResult);
        auto& error = otherResult.Error();
        SPXTEST_REQUIRE(error.WasMoved());
        SPXTEST_REQUIRE_FALSE(error.WasCopied());
    }
    SPXTEST_SECTION("Move Error")
    {
        auto error = std::move(result).Error();
        SPXTEST_REQUIRE(error.WasMoved());
        SPXTEST_REQUIRE_FALSE(error.WasCopied());
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result AndThen (Result<T, E>)", "[util][result]")
{
    SPXTEST_SECTION("With value")
    {
        using R = Result<MoveCopyTest, ErrorType>;
        R result{ MoveCopyTest{} };
        SPXTEST_SECTION("Into value")
        {
            bool wasCopied{ false };
            bool wasMoved{ false };
            auto newResult = result.AndThen<int>([&](auto value)
            {
                wasCopied = value.WasCopied();
                wasMoved = value.WasMoved();
                return Result<int, ErrorType>{ 3 };
            });
            SPXTEST_REQUIRE(newResult);
            SPXTEST_REQUIRE(newResult.Get() == 3);
            SPXTEST_REQUIRE(wasMoved);
            SPXTEST_REQUIRE_FALSE(wasCopied);
        }
        SPXTEST_SECTION("Into empty result")
        {
            bool wasCopied{ false };
            bool wasMoved{ false };
            auto newResult = result.AndThen([&](auto value)
            {
                wasCopied = value.WasCopied();
                wasMoved = value.WasMoved();
                return Result<ErrorType>{};
            });
            SPXTEST_REQUIRE(newResult);
            SPXTEST_REQUIRE(wasMoved);
            SPXTEST_REQUIRE_FALSE(wasCopied);
        }
        SPXTEST_SECTION("Into error")
        {
            ErrorType error{ "message" };
            bool wasCopied{ false };
            bool wasMoved{ false };
            auto newResult = result.AndThen<int>([&](MoveCopyTest value)
            {
                wasCopied = value.WasCopied();
                wasMoved = value.WasMoved();
                return Result<int, ErrorType>{ error };
            });
            SPXTEST_REQUIRE_FALSE(newResult);
            SPXTEST_REQUIRE(newResult.Error() == error);
            SPXTEST_REQUIRE(wasMoved);
            SPXTEST_REQUIRE_FALSE(wasCopied);
        }
    }
    SPXTEST_SECTION("With error")
    {
        using R = Result<int, MoveCopyTest>;
        R result{ MoveCopyTest{} };
        auto newResult = result.AndThen<int>([&](auto value)
        {
            (void)value;
            return Result<int, MoveCopyTest>{ 3 };
        });
        SPXTEST_REQUIRE_FALSE(newResult);
        auto& error = newResult.Error();
        SPXTEST_REQUIRE(error.WasMoved());
        SPXTEST_REQUIRE_FALSE(error.WasCopied());

    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result OrElse (Result<T, E>)", "[util][result]")
{
    SPXTEST_SECTION("With value")
    {
        using R = Result<MoveCopyTest, ErrorType>;
        R result{ MoveCopyTest{} };
        auto newResult = result.OrElse<ErrorType>([&](auto value)
        {
            (void)value;
            return Result<MoveCopyTest, ErrorType>{ MoveCopyTest{} };
        });
        SPXTEST_REQUIRE(newResult);
        auto& value = newResult.Get();
        SPXTEST_REQUIRE(value.WasMoved());
        SPXTEST_REQUIRE_FALSE(value.WasCopied());
    }
    SPXTEST_SECTION("With error")
    {
        using R = Result<MoveCopyTest, ErrorType>;
        R result{ ErrorType{ "a message " } };
        bool wasCopied{ false };
        bool wasMoved{ false };
        SPXTEST_SECTION("Into value")
        {
            auto newResult = result.OrElse<int>([&](auto error)
            {
                wasCopied = error.WasCopied();
                wasMoved = error.WasMoved();
                return Result<MoveCopyTest, int>{ MoveCopyTest{} };
            });
            SPXTEST_REQUIRE(newResult);
            SPXTEST_REQUIRE(wasMoved);
            SPXTEST_REQUIRE_FALSE(wasCopied);
        }
        SPXTEST_SECTION("Into error")
        {
            auto newResult = result.OrElse<int>([&](auto error)
            {
                wasCopied = error.WasCopied();
                wasMoved = error.WasMoved();
                return Result<MoveCopyTest, int>{ 3 };
            });
            SPXTEST_REQUIRE_FALSE(newResult);
            SPXTEST_REQUIRE(wasMoved);
            SPXTEST_REQUIRE_FALSE(wasCopied);
        }
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Map (Result<T, E>)", "[util][result]")
{
    using R = Result<MoveCopyTest, ErrorType>;

    SPXTEST_SECTION("With value")
    {
        R result{ MoveCopyTest{} };
        bool called{ false };
        bool wasCopied{ false };
        bool wasMoved{ false };
        auto newResult = result.Map<double>([&](auto value)
        {
            called = true;
            wasCopied = value.WasCopied();
            wasMoved = value.WasMoved();
            return 3.14;
        });
        SPXTEST_REQUIRE(newResult);
        SPXTEST_REQUIRE(newResult.Get() == 3.14);
        SPXTEST_REQUIRE(called);
        SPXTEST_REQUIRE_FALSE(wasCopied);
        SPXTEST_REQUIRE(wasMoved);
    }

    SPXTEST_SECTION("With error")
    {
        R result{ ErrorType{ "message" } };
        bool called{ false };
        auto newResult = result.Map<double>([&](auto)
        {
            called = true;
            return 3.14;
        });
        SPXTEST_REQUIRE_FALSE(newResult);
        SPXTEST_REQUIRE_FALSE(called);
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result MapErr (Result<T, E>)", "[util][result]")
{
    using R = Result<MoveCopyTest, ErrorType>;

    SPXTEST_SECTION("With value")
    {
        R result{ MoveCopyTest{} };
        bool called{ false };
        auto newResult = result.MapErr<double>([&](auto)
        {
            called = true;
            return 3.14;
        });
        SPXTEST_REQUIRE(newResult);
        SPXTEST_REQUIRE_FALSE(called);
    }

    SPXTEST_SECTION("With error")
    {
        R result{ ErrorType{ "message" } };
        bool called{ false };
        bool wasCopied{ false };
        bool wasMoved{ false };
        auto newResult = result.MapErr<double>([&](auto err)
        {
            called = true;
            wasCopied = err.WasCopied();
            wasMoved = err.WasMoved();
            return 3.14;
        });
        SPXTEST_REQUIRE_FALSE(newResult);
        SPXTEST_REQUIRE(newResult.Error() == 3.14);
        SPXTEST_REQUIRE(called);
        SPXTEST_REQUIRE_FALSE(wasCopied);
        SPXTEST_REQUIRE(wasMoved);
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Unwrap (Result<T, E>)", "[util][result]")
{
    using R = Result<MoveCopyTest, std::string>;
    SPXTEST_SECTION("With value")
    {
        R result{ MoveCopyTest{} };
        bool wasCopied{ false };
        bool wasMoved{ false };
        SPXTEST_REQUIRE_NOTHROW([&]()
        {
            auto value = result.Unwrap<std::string>([](auto message)
            {
                return message;
            });
            wasCopied = value.WasCopied();
            wasMoved = value.WasMoved();
        }());
        SPXTEST_REQUIRE_FALSE(wasCopied);
        SPXTEST_REQUIRE(wasMoved);
    }
    SPXTEST_SECTION("With error")
    {
        constexpr const char * cookie{ "largecookie" };
        R result{ cookie };
        SPXTEST_REQUIRE_THROWS_WITH_CONTAINS(result.Unwrap<std::string>([](auto message)
        {
            return message;
        }), cookie);
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result AndThen (Result<E>)", "[util][result]")
{
    SPXTEST_SECTION("With value")
    {
        using R = Result<ErrorType>;
        R result{};
        bool called{ false };
        SPXTEST_SECTION("Into value")
        {
            auto newResult = result.AndThen<int>([&]()
            {
                called = true;
                return Result<int, ErrorType>{ 3 };
            });
            SPXTEST_REQUIRE(newResult);
            SPXTEST_REQUIRE(newResult.Get() == 3);
        }
        SPXTEST_SECTION("Into empty result")
        {
            auto newResult = result.AndThen([&]()
            {
                called = true;
                return Result<ErrorType>{};
            });
            SPXTEST_REQUIRE(newResult);
        }
        SPXTEST_SECTION("Into error")
        {
            ErrorType error{ "message" };
            auto newResult = result.AndThen<int>([&]()
            {
                called = true;
                return Result<int, ErrorType>{ error };
            });
            SPXTEST_REQUIRE_FALSE(newResult);
            SPXTEST_REQUIRE(newResult.Error() == error);
        }
        SPXTEST_REQUIRE(called);
    }
    SPXTEST_SECTION("With error")
    {
        using R = Result<ErrorType>;
        R result{ ErrorType{ "message" } };
        bool called{ false };
        auto newResult = result.AndThen<int>([&]()
        {
            called = true;
            return Result<int, ErrorType>{ 3 };
        });
        SPXTEST_REQUIRE_FALSE(called);
        SPXTEST_REQUIRE_FALSE(newResult);
        auto& error = newResult.Error();
        SPXTEST_REQUIRE(error.WasMoved());
        SPXTEST_REQUIRE_FALSE(error.WasCopied());
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result OrElse (Result<E>)", "[util][result]")
{
    SPXTEST_SECTION("With value")
    {
        using R = Result<ErrorType>;
        R result{};
        bool called{ false };
        auto newResult = result.OrElse<int>([&](auto)
        {
            return Result<int>{ 3 };
        });
        SPXTEST_REQUIRE_FALSE(called);
        SPXTEST_REQUIRE(newResult);
    }
    SPXTEST_SECTION("With error")
    {
        using R = Result<ErrorType>;
        R result{ ErrorType{ "a message " } };
        bool called{ false };
        bool wasCopied{ false };
        bool wasMoved{ false };
        SPXTEST_SECTION("Into value")
        {
            auto newResult = result.OrElse<int>([&](auto error)
            {
                called = true;
                wasCopied = error.WasCopied();
                wasMoved = error.WasMoved();
                return Result<int>{};
            });
            SPXTEST_REQUIRE(called);
            SPXTEST_REQUIRE(newResult);
            SPXTEST_REQUIRE(wasMoved);
            SPXTEST_REQUIRE_FALSE(wasCopied);
        }
        SPXTEST_SECTION("Into error")
        {
            auto newResult = result.OrElse<int>([&](auto error)
            {
                called = true;
                wasCopied = error.WasCopied();
                wasMoved = error.WasMoved();
                return Result<int>{ 3 };
            });
            SPXTEST_REQUIRE(called);
            SPXTEST_REQUIRE_FALSE(newResult);
            SPXTEST_REQUIRE(wasMoved);
            SPXTEST_REQUIRE_FALSE(wasCopied);
        }
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result MapErr (Result<E>)", "[util][result]")
{
    using R = Result<ErrorType>;

    SPXTEST_SECTION("With value")
    {
        R result{};
        bool called{ false };
        auto newResult = result.MapErr<double>([&](auto)
        {
            called = true;
            return 3.14;
        });
        SPXTEST_REQUIRE(newResult);
        SPXTEST_REQUIRE_FALSE(called);
    }

    SPXTEST_SECTION("With error")
    {
        R result{ ErrorType{ "message" } };
        bool called{ false };
        bool wasCopied{ false };
        bool wasMoved{ false };
        auto newResult = result.MapErr<double>([&](auto err)
        {
            called = true;
            wasCopied = err.WasCopied();
            wasMoved = err.WasMoved();
            return 3.14;
        });
        SPXTEST_REQUIRE_FALSE(newResult);
        SPXTEST_REQUIRE(newResult.Error() == 3.14);
        SPXTEST_REQUIRE(called);
        SPXTEST_REQUIRE_FALSE(wasCopied);
        SPXTEST_REQUIRE(wasMoved);
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Range", "[util][result]")
{
    constexpr size_t count{ 10 };

    SPXTEST_SECTION("All succeed")
    {
        auto result = Result<std::string, ErrorType>::Range(count, [](auto index)
        {
            std::string s{ "number " };
            s += static_cast<char>('0' + index);
            return Result<std::string, ErrorType>{ std::move(s) };
        });
        SPXTEST_REQUIRE(result);
        auto values = std::move(result).Get();
        SPXTEST_REQUIRE(count == values.size());
        SPXTEST_REQUIRE(values[0] == "number 0");
        SPXTEST_REQUIRE(values[9] == "number 9");
    }

    SPXTEST_SECTION("One fails")
    {
        auto result = Result<std::string, ErrorType>::Range(count, [&](auto index)
        {
            std::string s{ "number " };
            s += static_cast<char>('0' + index);
            if (index == (count / 2))
            {
                return Result<std::string, ErrorType>{ ErrorType{ "An error" } };
            }
            return Result<std::string, ErrorType>{ std::move(s) };
        });
        SPXTEST_REQUIRE_FALSE(result);
        SPXTEST_REQUIRE(result.Error() == ErrorType{ "An error" });
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result ForEach", "[util][result]")
{
    constexpr size_t count{ 10 };

    SPXTEST_SECTION("With value")
    {
        auto result = Result<std::string, ErrorType>::Range(count, [](auto index)
        {
            std::string s{ "number " };
            s += static_cast<char>('0' + index);
            return Result<std::string, ErrorType>{ std::move(s) };
        });

        SPXTEST_SECTION("All succeed")
        {
            auto newResult = result.ForEach<size_t>([&](auto value)
            {
                return Result<size_t, ErrorType>{ value.size() };
            });
            SPXTEST_REQUIRE(newResult);
            auto values = std::move(newResult).Get();
            SPXTEST_REQUIRE(count == values.size());
            SPXTEST_REQUIRE(values[0] == 8);
            SPXTEST_REQUIRE(values[9] == 8);
        }

        SPXTEST_SECTION("One fails")
        {
            size_t i{ 0 };
            auto newResult = result.ForEach<size_t>([&](auto value)
            {
                if (++i > (count / 2))
                {
                    return Result<size_t, ErrorType>{ ErrorType{ "An error" } };
                }
                return Result<size_t, ErrorType>{ value.size() };
            });
            SPXTEST_REQUIRE_FALSE(newResult);
            SPXTEST_REQUIRE(newResult.Error() == ErrorType{ "An error" });
        }
    }

    SPXTEST_SECTION("With error")
    {
        int i{ 0 };
        auto result = Result<std::string, ErrorType>::Range(count, [](auto)
        {
            return Result<std::string, ErrorType>{ ErrorType{ "An error" } };
        }).ForEach<int>([&](auto)
        {
            return Result<int, ErrorType>{ i++ };
        });
        SPXTEST_REQUIRE_FALSE(result);
        SPXTEST_REQUIRE(result.Error() == ErrorType{ "An error" });

    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result FindFirst", "[util][result]")
{
    constexpr size_t count{ 10 };

    SPXTEST_SECTION("With value")
    {
        auto result = Result<size_t, ErrorType>::Range(count, [](auto index)
        {
            return Result<size_t, ErrorType>{ index };
        });

        SPXTEST_SECTION("Found")
        {
            auto newResult = result.FindFirst([&](auto value)
            {
                return value == 3;
            });
            SPXTEST_REQUIRE(newResult);
            auto maybeValue = std::move(newResult).Get();
            SPXTEST_REQUIRE(maybeValue);
            SPXTEST_REQUIRE(maybeValue.Get() == 3);
        }

        SPXTEST_SECTION("Not Found")
        {
            auto newResult = result.FindFirst([&](auto value)
            {
                return value == 42;
            });
            SPXTEST_REQUIRE(newResult);
            auto maybeValue = std::move(newResult).Get();
            SPXTEST_REQUIRE_FALSE(maybeValue);
        }
    }

    SPXTEST_SECTION("With error")
    {
        auto result = Result<size_t, ErrorType>::Range(count, [](auto)
        {
            return Result<size_t, ErrorType>{ ErrorType{ "An error" } };
        }).FindFirst([](auto value)
        {
            return value == 0;
        });
        SPXTEST_REQUIRE_FALSE(result);
        SPXTEST_REQUIRE(result.Error() == ErrorType{ "An error" });
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result FindIndexOfFirst", "[util][result]")
{
    constexpr size_t count{ 10 };

    SPXTEST_SECTION("With value")
    {
        auto result = Result<std::vector<int>, ErrorType>{ std::vector<int>{ 11, 22, 33, 44, 55, 66, 77, 88, 99 } };

        SPXTEST_SECTION("Found")
        {
            auto newResult = result.FindIndexOfFirst([&](auto value)
            {
                return value == 55;
            });
            SPXTEST_REQUIRE(newResult);
            auto maybeValue = std::move(newResult).Get();
            SPXTEST_REQUIRE(maybeValue);
            SPXTEST_REQUIRE(maybeValue.Get() == 4);
        }

        SPXTEST_SECTION("Not Found")
        {
            auto newResult = result.FindFirst([&](auto value)
            {
                return value == 42;
            });
            SPXTEST_REQUIRE(newResult);
            auto maybeValue = std::move(newResult).Get();
            SPXTEST_REQUIRE_FALSE(maybeValue);
        }
    }

    SPXTEST_SECTION("With error")
    {
        auto result = Result<size_t, ErrorType>::Range(count, [](auto)
        {
            return Result<size_t, ErrorType>{ ErrorType{ "An error" } };
        }).FindFirst([](auto value)
        {
            return value == 0;
        });
        SPXTEST_REQUIRE_FALSE(result);
        SPXTEST_REQUIRE(result.Error() == ErrorType{ "An error" });
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Sorted", "[util][result]")
{
    SPXTEST_SECTION("With numbers")
    {
        std::vector<int> numberVector{ 7, 3, 6, 4, 1, 5, 8, 2 };
        Result<std::vector<int>, int> result{ numberVector };
        auto sorted = result.Sorted([](auto a, auto b)
        {
            return a < b;
        });
        SPXTEST_REQUIRE(sorted);
        auto sortedVector = std::move(sorted).Get();
        SPXTEST_REQUIRE(sortedVector.size() == numberVector.size());
        SPXTEST_REQUIRE(sortedVector[0] == 1);
        SPXTEST_REQUIRE(sortedVector[1] == 2);
        SPXTEST_REQUIRE(sortedVector[2] == 3);
        SPXTEST_REQUIRE(sortedVector[3] == 4);
        SPXTEST_REQUIRE(sortedVector[4] == 5);
        SPXTEST_REQUIRE(sortedVector[5] == 6);
        SPXTEST_REQUIRE(sortedVector[6] == 7);
        SPXTEST_REQUIRE(sortedVector[7] == 8);
    }

    SPXTEST_SECTION("With structs")
    {
        struct A
        {
            std::string Value;
        };
        std::vector<A> structVector{ { "the" }, { "quick" }, { "brown" }, { "fox" }, { "jumps" }, { "over" }, { "the" }, { "lazy" }, { "dog" } };
        Result<std::vector<A>, int> result{ structVector };
        auto sorted = result.Sorted([](const auto& a, const auto& b)
        {
            return a.Value < b.Value;
        });
        SPXTEST_REQUIRE(sorted);
        auto sortedVector = std::move(sorted).Get();
        SPXTEST_REQUIRE(sortedVector.size() == structVector.size());
        SPXTEST_REQUIRE(sortedVector[0].Value == "brown");
        SPXTEST_REQUIRE(sortedVector[1].Value == "dog");
        SPXTEST_REQUIRE(sortedVector[2].Value == "fox");
        SPXTEST_REQUIRE(sortedVector[3].Value == "jumps");
        SPXTEST_REQUIRE(sortedVector[4].Value == "lazy");
        SPXTEST_REQUIRE(sortedVector[5].Value == "over");
        SPXTEST_REQUIRE(sortedVector[6].Value == "quick");
        SPXTEST_REQUIRE(sortedVector[7].Value == "the");
        SPXTEST_REQUIRE(sortedVector[8].Value == "the");
    }
}
SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Result Also", "[util][result]")
{
    constexpr auto stringValue{ "first result" };
    constexpr auto doubleValue{ 3.14 };
    constexpr auto intValue{ 42 };

    constexpr auto firstError{ "first error" };
    constexpr auto secondError{ "second error" };
    constexpr auto thirdError{ "third error" };

    SPXTEST_SECTION("Initial is a success")
    {
        StringResult initial{ stringValue };

        SPXTEST_SECTION("Also suceeeds")
        {
            auto result = initial.Also<double>([&]()
            {
                return Result<double, ErrorType>{ doubleValue };
            });

            SECTION("Check result")
            {
                SPXTEST_REQUIRE(result);
                const auto& value = result.Get();
                const auto& retrievedString = std::get<std::string>(value);
                const auto& retrievedDouble = std::get<double>(value);
                SPXTEST_REQUIRE(stringValue == retrievedString);
                SPXTEST_REQUIRE(doubleValue == retrievedDouble);
            }

            SPXTEST_SECTION("Another Also succeeds")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ intValue };
                });

                SPXTEST_REQUIRE(newResult);
                const auto& value = newResult.Get();
                const auto& retrievedString = std::get<std::string>(value);
                const auto& retrievedDouble = std::get<double>(value);
                const auto& retrievedInt = std::get<int>(value);
                SPXTEST_REQUIRE(stringValue == retrievedString);
                SPXTEST_REQUIRE(doubleValue == retrievedDouble);
                SPXTEST_REQUIRE(intValue == retrievedInt);
            }

            SPXTEST_SECTION("Another Also fails")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ ErrorType{ thirdError } };
                });

                SPXTEST_REQUIRE(!newResult);
                const auto& error = newResult.Error();
                SPXTEST_REQUIRE(error.Message == thirdError);

            }
        }

        SPXTEST_SECTION("Also fails")
        {
            auto result = initial.Also<double>([&]()
            {
                return Result<double, ErrorType>{ ErrorType{ secondError } };
            });

            SECTION("Check result")
            {
                SPXTEST_REQUIRE(!result);
                const auto& error = result.Error();
                SPXTEST_REQUIRE(error.Message == secondError);
            }

            SPXTEST_SECTION("Another Also succeeds")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ intValue };
                });

                SPXTEST_REQUIRE(!newResult);
                const auto& error = newResult.Error();
                SPXTEST_REQUIRE(error.Message == secondError);
            }

            SPXTEST_SECTION("Another Also fails")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ ErrorType{ thirdError } };
                });

                SPXTEST_REQUIRE(!newResult);
                const auto& error = newResult.Error();
                SPXTEST_REQUIRE(error.Message == secondError);
            }
        }
    }

    SPXTEST_SECTION("Initial is an error")
    {
        StringResult initial{ ErrorType{ firstError } };

        SPXTEST_SECTION("Also suceeeds")
        {
            auto result = initial.Also<double>([&]()
            {
                return Result<double, ErrorType>{ doubleValue };
            });

            SECTION("Check result")
            {
                SPXTEST_REQUIRE(!result);
                const auto& error = result.Error();
                SPXTEST_REQUIRE(error.Message == firstError);
            }

            SPXTEST_SECTION("Another Also succeeds")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ intValue };
                });

                SPXTEST_REQUIRE(!newResult);
                const auto& error = newResult.Error();
                SPXTEST_REQUIRE(error.Message == firstError);
            }

            SPXTEST_SECTION("Another Also fails")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ ErrorType{ thirdError } };
                });

                SPXTEST_REQUIRE(!newResult);
                const auto& error = newResult.Error();
                SPXTEST_REQUIRE(error.Message == firstError);
            }
        }

        SPXTEST_SECTION("Also fails")
        {
            auto result = initial.Also<double>([&]()
            {
                return Result<double, ErrorType>{ ErrorType{ secondError } };
            });

            SECTION("Check result")
            {
                SPXTEST_REQUIRE(!result);
                const auto& error = result.Error();
                SPXTEST_REQUIRE(error.Message == firstError);
            }

            SPXTEST_SECTION("Another Also succeeds")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ intValue };
                });

                SPXTEST_REQUIRE(!newResult);
                const auto& error = newResult.Error();
                SPXTEST_REQUIRE(error.Message == firstError);
            }

            SPXTEST_SECTION("Another Also fails")
            {
                auto newResult = result.Also<int>([&]()
                {
                    return Result<int, ErrorType>{ ErrorType{ thirdError } };
                });

                SPXTEST_REQUIRE(!newResult);
                const auto& error = newResult.Error();
                SPXTEST_REQUIRE(error.Message == firstError);
            }
        }
    }
}
SPXTEST_CASE_END()
