//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "test_utils.h"
#include "substrings_matcher.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

TEST_CASE("SubstringsMatcher constructor", "[cxx][core][common][substrings_matcher]")
{
    SECTION("default")
    {
        SubstringsMatcher matcher;
        REQUIRE(0 == matcher.MaxMatchLen());
    }

    SECTION("vector")
    {
        {
            std::vector<std::string> subs;
            SubstringsMatcher matcher;
            REQUIRE(0 == matcher.MaxMatchLen());
        }
        {
            std::vector<std::string> subs{ "First" };
            SubstringsMatcher matcher(subs);
            REQUIRE(5 == matcher.MaxMatchLen());
        }
        {
            std::vector<std::string> subs{ "First", "0123456789"};
            SubstringsMatcher matcher(subs);
            REQUIRE(10 == matcher.MaxMatchLen());
        }
    }

    SECTION("initializer list")
    {
        {
            SubstringsMatcher matcher({});
            REQUIRE(0 == matcher.MaxMatchLen());
        }
        {
            SubstringsMatcher matcher({ "First" });
            REQUIRE(5 == matcher.MaxMatchLen());
        }
        {
            SubstringsMatcher matcher({ "First", "0123456789" });
            REQUIRE(10 == matcher.MaxMatchLen());
        }
    }
}

TEST_CASE("SubstringsMatcher find", "[cxx][core][common][substrings_matcher]")
{
    SECTION("no substrings")
    {
        std::string text("This is a short test message");
        std::string found;
        {
            SubstringsMatcher matcher;
            REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find(text, &found));
            REQUIRE("" == found);
        }
        {
            SubstringsMatcher matcher({});
            REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find(text));
        }
        {
            SubstringsMatcher matcher(std::vector<std::string>{});
            REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find(text, 12, &found));
            REQUIRE("" == found);
        }
    }

    SECTION("empty text")
    {
        std::string found;
        SubstringsMatcher matcher({ "is", "a" "test message" });

        REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find(""));
        REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find("", &found));
        REQUIRE("" == found);
        REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find("", 1, 10, &found));
        REQUIRE("" == found);
    }

    SECTION("at start")
    {
        std::string text("This is a short test message");

        SubstringsMatcher matcher({ "is", "This", "message" });
        REQUIRE(0 == matcher.Find(text));
        REQUIRE(text.find("This") == 0);
    }

    SECTION("middle")
    {
        std::string text("This is a short test message");

        SubstringsMatcher matcher({ "ax", "short", "massage" });
        size_t index = matcher.Find(text);
        REQUIRE(10 == index);
        REQUIRE(text.find("short") == index);
    }

    SECTION("at end")
    {
        std::string text("This is a short test message");

        SubstringsMatcher matcher({ "message", "message-like"});
        size_t index = matcher.Find(text);
        REQUIRE(21 == index);
        REQUIRE(text.find("message") == index);
    }

    SECTION("count & offset & found combinations")
    {
        std::string text("This isn't a short test message");

        size_t index;
        std::string found;
        SubstringsMatcher matcher({ "This", "is", "isn't", "short", "message" });

        index = matcher.Find(text);
        REQUIRE(0 == index);

        index = matcher.Find(text, &found);
        REQUIRE(0 == index);
        REQUIRE("This" == found);

        index = matcher.Find(text, 3);
        REQUIRE(5 == index);

        index = matcher.Find(text, 3, &found);
        REQUIRE(5 == index);
        REQUIRE("isn't" == found);

        index = matcher.Find(text, 5, 3);
        REQUIRE(5 == index);

        index = matcher.Find(text, 5, 3, &found);
        REQUIRE(5 == index);
        REQUIRE("is" == found); // we don't see "isn't" since we stop searching early

        index = matcher.Find(text, 5, 5, &found);
        REQUIRE(5 == index);
        REQUIRE("isn't" == found);
    }

    SECTION("out of bounds")
    {
        std::string text("This is a short test message");
        std::string found;
        SubstringsMatcher matcher({ "is", "a" "test message" });

        REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find(text, 100));
        size_t index = matcher.Find(text, 0, 100, &found);
        REQUIRE(2 == index);
        REQUIRE("is" == found);
        REQUIRE(text.find(found) == index);
        REQUIRE(SubstringsMatcher::NO_MATCH == matcher.Find(text, 20, 10, &found));
    }

    SECTION("longest match")
    {
        std::string text("There are three hundred eighty two birds");

        std::string found;
        SubstringsMatcher matcher(
            {
                "two",
                "three",
                "three hundred",
                "three hundred eighty",
            });

        size_t index = matcher.Find(text, &found);

        REQUIRE(10 == index);
        REQUIRE("three hundred eighty" == found);
        REQUIRE(text.find(found) == index);
    }

    SECTION("next best match")
    {
        std::string text("There are three hundred eighty two birds");

        std::string found;
        SubstringsMatcher matcher(
            {
                "two",
                "three",
                "three hundred",
                "three hundred eightY",
            });

        size_t index = matcher.Find(text, &found);

        REQUIRE(10 == index);
        REQUIRE("three hundred" == found);
        REQUIRE(text.find(found) == index);
    }

    SECTION("next best match may be shortest")
    {
        std::string text("There are three hundred eighty two birds");

        std::string found;
        SubstringsMatcher matcher(
            {
                "three",
                "three hundreds",
                "three hundred eightY",
            });

        size_t index = matcher.Find(text, &found);

        REQUIRE(10 == index);
        REQUIRE("three" == found);
        REQUIRE(text.find(found) == index);
    }

    SECTION("more specific before less specific")
    {
        std::string text("There are three hundred eighty two birds");
        std::string found;
        SubstringsMatcher matcher(
            {
                "three hundred eightY",
                "three hundred",
                "three",
            });

        size_t index = matcher.Find(text, &found);

        REQUIRE(10 == index);
        REQUIRE("three hundred" == found);
        REQUIRE(text.find(found) == index);
    }
}

