#pragma once

#include <thread>
#include <vector>
#include <net/queue.h>
#include "dbg_io.h"

class stdinput
	: public vscode::io
	, public std::thread
{
public:
	virtual void update(int ms);
	virtual void close();
	virtual bool input(std::string& buf);
	virtual bool output(const char* buf, size_t len);

public:
	stdinput();
	~stdinput();
	void run();
	void push_input(const char* buf, size_t len);
	void raw_output(const char* buf, size_t len);

private:
	net::queue<std::string, 8> input_;
	net::queue<std::string, 8> preinput_;
	std::vector<char>          buffer_;
};
