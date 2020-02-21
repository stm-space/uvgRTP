#include "debug.hh"
#include "media_stream.hh"

#include <cstring>
#include <errno.h>

kvz_rtp::media_stream::media_stream(std::string addr, int src_port, int dst_port, rtp_format_t fmt, int flags):
#ifdef __RTP_CRYPTO__
    srtp_(nullptr),
#endif
    socket_(),
    ctx_config_(),
    media_config_(nullptr)
{
    fmt_      = fmt;
    addr_     = addr;
    flags_    = flags;
    src_port_ = src_port;
    dst_port_ = dst_port;
}

kvz_rtp::media_stream::~media_stream()
{
}

rtp_error_t kvz_rtp::media_stream::init_connection()
{
    rtp_error_t ret = RTP_OK;

    if ((ret = socket_.init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

#if 0
    int enable = 1;
    if ((ret = socket_.setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
        return ret;
#endif

    if ((ret = socket_.bind(AF_INET, INADDR_ANY, src_port_)) != RTP_OK)
        return ret;

    addr_out_ = socket_.create_sockaddr(AF_INET, addr_, dst_port_);
    socket_.set_sockaddr(addr_out_);

    return ret;
}

rtp_error_t kvz_rtp::media_stream::init()
{
    if (init_connection() != RTP_OK) {
        LOG_ERROR("Failed to initialize the underlying socket: %s!", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    rtp_  = new kvz_rtp::rtp(fmt_);

    sender_   = new kvz_rtp::sender(socket_, ctx_config_, fmt_, rtp_);
    receiver_ = new kvz_rtp::receiver(socket_, ctx_config_, fmt_, rtp_);

    sender_->init();
    receiver_->start();

    return RTP_OK;
}

#ifdef __RTP_CRYPTO__
rtp_error_t kvz_rtp::media_stream::init(kvz_rtp::zrtp& zrtp)
{
    /* TODO:  */
}
#endif

rtp_error_t kvz_rtp::media_stream::push_frame(uint8_t *data, size_t data_len, int flags)
{
    return sender_->push_frame(data, data_len, flags);
}

rtp_error_t kvz_rtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    return sender_->push_frame(std::move(data), data_len, flags);
}

kvz_rtp::frame::rtp_frame *kvz_rtp::media_stream::pull_frame()
{
    return receiver_->pull_frame();
}

rtp_error_t kvz_rtp::media_stream::install_receive_hook(void *arg, void (*hook)(void *, kvz_rtp::frame::rtp_frame *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    receiver_->install_recv_hook(arg, hook);

    return RTP_OK;
}

rtp_error_t kvz_rtp::media_stream::install_deallocation_hook(void (*hook)(void *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sender_->install_dealloc_hook(hook);

    return RTP_OK;
}

void kvz_rtp::media_stream::set_media_config(void *config)
{
    media_config_ = config;
}

void *kvz_rtp::media_stream::get_media_config()
{
    return media_config_;
}

rtp_error_t kvz_rtp::media_stream::configure_ctx(int flag, ssize_t value)
{
    if (flag < 0 || flag >= RCC_LAST || value < 0)
        return RTP_INVALID_VALUE;

    ctx_config_.ctx_values[flag] = value;

    return RTP_OK;
}

rtp_error_t kvz_rtp::media_stream::configure_ctx(int flag)
{
    if (flag < 0 || flag >= RCE_LAST)
        return RTP_INVALID_VALUE;

    ctx_config_.flags |= flag;

    return RTP_OK;
}