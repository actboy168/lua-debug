#pragma once

#include <bee/net/socket.h>
#include <bee/net/endpoint.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/path_helper.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <sys/select.h>
#include <unistd.h>
#else
#include <WinSock.h>
#endif

namespace luadebug::log {

static bool attach_mode = false;

void init(bool attach) {
	attach_mode = attach;
}

void info(const char* msg) {
#ifndef NDEBUG
	//TODO: use std::print
	fprintf(stderr, "[lua-debug][launcher]%s\n", msg);
#endif
}

using namespace bee::net;

inline int fatal_nfd(socket::fd_t fd) {
#if defined(_WIN32)
	return 0;
#else
	return fd+1;
#endif
}

inline bool fatal_send(socket::fd_t fd, const char* buf, int len) {
	fd_set writefds;
	FD_ZERO(&writefds);
	FD_SET(fd, &writefds);
	if (::select(fatal_nfd(fd), NULL, &writefds, NULL, 0) <= 0) {
		return false;
	}
	int rc;
	switch (socket::send(fd, rc, buf, len)) {
	case socket::status::wait:
		return fatal_send(fd, buf, len);
	case socket::status::success:
		if (rc >= len) {
			return true;
		}
		return fatal_send(fd, buf + rc, len - rc);
	case socket::status::failed:
		return false;
	default:
		std::unreachable();
	}
}

void fatal(const char* msg) {
	info(msg);

	auto dllpath = bee::path_helper::dll_path();
	if (!dllpath) {
		return;
	}
	auto rootpath = dllpath.value().parent_path().parent_path();
	auto path = std::format("{}/tmp/pid_{}", rootpath.generic_u8string(),
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
	socket::fd_t fd = socket::open(socket::protocol::uds);
	if (socket::status::success != socket::bind(fd, ep)) {
		socket::close(fd);
		return;
	}
	if (socket::status::success != socket::listen(fd, 5)) {
		socket::close(fd);
		return;
	}
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	if (::select(fatal_nfd(fd), &readfds, NULL, NULL, 0) <= 0) {
		socket::close(fd);
		return;
	}
	socket::fd_t newfd;
	if (socket::status::success != socket::accept(fd, newfd)) {
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
	std::string json = std::format(jsonfmt, attach_mode? "attach": "launch", msg);
	std::string data = std::format("Content-Length: {}\r\n\r\n{}", json.size(), json);
	fatal_send(newfd, data.data(), (int)data.size());
}
}
