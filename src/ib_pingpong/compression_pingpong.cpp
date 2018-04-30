#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ib_init.hpp>
#include <compressor.hpp>
#include <bdi_compressor.hpp>
#include "../timer.hpp"
#include <testing.hpp>

#include <unistd.h>
#include <malloc.h>

typedef cl_int (OCLGETMEMOBJECTFD)(cl_context, cl_mem, int *);
OCLGETMEMOBJECTFD *oclGetMemObjectFd = NULL;

double comp_test(struct params *ps) {
    struct pingpong_context *ctx;
    unsigned int netsize, initsize = ps->size;
    int routs, rcnt, scnt;
    int fd = -1;

    netsize = (unsigned int)BDICompressor::get_compr_size(initsize / sizeof(int));
    // std::cout << "net size: " << netsize << std::endl;
    ps->size = netsize;
    ps->dmabuf = 1;

    auto device = DeviceFinder::get_device(DT_IGPU);
    BDICompressor comp;
    Program dumper = compile_program(&device, "kernel.cl", "bdi_decompress");
    char *va = (char *)memalign((size_t)sysconf(_SC_PAGESIZE), initsize);
    comp.set_mem(va, initsize / sizeof(int));
    cl_mem mem_trg = comp.mem_comp.value().mem;

#ifdef CL_VERSION_1_2
    oclGetMemObjectFd = (OCLGETMEMOBJECTFD *)clGetExtensionFunctionAddressForPlatform(device.platform_id, "clGetMemObjectFdIntel");
#else
    oclGetMemObjectFd = (OCLGETMEMOBJECTFD *)clGetExtensionFunctionAddress("clGetMemObjectFdIntel");
#endif
    if (oclGetMemObjectFd(device.context, mem_trg, &fd) != CL_SUCCESS)
        exit(3);
    if (fd < 0) {
        printf("Invalid fd\n");
        exit(4);
    }

    if (init_ibv_pingpong(ps, fd, &ctx, &routs)) {
        std::cerr << "ib init failed" << std::endl;
        exit(1);
    }

    double res = Benchmark::run(TEST_CNT, 100000000000, [&](Timer& timer) {
        timer.stop();
        fill_test_data(initsize, va);
        timer.start();

        ctx->pending = PINGPONG_RECV_WRID;

        comp.compress();
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
                comp.compress();

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
    ps->size = initsize;
    return res;
}

int main(int argc, char *argv[]) {
    test_perf(argc, argv, comp_test);
}
