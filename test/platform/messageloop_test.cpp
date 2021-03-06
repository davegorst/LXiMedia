#include "test.h"
#include "platform/messageloop.cpp"
#include <thread>

static const struct messageloop_test
{
    messageloop_test()
        : post_test(this, "platform::messageloop::post", &messageloop_test::post),
          send_test(this, "platform::messageloop::send", &messageloop_test::send),
          process_events_test(this, "platform::messageloop::process_events", &messageloop_test::process_events),
          timer_test(this, "platform::messageloop::timer", &messageloop_test::timer)
    {
    }

    struct test post_test;
    void post()
    {
        class platform::messageloop messageloop;
        class platform::messageloop_ref messageloop_ref(messageloop);
        messageloop_ref.post([&messageloop] { messageloop.stop(123); });
        test_assert(messageloop.run() == 123);
    }

    struct test send_test;
    void send()
    {
        class platform::messageloop messageloop;

        int test = 0;
        std::thread sender([&messageloop, &test]
        {
            messageloop.send([&test] { test = 456; });
            messageloop.stop(123);
        });

        test_assert(messageloop.run() == 123);

        sender.join();
        test_assert(test == 456);
    }

    struct test process_events_test;
    void process_events()
    {
        class platform::messageloop messageloop;
        class platform::messageloop_ref messageloop_ref(messageloop);

        int test = 0;
        messageloop_ref.post([&test] { test = 456; });
        test_assert(test == 0);
        test_assert(messageloop.process_events(std::chrono::seconds(0)) == -1);
        test_assert(test == 456);
    }

    struct test timer_test;
    void timer()
    {
#if defined(__unix__)
        static const int granularity = 8;
#elif defined(WIN32) || defined(__APPLE__)
        static const int granularity = 32;
#endif

        class platform::messageloop messageloop;

        int test = 0;
        class platform::timer timer_1(messageloop, [&test] { test++; });
        class platform::timer timer_2(messageloop, [&test, &timer_1] { test++; timer_1.stop(); });
        class platform::timer timer_3(messageloop, [&messageloop] { messageloop.stop(123); });

        timer_1.start(std::chrono::milliseconds(granularity * 2));
        timer_2.start(std::chrono::milliseconds(granularity * 5), true);
        timer_3.start(std::chrono::milliseconds(granularity * 12), true);

        std::chrono::steady_clock clock;
        const auto start = clock.now();
        test_assert(messageloop.run() == 123);
        const auto stop = clock.now();

        test_assert(test == 3);

        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        test_assert(duration.count() > granularity * 8);
        test_assert(duration.count() < granularity * 14);
    }
} messageloop_test;
