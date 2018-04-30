#ifndef GPU_COMP_TESTING_HPP
#define GPU_COMP_TESTING_HPP

#include <ib_init.hpp>
#include <iostream>

//template <typename Foo>
//void test_perf(int argc, char *argv[], Foo foo)
//{
//    struct params ps = {};
//
//    parse_args(argc, argv, &ps);
//    ps.servername = "localhost";
//
//    double ms = foo(&ps);
//
//    {
//        long long bytes = (long long) ps.size * ps.iters * 2;
//
//        auto usec = ms * 1000.0;
//        printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n",
//               bytes, usec / 1000000., bytes * 8. / usec);
//        printf("%d iters in %.2f seconds = %.2f usec/iter\n",
//               ps.iters, usec / 1000000., usec / ps.iters);
//    }
//}

template <typename Foo>
void test_perf(int argc, char *argv[], Foo foo)
{
    struct params ps = {};

    parse_args(argc, argv, &ps);
    ps.servername = "localhost";

    for (int i = 12; i <= 24; ++i) {
        size_t size = 1ul << i;

        ps.size = (unsigned)size;
        double ms = foo(&ps);

        std::cout << size << ' ' << ms << std::endl;
    }

//    {
//        long long bytes = (long long) ps.size * ps.iters * 2;
//
//        auto usec = ms * 1000.0;
//        printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n",
//               bytes, usec / 1000000., bytes * 8. / usec);
//        printf("%d iters in %.2f seconds = %.2f usec/iter\n",
//               ps.iters, usec / 1000000., usec / ps.iters);
//    }
}

#endif //GPU_COMP_TESTING_HPP
