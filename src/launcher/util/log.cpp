#include <autoattach/ctx.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/path_helper.h>
#include <stdio.h>

#include <algorithm>

#if !defined(_WIN32)
#    include <sys/select.h>
#    include <unistd.h>
#else
#    include <WinSock.h>
#endif

namespace luadebug::log {
    using namespace bee::net;

    static int nfd(fd_t fd) {
#if defined(_WIN32)
        return 0;
#else
        return fd + 1;
#endif
    }

    static bool sendto_frontend(fd_t fd, const char* buf, int len) {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd, &writefds);
        if (::select(nfd(fd), NULL, &writefds, NULL, 0) <= 0) {
            return false;
        }
        int rc;
        switch (socket::send(fd, rc, buf, len)) {
        case socket::status::wait:
            return sendto_frontend(fd, buf, len);
        case socket::status::success:
            if (rc >= len) {
                return true;
            }
            return sendto_frontend(fd, buf + rc, len - rc);
        case socket::status::failed:
            return false;
        default:
            std::unreachable();
        }
    }

    void notify_frontend(const std::string& msg) {
        auto dllpath = bee::path_helper::dll_path();
        if (!dllpath) {
            return;
        }
        auto rootpath = dllpath.value().parent_path().parent_path();
        auto path     = std::format("{}/tmp/pid_{}", rootpath.generic_u8string(),
#if defined(_WIN32)
                                GetCurrentProcessId()
#else
                                getpid()
#endif
        );
        if (!socket::initialize()) {
            return;
        }
        endpoint ep = endpoint::from_unixpath(path);
        if (!ep.valid()) {
            return;
        }
        socket::unlink(ep);
        fd_t fd = socket::open(socket::protocol::unix);
        if (!socket::bind(fd, ep)) {
            socket::close(fd);
            return;
        }
        if (!socket::listen(fd, 5)) {
            socket::close(fd);
            return;
        }
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        if (::select(nfd(fd), &readfds, NULL, NULL, 0) <= 0) {
            socket::close(fd);
            return;
        }
        fd_t newfd;
        if (socket::fdstat::success != socket::accept(fd, newfd)) {
            socket::close(fd);
            return;
        }
        socket::close(fd);
        auto err = socket::errcode(newfd);
        if (err) {
            socket::close(newfd);
            return;
        }
        std::string jsonfmt = R"({{
		"type": "response",
		"seq": 0,
		"command": "{}",
		"request_seq": 2,
		"success": false,
		"message": "{}"
	}})";
        jsonfmt.erase(std::remove_if(jsonfmt.begin(), jsonfmt.end(), ::isspace), jsonfmt.end());
        // TODO: parse protocol
        auto is_attach   = autoattach::ctx::get()->lua_module->mode == autoattach::work_mode::attach;
        std::string json = std::format(jsonfmt, is_attach ? "attach" : "launch", msg);
        std::string data = std::format("Content-Length: {}\r\n\r\n{}", json.size(), json);
        sendto_frontend(newfd, data.data(), (int)data.size());
    }
}
