#define _GNU_SOURCE
#include <ib_init.hpp>
#include <compressor.hpp>
#include "../timer.hpp"
#include <testing.hpp>

// #define CHECK_RESULT

static double server_foo(struct params *ps)
{
    struct pingpong_context *ctx;
    int routs, rcnt, scnt;

    ps->servername = NULL;

    if (init_ibv_pingpong(ps, -1, &ctx, &routs)) {
        fprintf(stderr, "ib init failed\n");
        exit(1);
    }

    (void)Benchmark::run(TEST_CNT, 100000000000, [&](Timer& timer) {
        ctx->pending = PINGPONG_RECV_WRID;

        rcnt = scnt = 0;
        while (rcnt < ps->iters || scnt < ps->iters) {
            int ret;
            int ne, i;
            struct ibv_wc wc[2];

            do {
                ne = ibv_poll_cq(pp_cq(ctx), 2, wc);
                if (ne < 0) {
                    fprintf(stderr, "poll CQ failed %d\n", ne);
                    return 1;
                }
            } while (ne < 1);

            for (i = 0; i < ne; ++i) {
                ret = parse_single_wc(ctx, &scnt, &rcnt, &routs,
                                      ps->iters, wc[i].wr_id, wc[i].status);
                if (ret) {
                    fprintf(stderr, "parse WC failed %d\n", ne);
                    return 1;
                }
            }
        }

#ifdef CHECK_RESULT
        if (!ps->servername) {
            for (int i = 0; i < ps->size; i ++) {
                char v = ctx->buf[i];
                char exp = (char)(i % (256 - 13));

                if (v != exp) {
                    printf("invalid data at %d (%c : %c)\n", i, v, exp);
                    break;
                }
            }
            printf("good data! %i\n", ps->size);
        }
#endif
    });

    if (ctx->buf)
        free(ctx->buf);
}

int main(int argc, char *argv[]) {
    test_perf(argc, argv, server_foo);
}
