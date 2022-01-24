#include <bal/abi.h>
#include <brutal/debug.h>
#include <ipc/component.h>

typedef struct
{
    IpcComponent *self;
    BrMsg msg;
    IpcHandler *fn;
    void *vtable;
    void *ctx;
} BrEvCtx;

void *br_ev_handle(BrEvCtx *ctx)
{
    BrMsg msg = ctx->msg;
    ctx->fn(ctx->self, &msg, ctx->vtable, ctx->ctx);
    return nullptr;
}

static void *ipc_component_dispatch(IpcComponent *self)
{
    do
    {
        BrIpcArgs ipc = {
            .flags = BR_IPC_RECV | BR_IPC_BLOCK,
            .deadline = fiber_deadline(),
        };

        BrResult result = br_ipc(&ipc);

        if (result == BR_SUCCESS)
        {
            bool handeled = false;

            vec_foreach_v(pending, &self->pendings)
            {
                if (pending->seq == ipc.msg.seq)
                {
                    pending->resp = ipc.msg;
                    pending->ok = true;
                    handeled = true;
                }
            }

            if (!handeled)
            {
                vec_foreach_v(provider, &self->providers)
                {
                    if (provider->port == ipc.msg.to.port &&
                        provider->proto == ipc.msg.prot)
                    {
                        fiber_start(
                            (FiberFn *)br_ev_handle,
                            &(BrEvCtx){
                                .self = self,
                                .msg = ipc.msg,
                                .fn = provider->handler,
                                .vtable = provider->vtable,
                                .ctx = provider->ctx,
                            });

                        handeled = true;
                    }
                }
            }

            if (!handeled && self->sink)
            {
                fiber_start(
                    (FiberFn *)br_ev_handle,
                    &(BrEvCtx){
                        .self = self,
                        .msg = ipc.msg,
                        .ctx = nullptr,
                        .fn = self->sink,
                    });
            }
        }

        fiber_yield();
    } while (self->running);

    return nullptr;
}

static IpcComponent *_instance = nullptr;

IpcComponent *ipc_component_self(void)
{
    return _instance;
}

void ipc_component_init(IpcComponent *self, Alloc *alloc)
{
    *self = (IpcComponent){};
    self->alloc = alloc;
    self->running = true;
    self->dispatcher = fiber_start((FiberFn *)ipc_component_dispatch, self);
    self->dispatcher->state = FIBER_IDLE;

    vec_init(&self->pendings, alloc);
    vec_init(&self->providers, alloc);
    vec_init(&self->capabilities, alloc);

    _instance = self;
}

void ipc_component_deinit(IpcComponent *self)
{
    vec_deinit(&self->capabilities);
    vec_deinit(&self->providers);
    vec_deinit(&self->pendings);
}

void ipc_component_inject(IpcComponent *self, IpcCap const *caps, int count)
{
    for (int i = 0; i < count; i++)
    {
        vec_push(&self->capabilities, caps[i]);
    }
}

Iter ipc_component_query(IpcComponent *self, uint32_t proto, IterFn *iter, void *ctx)
{
    vec_foreach(cap, &self->capabilities)
    {
        if (cap->proto == proto && iter(cap, ctx) == ITER_STOP)
        {
            return ITER_STOP;
        }
    }

    return ITER_CONTINUE;
}

IpcCap ipc_component_require(IpcComponent *self, uint32_t proto)
{
    vec_foreach(cap, &self->capabilities)
    {
        if (cap->proto == proto)
        {
            return *cap;
        }
    }

    panic$("No capability found for proto: {}", proto);
}

IpcCap ipc_component_provide(IpcComponent *self, uint32_t id, IpcHandler *fn, void *vtable, void *ctx)
{
    static uint64_t port_id = 0;
    IpcProvider *provider = alloc_make(self->alloc, IpcProvider);

    provider->proto = id;
    provider->port = port_id++;
    provider->handler = fn;
    provider->vtable = vtable;
    provider->ctx = ctx;

    vec_push(&self->providers, provider);

    return (IpcCap){
        .addr = (BrAddr){
            bal_self_id(),
            provider->port,
        },
        .proto = provider->port,
    };
}

static bool wait_pending(void *ctx)
{
    IpcPending *pending = ctx;
    return pending->ok;
}

BrResult ipc_component_request(IpcComponent *self, IpcCap to, BrMsg *req, BrMsg *resp)
{
    static uint32_t seq = 0;
    req->seq = seq++;

    br_ipc(&(BrIpcArgs){
        .to = to.addr,
        .msg = *req,
        .deadline = 0,
        .flags = BR_IPC_SEND,
    });

    IpcPending *pending = alloc_make(self->alloc, IpcPending);
    pending->seq = req->seq;

    vec_push(&self->pendings, pending);

    fiber_block((FiberBlocker){
        .function = wait_pending,
        .context = pending,
        .deadline = -1,
    });

    *resp = pending->resp;

    vec_remove(&self->pendings, pending);
    alloc_free(self->alloc, pending);

    return BR_SUCCESS;
}

BrResult ipc_component_respond(MAYBE_UNUSED IpcComponent *self, BrMsg const *req, BrMsg *resp)
{
    resp->seq = req->seq;
    resp->prot = req->prot;

    return br_ipc(&(BrIpcArgs){
        .to = req->from,
        .flags = BR_IPC_SEND,
        .deadline = 0,
        .msg = *resp,
    });
}

int ipc_component_run(IpcComponent *self)
{
    fiber_await(self->dispatcher);
    return self->result;
}

void ipc_component_exit(IpcComponent *self, int value)
{
    self->result = value;
    self->running = false;
}
