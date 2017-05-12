#pragma once

#include <thread>
#include <vector>
#include <net/queue.h>
#include "dbg_io.h"

class fileio
	: public vscode::io
{
public:
	fileio(FILE* fin, FILE* fout);
	void update(int ms);
	vscode::rprotocol input();
	bool input_empty() const;
	bool output(const vscode::wprotocol& wp);
	void output(const char* str, size_t len);

private:
	FILE* fin_;
	FILE* fout_;
	std::vector<char> buffer_;
	net::queue<vscode::rprotocol, 8> input_queue_;
};

class stdinput
	: public fileio
	, public std::thread
{
public:
	stdinput();
	void run();
	void close();
};
