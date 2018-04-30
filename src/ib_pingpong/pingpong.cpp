#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <ib_init.hpp>
#include <compressor.hpp>
#include "../timer.hpp"
#include <testing.hpp>

#include <iostream>

static double def_test(struct params *ps)
{
    struct pingpong_context *ctx;
    int routs, rcnt, scnt;

    if (init_ibv_pingpong(ps, -1, &ctx, &routs)) {
        std::cerr << "ib init failed" << std::endl;
        exit(1);
    }

    auto res = Benchmark::run(TEST_CNT, 100000000000, [&](Timer& timer) {
        timer.stop();
        fill_test_data(ps->size, ctx->buf);
        timer.start();

        ctx->pending = PINGPONG_RECV_WRID;

        if (pp_post_send(ctx)) {
            fprintf(stderr, "Couldn't post send\n");
            exit(1);
        }
        ctx->pending |= PINGPONG_SEND_WRID;

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
                ret = parse_single_wc(ctx, &scnt, &rcnt, &routs, ps->iters,
                                      wc[i].wr_id, wc[i].status);
                if (ret) {
                    fprintf(stderr, "parse WC failed %d\n", ne);
                    return 1;
                }
            }
        }
    });

    if (ctx->buf)
        free(ctx->buf);
    return res;
}

int main(int argc, char *argv[]) {
    test_perf(argc, argv, def_test);
}
