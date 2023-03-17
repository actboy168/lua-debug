#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/path_helper.h>
#include <stdio.h>

#if !defined(_WIN32)
#    include <sys/select.h>
#    include <unistd.h>
#else
#    include <WinSock.h>
#endif

namespace luadebug::log {

    static bool attach_mode = false;

    void init(bool attach) {
        attach_mode = attach;
    }

    using namespace bee::net;

    static int nfd(socket::fd_t fd) {
#if defined(_WIN32)
        return 0;
#else
        return fd + 1;
#endif
    }

    static bool sendto_frontend(socket::fd_t fd, const char* buf, int len) {
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
        socket::fd_t fd = socket::open(socket::protocol::unix);
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
        socket::fd_t newfd;
        if (socket::fdstat::success != socket::accept(fd, newfd)) {
            socket::close(fd);
            return;
        }
        socket::close(fd);
        int err = socket::errcode(newfd);
        if (err != 0) {
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
        std::string json = std::format(jsonfmt, attach_mode ? "attach" : "launch", msg);
        std::string data = std::format("Content-Length: {}\r\n\r\n{}", json.size(), json);
        sendto_frontend(newfd, data.data(), (int)data.size());
    }
}
