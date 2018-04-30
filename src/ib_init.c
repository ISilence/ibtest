/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif
#include <ib_init.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <inttypes.h>
#include <malloc.h>

static int pp_get_port_info(struct ibv_context *context, int port,
                     struct ibv_port_attr *attr)
{
    return ibv_query_port(context, port, attr);
}

static void wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
{
    char tmp[9];
    __be32 v32;
    int i;
    uint32_t tmp_gid[4];

    for (tmp[8] = 0, i = 0; i < 4; ++i) {
        memcpy(tmp, wgid + i * 8, 8);
        sscanf(tmp, "%x", &v32);
        tmp_gid[i] = be32toh(v32);
    }
    memcpy(gid, tmp_gid, sizeof(*gid));
}

static void gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
{
    uint32_t tmp_gid[4];
    int i;

    memcpy(tmp_gid, gid, sizeof(tmp_gid));
    for (i = 0; i < 4; ++i)
        sprintf(&wgid[i * 8], "%08x", htobe32(tmp_gid[i]));
}

int pp_post_recv(struct pingpong_context *ctx, int n)
{
    struct ibv_sge list = {
            .addr	= (uintptr_t) ctx->buf,
            .length = ctx->size,
            .lkey	= ctx->mr->lkey
    };
    struct ibv_recv_wr wr = {
            .wr_id	    = PINGPONG_RECV_WRID,
            .sg_list    = &list,
            .num_sge    = 1,
    };
    struct ibv_recv_wr *bad_wr;
    int i;

    for (i = 0; i < n; ++i)
        if (ibv_post_recv(ctx->qp, &wr, &bad_wr))
            break;

    return i;
}


int pp_post_send(struct pingpong_context *ctx)
{
    struct ibv_sge list = {
            .addr	= (uintptr_t) ctx->buf,
            .length = ctx->size,
            .lkey	= ctx->mr->lkey
    };
    struct ibv_send_wr wr = {
            .wr_id	    = PINGPONG_SEND_WRID,
            .sg_list    = &list,
            .num_sge    = 1,
            .opcode     = IBV_WR_SEND,
            .send_flags = ctx->send_flags,
    };
    struct ibv_send_wr *bad_wr;

    return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

int parse_single_wc(struct pingpong_context *ctx, int *scnt,
                    int *rcnt, int *routs, int iters,
                    uint64_t wr_id, enum ibv_wc_status status)
{
    if (status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                ibv_wc_status_str(status),
                status, (int)wr_id);
        return 1;
    }

    switch ((int)wr_id) {
        case PINGPONG_SEND_WRID:
            ++(*scnt);
            break;

        case PINGPONG_RECV_WRID:
            if (--(*routs) <= 1) {
                *routs += pp_post_recv(ctx, ctx->rx_depth - *routs);
                if (*routs < ctx->rx_depth) {
                    fprintf(stderr,
                            "Couldn't post receive (%d)\n",
                            *routs);
                    return 1;
                }
            }

            ++(*rcnt);
            break;

        default:
            fprintf(stderr, "Completion for unknown wr_id %d\n",
                    (int)wr_id);
            return 1;
    }

    ctx->pending &= ~(int)wr_id;
    if (*scnt < iters && !ctx->pending) {
        if (pp_post_send(ctx)) {
            fprintf(stderr, "Couldn't post send\n");
            return 1;
        }
        ctx->pending = PINGPONG_RECV_WRID |
                       PINGPONG_SEND_WRID;
    }

    return 0;
}

static int pp_connect_ctx(struct pingpong_context *ctx, int port, int my_psn,
                   enum ibv_mtu mtu, int sl,
                   struct pingpong_dest *dest, int sgid_idx)
{
    struct ibv_qp_attr attr = {
            .qp_state		= IBV_QPS_RTR,
            .path_mtu		= mtu,
            .dest_qp_num		= dest->qpn,
            .rq_psn			= dest->psn,
            .max_dest_rd_atomic	= 1,
            .min_rnr_timer		= 12,
            .ah_attr		= {
                    .is_global	= 0,
                    .dlid		= dest->lid,
                    .sl		= sl,
                    .src_path_bits	= 0,
                    .port_num	= port
            }
    };

    if (dest->gid.global.interface_id) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.dgid = dest->gid;
        attr.ah_attr.grh.sgid_index = sgid_idx;
    }
    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE              |
                      IBV_QP_AV                 |
                      IBV_QP_PATH_MTU           |
                      IBV_QP_DEST_QPN           |
                      IBV_QP_RQ_PSN             |
                      IBV_QP_MAX_DEST_RD_ATOMIC |
                      IBV_QP_MIN_RNR_TIMER)) {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }

    attr.qp_state	    = IBV_QPS_RTS;
    attr.timeout	    = 14;
    attr.retry_cnt	    = 7;
    attr.rnr_retry	    = 7;
    attr.sq_psn	    = my_psn;
    attr.max_rd_atomic  = 1;
    if (ibv_modify_qp(ctx->qp, &attr,
                      IBV_QP_STATE              |
                      IBV_QP_TIMEOUT            |
                      IBV_QP_RETRY_CNT          |
                      IBV_QP_RNR_RETRY          |
                      IBV_QP_SQ_PSN             |
                      IBV_QP_MAX_QP_RD_ATOMIC)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    return 0;
}

static struct pingpong_dest *pp_client_exch_dest(const char *servername, int port,
                                          const struct pingpong_dest *my_dest)
{
    struct addrinfo *res, *t;
    struct addrinfo hints = {
            .ai_family   = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM
    };
    char *service;
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1;
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];

    if (asprintf(&service, "%d", port) < 0)
        return NULL;

    n = getaddrinfo(servername, service, &hints, &res);

    if (n < 0) {
        fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
        free(service);
        return NULL;
    }

    for (t = res; t; t = t->ai_next) {
        sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
        if (sockfd >= 0) {
            if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
                break;
            close(sockfd);
            sockfd = -1;
        }
    }

    freeaddrinfo(res);
    free(service);

    if (sockfd < 0) {
        fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
        return NULL;
    }

    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn,
            my_dest->psn, gid);
    if (write(sockfd, msg, sizeof msg) != sizeof msg) {
        fprintf(stderr, "Couldn't send local address\n");
        goto out;
    }

    if (read(sockfd, msg, sizeof msg) != sizeof msg ||
        write(sockfd, "done", sizeof "done") != sizeof "done") {
        perror("client read/write");
        fprintf(stderr, "Couldn't read/write remote address\n");
        goto out;
    }

    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest)
        goto out;

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn,
           &rem_dest->psn, gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

out:
    close(sockfd);
    return rem_dest;
}

static struct pingpong_dest *pp_server_exch_dest(struct pingpong_context *ctx,
                                                 int ib_port, enum ibv_mtu mtu,
                                                 int port, int sl,
                                                 const struct pingpong_dest *my_dest,
                                                 int sgid_idx)
{
    struct addrinfo *res, *t;
    struct addrinfo hints = {
            .ai_flags    = AI_PASSIVE,
            .ai_family   = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM
    };
    char *service;
    char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
    int n;
    int sockfd = -1, connfd;
    struct pingpong_dest *rem_dest = NULL;
    char gid[33];

    if (asprintf(&service, "%d", port) < 0)
        return NULL;

    n = getaddrinfo(NULL, service, &hints, &res);

    if (n < 0) {
        fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
        free(service);
        return NULL;
    }

    for (t = res; t; t = t->ai_next) {
        sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
        if (sockfd >= 0) {
            n = 1;

            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

            if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
                break;
            close(sockfd);
            sockfd = -1;
        }
    }

    freeaddrinfo(res);
    free(service);

    if (sockfd < 0) {
        fprintf(stderr, "Couldn't listen to port %d\n", port);
        return NULL;
    }

    listen(sockfd, 1);
    connfd = accept(sockfd, NULL, NULL);
    close(sockfd);
    if (connfd < 0) {
        fprintf(stderr, "accept() failed\n");
        return NULL;
    }

    n = read(connfd, msg, sizeof msg);
    if (n != sizeof msg) {
        perror("server read");
        fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int) sizeof msg);
        goto out;
    }

    rem_dest = malloc(sizeof *rem_dest);
    if (!rem_dest)
        goto out;

    sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn,
           &rem_dest->psn, gid);
    wire_gid_to_gid(gid, &rem_dest->gid);

    if (pp_connect_ctx(ctx, ib_port, my_dest->psn, mtu, sl, rem_dest,
                       sgid_idx)) {
        fprintf(stderr, "Couldn't connect to remote QP\n");
        free(rem_dest);
        rem_dest = NULL;
        goto out;
    }


    gid_to_wire_gid(&my_dest->gid, gid);
    sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn,
            my_dest->psn, gid);
    if (write(connfd, msg, sizeof msg) != sizeof msg ||
        read(connfd, msg, sizeof msg) != sizeof "done") {
        fprintf(stderr, "Couldn't send/recv local address\n");
        free(rem_dest);
        rem_dest = NULL;
        goto out;
    }


out:
    close(connfd);
    return rem_dest;
}

static struct pingpong_context *pp_init_ctx(struct ibv_device *ib_dev, int size,
                                     int rx_depth, int port,
                                     int fd)
{
    struct pingpong_context *ctx;
    int access_flags = IBV_ACCESS_LOCAL_WRITE;

    ctx = calloc(1, sizeof *ctx);
    if (!ctx)
        return NULL;

    ctx->size       = size;
    ctx->send_flags = IBV_SEND_SIGNALED;
    ctx->rx_depth   = rx_depth;

    ctx->buf = NULL;
    if (fd < 0) {
//        ctx->buf = malloc(size + 10) + 10;
        ctx->buf = memalign((int)sysconf(_SC_PAGESIZE), size);
        if (!ctx->buf) {
            fprintf(stderr, "Couldn't allocate work buf.\n");
            goto error;
        }
    }
//
//    /* FIXME memset(ctx->buf, 0, size); */
//    memset(ctx->buf, 0x7b, size);

    ctx->context = ibv_open_device(ib_dev);
    if (!ctx->context) {
        fprintf(stderr, "Couldn't get context for %s\n",
                ibv_get_device_name(ib_dev));
        goto error;
    }

    ctx->channel = NULL;
    ctx->pd = ibv_alloc_pd(ctx->context);
    if (!ctx->pd) {
        fprintf(stderr, "Couldn't allocate PD\n");
        goto error;
    }

    if (fd < 0)
        ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size, access_flags);
    else
        ctx->mr = ibv_reg_mr_dma_buf(ctx->pd, fd, access_flags);

    if (!ctx->mr) {
        fprintf(stderr, "Couldn't register MR\n");
        goto error;
    }


    ctx->cq_s.cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL,
                                 ctx->channel, 0);

    if (!pp_cq(ctx)) {
        fprintf(stderr, "Couldn't create CQ\n");
        goto error;
    }

    {
        struct ibv_qp_attr attr;
        struct ibv_qp_init_attr init_attr = {
                .send_cq = pp_cq(ctx),
                .recv_cq = pp_cq(ctx),
                .cap     = {
                        .max_send_wr  = 1,
                        .max_recv_wr  = rx_depth,
                        .max_send_sge = 1,
                        .max_recv_sge = 1
                },
                .qp_type = IBV_QPT_RC
        };

        ctx->qp = ibv_create_qp(ctx->pd, &init_attr);
        if (!ctx->qp)  {
            fprintf(stderr, "Couldn't create QP\n");
            goto error;
        }

        ibv_query_qp(ctx->qp, &attr, IBV_QP_CAP, &init_attr);
        if (init_attr.cap.max_inline_data >= size) {
            ctx->send_flags |= IBV_SEND_INLINE;
        }
    }

    {
        struct ibv_qp_attr attr = {
                .qp_state        = IBV_QPS_INIT,
                .pkey_index      = 0,
                .port_num        = port,
                .qp_access_flags = 0
        };

        if (ibv_modify_qp(ctx->qp, &attr,
                          IBV_QP_STATE              |
                          IBV_QP_PKEY_INDEX         |
                          IBV_QP_PORT               |
                          IBV_QP_ACCESS_FLAGS)) {
            fprintf(stderr, "Failed to modify QP to INIT\n");
            goto error;
        }
    }

    return ctx;


    error:
    return NULL;
}

int init_ibv_pingpong(   struct params *ps,
                         int fd,
                         struct pingpong_context **pctx,
                         int *routs)
{
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    unsigned int rx_depth = 500;
    int ib_port = 1;
    struct pingpong_context *ctx;
    struct pingpong_dest my_dest = {};
    struct pingpong_dest *rem_dest;
    unsigned int port = 18515;
    char gid[33];
    enum ibv_mtu mtu = IBV_MTU_1024;
    int sl = 0;

    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        perror("Failed to get IB devices list");
        return 1;
    }

    ib_dev = *dev_list;
    if (!ib_dev) {
        fprintf(stderr, "No IB devices found\n");
        return 1;
    }

    ctx = pp_init_ctx(ib_dev, (int)ps->size, rx_depth, ib_port, fd);
    if (!ctx)
        return 1;

    *routs = pp_post_recv(ctx, ctx->rx_depth);
    if (*routs < ctx->rx_depth) {
        fprintf(stderr, "Couldn't post receive (%d)\n", *routs);
        return 1;
    }

    if (pp_get_port_info(ctx->context, ib_port, &ctx->portinfo)) {
        fprintf(stderr, "Couldn't get port info\n");
        return 1;
    }

    my_dest.lid = ctx->portinfo.lid;
    if (ctx->portinfo.link_layer != IBV_LINK_LAYER_ETHERNET &&
        !my_dest.lid) {
        fprintf(stderr, "Couldn't get local LID\n");
        return 1;
    }

    if (ps->gidx >= 0) {
        if (ibv_query_gid(ctx->context, (uint8_t)ib_port, ps->gidx, &my_dest.gid)) {
            fprintf(stderr, "can't read sgid of index %d\n", ps->gidx);
            return 1;
        }
    } else
        memset(&my_dest.gid, 0, sizeof my_dest.gid);

    my_dest.qpn = ctx->qp->qp_num;
    my_dest.psn = lrand48() & 0xffffff;
    inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof gid);
//    printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
//           my_dest.lid, my_dest.qpn, my_dest.psn, gid);


    if (ps->servername)
        rem_dest = pp_client_exch_dest(ps->servername, port, &my_dest);
    else
        rem_dest = pp_server_exch_dest(ctx, ib_port, mtu, port, sl,
                                       &my_dest, ps->gidx);
    if (!rem_dest)
        return 1;

    inet_ntop(AF_INET6, &rem_dest->gid, gid, sizeof gid);
//    printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
//           rem_dest->lid, rem_dest->qpn, rem_dest->psn, gid);

    if (ps->servername)
        if (pp_connect_ctx(ctx, ib_port, my_dest.psn, mtu, sl, rem_dest, ps->gidx))
            return 1;

    *pctx = ctx;
    return 0;
}


static void usage(const char *argv0) {
    printf("Usage:\n");
    printf("  %s            start a server and wait for connection\n", argv0);
    printf("  %s <host>     connect to server at <host>\n", argv0);
    printf("\n");
    printf("Options:\n");
    printf("  -s, --size=<size>      size of message to exchange (default 4096)\n");
    printf("  -n, --iters=<iters>    number of exchanges (default 1000)\n");
}

void parse_args(int argc, char *argv[], struct params* params)
{
    params->dmabuf = 0;
    params->size = 4096;
    params->servername = NULL;
    params->iters = 1000;
    params->gidx = 1;

    while (1) {
        int c;

        static struct option long_options[] = {
                {"size", 1, NULL, 's'},
                {"iters", 1, NULL, 'n'},
                {"dma-buf", 1, NULL, 'b'},
                {}
        };
        c = getopt_long(argc, argv, "s:n:b",
                        long_options, NULL);
        if (c == -1)
            break;
        switch (c) {
            case 's':
                params->size = (unsigned)strtoul(optarg, NULL, 0);
                break;
            case 'n':
                params->iters = (unsigned)strtoul(optarg, NULL, 0);
                break;
            case 'b':
                params->dmabuf = 1;
                break;
            default:
                usage(argv[0]);
                exit(1);
        }
    }
}

