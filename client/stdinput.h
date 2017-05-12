#pragma once

#include <thread>
#include <vector>
#include "dbg_protocol.h"
#include <net/queue.h>

class stdinput
	: public std::thread
{
public:
	stdinput();
	void run();
	bool update();
	bool empty() const;
	vscode::rprotocol pop();
	static void output(const char* str, size_t len);
	static void output(const vscode::wprotocol& wp);

private:
	std::vector<char> buffer_;
	net::queue<vscode::rprotocol, 8> input_queue_;
};
