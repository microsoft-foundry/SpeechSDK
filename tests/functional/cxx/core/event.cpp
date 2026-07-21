//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include "stdafx.h"

#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "util/event.h"
#include "test_utils.h"

using namespace Microsoft::CognitiveServices::Speech::Impl;

static size_t publicCount = 0;
static std::vector<std::string> publicCallbacks;

static void reset()
{
    publicCount = 0;
    publicCallbacks.clear();
}

static void HandleVoidCallback()
{
    publicCount++;
}

static void HandleStringCallback(const std::string& a)
{
    publicCount++;
    publicCallbacks.push_back(a);
}

static void HandleMixedCallback(const std::string& a, int b)
{
    publicCount++;
    publicCallbacks.push_back(a + ", " + std::to_string(b));
}

class callbacks : public std::enable_shared_from_this<callbacks>
{
public:
    void HandleCallback()
    {
        publicCount++;
        m_count++;
    }

    void HandleCallback(const std::string& s)
    {
        publicCount++;
        m_count++;
        m_stringCallbacks.push_back(s);
    }

    void HandleCallback(const std::string& a, int b)
    {
        publicCount++;
        m_count++;
        m_stringCallbacks.push_back(a + ", " + std::to_string(b));
    }

    std::vector<std::string> m_stringCallbacks;
    size_t m_count;
};


SPXTEST_CASE_BEGIN("No arg event callbacks", "[core][event][callbacks][no_args]")
{
    reset();

    std::shared_ptr<callbacks> c = std::make_shared<callbacks>();

    Event<> evt;
    evt += HandleVoidCallback;
    evt.Add(c, &callbacks::HandleCallback);

    evt();
    SPXTEST_REQUIRE(2 == publicCount);
    SPXTEST_REQUIRE(1 == c->m_count);

    evt.Clear();
    evt();
    SPXTEST_REQUIRE(2 == publicCount);
    SPXTEST_REQUIRE(1 == c->m_count);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("One arg event callbacks", "[core][event][callbacks][one_arg]")
{
    reset();

    std::shared_ptr<callbacks> c = std::make_shared<callbacks>();

    Event<const std::string&> evt;
    evt += HandleStringCallback;
    evt.Add(c, &callbacks::HandleCallback);

    std::string arg("Yes");

    evt(arg);
    SPXTEST_REQUIRE(2 == publicCount);
    SPXTEST_REQUIRE(1 == publicCallbacks.size());
    SPXTEST_REQUIRE(arg == publicCallbacks[0]);
    SPXTEST_REQUIRE(1 == c->m_count);
    SPXTEST_REQUIRE(1 == c->m_stringCallbacks.size());
    SPXTEST_REQUIRE(arg == c->m_stringCallbacks[0]);

    evt.Clear();
    evt(arg);
    SPXTEST_REQUIRE(2 == publicCount);
    SPXTEST_REQUIRE(1 == publicCallbacks.size());
    SPXTEST_REQUIRE(arg == publicCallbacks[0]);
    SPXTEST_REQUIRE(1 == c->m_count);
    SPXTEST_REQUIRE(1 == c->m_stringCallbacks.size());
    SPXTEST_REQUIRE(arg == c->m_stringCallbacks[0]);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Two arg event callbacks", "[core][event][callbacks][two_arg]")
{
    reset();

    std::shared_ptr<callbacks> c = std::make_shared<callbacks>();

    Event<const std::string&, int> evt;
    evt += HandleMixedCallback;
    evt.Add(c, &callbacks::HandleCallback);

    std::string arg0("Yes");
    int arg1 = 12;
    std::string expected0 = arg0 + ", " + std::to_string(arg1);

    evt(arg0, arg1);
    SPXTEST_REQUIRE(2 == publicCount);
    SPXTEST_REQUIRE(1 == publicCallbacks.size());
    SPXTEST_REQUIRE(expected0 == publicCallbacks[0]);
    SPXTEST_REQUIRE(1 == c->m_count);
    SPXTEST_REQUIRE(1 == c->m_stringCallbacks.size());
    SPXTEST_REQUIRE(expected0 == c->m_stringCallbacks[0]);

    arg0 = "no";
    arg1 = -24;
    std::string expected1 = arg0 + ", " + std::to_string(arg1);

    evt(arg0, arg1);
    SPXTEST_REQUIRE(4 == publicCount);
    SPXTEST_REQUIRE(2 == publicCallbacks.size());
    SPXTEST_REQUIRE(expected0 == publicCallbacks[0]);
    SPXTEST_REQUIRE(expected1 == publicCallbacks[1]);
    SPXTEST_REQUIRE(2 == c->m_count);
    SPXTEST_REQUIRE(2 == c->m_stringCallbacks.size());
    SPXTEST_REQUIRE(expected0 == c->m_stringCallbacks[0]);
    SPXTEST_REQUIRE(expected1 == c->m_stringCallbacks[1]);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Remove callbacks", "[core][event][callbacks][remove]")
{
    reset();

    auto instance = std::make_shared<callbacks>();
    std::vector<size_t> callbackIds;

    Event<const std::string&> evt;

    // add callbacks twice
    callbackIds.push_back(evt += HandleStringCallback);
    evt += HandleStringCallback;
    callbackIds.push_back(evt.Add(instance, &callbacks::HandleCallback));
    evt.Add(instance, &callbacks::HandleCallback);

    SPXTEST_REQUIRE(1 == callbackIds[0]);
    SPXTEST_REQUIRE(3 == callbackIds[1]);

    // now attempt to remove duplicates
    for (const auto& id : callbackIds)
    {
        evt.Remove(id);
    }

    // raise and validate
    std::string arg("This is a test");
    evt(arg);

    SPXTEST_REQUIRE(2 == publicCount);
    SPXTEST_REQUIRE(1 == publicCallbacks.size());
    SPXTEST_REQUIRE(arg == publicCallbacks[0]);
    SPXTEST_REQUIRE(1 == instance->m_count);
    SPXTEST_REQUIRE(1 == instance->m_stringCallbacks.size());
    SPXTEST_REQUIRE(arg == instance->m_stringCallbacks[0]);
}SPXTEST_CASE_END()


// Concurrent-event-handler safety tests for the internal Event<> type.
//
// Event<>::Raise takes a snapshot of the handler list under a mutex, then
// releases the mutex before invoking each handler from the snapshot. This
// keeps the lock window short and prevents deadlocks when a handler calls
// back into the same Event<>. However, it has a structural consequence:
// Event<>::Remove (and Event<>::Clear) mutate the canonical handler list
// but cannot retract handlers from snapshots that other threads are
// already iterating. A handler can therefore be invoked AFTER its own
// Remove call has returned.
//
// The safe way for callers to use Event<> is therefore to ensure the
// objects a handler dereferences outlive any in-flight Raise. The recommended
// patterns are either to register via Event<>::Add(shared_ptr<C>, &C::method)
// (which captures the instance by weak_ptr internally) or to capture a
// shared_ptr in the handler lambda directly.
//
// The two test cases below exercise this property.

SPXTEST_CASE_BEGIN("Event<> Remove does not drain in-flight Raise invocations",
                   "[util][event][architectural_property]")
{
    // Observation test. Asserts that a handler can be observed invoking
    // AFTER its corresponding Remove returned, demonstrating the
    // snapshot-and-release property of Raise.
    //
    // No use-after-free is produced: a thread-safe counter on stack-allocated
    // atomics outlives the producer thread (joined before the test returns).
    // The test would only fail if Event<>::Raise/Remove semantics changed
    // such that Remove synchronously drained in-flight Raises -- in which
    // case every consumer that today relies on lifetime extension would
    // need to be re-evaluated.

    Event<> event;
    std::atomic<bool> producerStop{false};
    std::atomic<int>  postRemoveInvocations{0};

    // Producer thread: tight loop of Raise. Each iteration snapshots the
    // handler list under the lock, releases the lock, then invokes from the
    // snapshot.
    std::thread producer([&event, &producerStop]() {
        while (!producerStop.load(std::memory_order_relaxed))
        {
            event();
        }
    });

    // Test thread: per iteration, register a handler, wait briefly so the
    // producer is likely to take at least one snapshot including the handler,
    // Remove the handler, then observe whether it still fires.
    constexpr int ITERATIONS = 1000;
    for (int i = 0; i < ITERATIONS; i++)
    {
        std::atomic<bool> removed{false};

        size_t id = event.Add([&removed, &postRemoveInvocations]()
        {
            // Brief sleep widens the in-handler window so that Remove
            // is likely to run on the test thread while we are still
            // inside this handler invocation.
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            if (removed.load(std::memory_order_acquire))
            {
                postRemoveInvocations.fetch_add(1, std::memory_order_relaxed);
            }
        });

        std::this_thread::sleep_for(std::chrono::microseconds(50));

        event.Remove(id);
        removed.store(true, std::memory_order_release);

        // Allow any in-flight Raise to finish invoking the handler from its
        // already-taken snapshot.
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    producerStop.store(true, std::memory_order_relaxed);
    producer.join();

    SPXTEST_REQUIRE(postRemoveInvocations.load() > 0);
}SPXTEST_CASE_END()

SPXTEST_CASE_BEGIN("Event<> Remove vs in-flight Raise produces UAF if handler captures unprotected state",
                   "[.][util][event][architectural_property]")
{
    // Hidden ([.] tag) demonstration test. Deliberately produces a
    // heap-use-after-free under ASAN by registering a handler that captures
    // a raw pointer to a heap-allocated fixture, then deleting the fixture
    // immediately after Remove. Any concurrent Raise that snapshotted the
    // handler before Remove will invoke the handler against the freed
    // fixture.
    //
    // The test is hidden by default because (a) it intentionally produces
    // a UAF that would abort the test binary under the default ASAN
    // configuration, and (b) it documents a property already asserted by
    // the observation test above. Run it explicitly when investigating
    // Event<> semantics or validating consumer-side lifetime-extension
    // patterns, with:
    //
    //   ASAN_OPTIONS="abort_on_error=0:halt_on_error=0:detect_leaks=0:print_stacktrace=1:symbolize=1"
    //     build/bin/core_tests "Event<> Remove vs in-flight Raise produces UAF*"
    //
    // Without ASAN, this test typically completes silently because the
    // freed storage is usually still mapped and contains plausible values;
    // the counter increment "succeeds" against zombie memory.

    Event<> event;
    std::atomic<bool> producerStop{false};

    std::thread producer([&event, &producerStop]() {
        while (!producerStop.load(std::memory_order_relaxed))
        {
            event();
        }
    });

    struct Fixture
    {
        std::atomic<int> counter{0};
    };

    constexpr int ITERATIONS = 1000;
    for (int i = 0; i < ITERATIONS; i++)
    {
        auto* fixture = new Fixture();

        size_t id = event.Add([fixture]()
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            // Deref of raw pointer. If the fixture has been deleted while
            // an in-flight Raise still holds a snapshot containing this
            // handler, this is heap-use-after-free.
            fixture->counter.fetch_add(1, std::memory_order_relaxed);
        });

        std::this_thread::sleep_for(std::chrono::microseconds(50));

        event.Remove(id);

        // Delete fixture immediately after Remove. Any Raise that snapshotted
        // the handler list before Remove ran but is still mid-iteration over
        // its snapshot will invoke the handler, which dereferences the
        // freed Fixture::counter.
        delete fixture;
    }

    producerStop.store(true, std::memory_order_relaxed);
    producer.join();

    // This test does not assert; success criterion under ASAN is the UAF
    // report. Without ASAN the test completes without observable failure.
}SPXTEST_CASE_END()
