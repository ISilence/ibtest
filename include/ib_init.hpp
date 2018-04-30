#ifndef GPU_COMP_IB_INIT_HPP
#define GPU_COMP_IB_INIT_HPP

#include <infiniband/verbs.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PINGPONG_RECV_WRID = 1,
    PINGPONG_SEND_WRID = 2,
};

struct pingpong_context {
    struct ibv_context *context;
    struct ibv_comp_channel *channel;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    char *buf;
    union {
        struct ibv_cq *cq;
        struct ibv_cq_ex *cq_ex;
    } cq_s;
    struct ibv_qp *qp;
    int size;
    int send_flags;
    int rx_depth;
    int pending;
    struct ibv_port_attr portinfo;
};

struct pingpong_dest {
    int lid;
    int qpn;
    int psn;
    union ibv_gid gid;
};
//
//enum ibv_mtu pp_mtu_to_enum(int mtu);
//int pp_get_port_info(struct ibv_context *context, int port,
//                     struct ibv_port_attr *attr);
//void wire_gid_to_gid(const char *wgid, union ibv_gid *gid);
//void gid_to_wire_gid(const union ibv_gid *gid, char wgid[]);
//
//
//
//
//int pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn,
//                      enum ibv_mtu mtu, int sl,
//                      struct pingpong_dest *dest, int sgid_idx);
//
//struct pingpong_dest *pp_client_exch_dest(const char *servername, int port,
//                                          const struct pingpong_dest *my_dest);
////struct pingpong_dest *pp_server_exch_dest(struct pingpong_context *ctx,
////                                         int ib_port, enum ibv_mtu mtu,
////                                         int port, int sl,
////                                         const struct pingpong_dest *my_dest,
////                                         int sgid_idx);

int pp_post_recv(struct pingpong_context *ctx, int n);
int pp_post_send(struct pingpong_context *ctx);

int parse_single_wc(struct pingpong_context *ctx, int *scnt,
                    int *rcnt, int *routs, int iters,
                    uint64_t wr_id, enum ibv_wc_status status);

//struct pingpong_context *pp_init_ctx(struct ibv_device *ib_dev, int size,
//                                     int rx_depth, int port, int fd);

static struct ibv_cq *pp_cq(struct pingpong_context *ctx) {
    return ctx->cq_s.cq;
}

struct params {
    const char *servername;
    unsigned int size;
    unsigned int iters;
    int dmabuf;
    int gidx;
};

int init_ibv_pingpong(   struct params *ps,
                         int fd,
                         struct pingpong_context **pctx,
                         int *routs);

void parse_args(int argc, char *argv[], struct params* params);

static inline void fill_test_data(size_t size, char *va)
{
    for (int i = 0; i < size; i++)
        va[i] = (char)(i % (256 - 13));
}

#ifdef __cplusplus
}
#endif

#endif //GPU_COMP_IB_INIT_HPP
