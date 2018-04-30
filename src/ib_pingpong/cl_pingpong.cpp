#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ib_init.hpp>
#include <testing.hpp>

#include <compressor.hpp>
#include <cl.hpp>
#include "../timer.hpp"


typedef cl_int (OCLGETMEMOBJECTFD)(cl_context, cl_mem, int *);
OCLGETMEMOBJECTFD *oclGetMemObjectFd = NULL;

static double cl_test(struct params *ps)
{
    struct pingpong_context *ctx;
    int routs, rcnt, scnt;
    int fd = -1;

    auto device = DeviceFinder::get_device(DT_IGPU);
    cl_int cl_err;
    cl_mem mem = clCreateBuffer(device.context, CL_MEM_READ_WRITE, ps->size, NULL, &cl_err);
    cl_check(cl_err);

    if (ps->dmabuf) {
        #ifdef CL_VERSION_1_2
            oclGetMemObjectFd = (OCLGETMEMOBJECTFD *)clGetExtensionFunctionAddressForPlatform(device.platform_id, "clGetMemObjectFdIntel");
        #else
            oclGetMemObjectFd = (OCLGETMEMOBJECTFD *)clGetExtensionFunctionAddress("clGetMemObjectFdIntel");
        #endif
        if (oclGetMemObjectFd(device.context, mem, &fd) != CL_SUCCESS)
            exit(3);
    }

    if (init_ibv_pingpong(ps, fd, &ctx, &routs)) {
        std::cerr << "ib init failed" << std::endl;
        exit(1);
    }

    auto res = Benchmark::run(TEST_CNT, 100000000000, [&](Timer& timer) {
        timer.stop();
        char *ptr = (char*)clEnqueueMapBuffer(device.queue, mem, CL_TRUE, CL_MAP_WRITE, 0, ctx->size, 0, NULL, NULL, NULL);
        fill_test_data(ps->size, ptr);
        clEnqueueUnmapMemObject(device.queue, mem, ptr, 0, NULL, NULL);
        clFinish(device.queue);
        timer.start();

        ctx->pending = PINGPONG_RECV_WRID;

        if (!ps->dmabuf) {
            cl_err = clEnqueueReadBuffer(device.queue, mem, CL_TRUE, 0, ctx->size, ctx->buf, 0, NULL, NULL);
            clFinish(device.queue);
            cl_check(cl_err);
        }
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

            if (!ps->dmabuf) {
                cl_err = clEnqueueReadBuffer(device.queue, mem, CL_TRUE, 0, ctx->size, ctx->buf, 0, NULL, NULL);
                clFinish(device.queue);
                cl_check(cl_err);
            }

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
    test_perf(argc, argv, cl_test);
}
