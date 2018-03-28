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
	bool input(std::string& buf);
	bool output(const char* buf, size_t len);
	void raw_output(const char* buf, size_t len);

private:
	FILE* fin_;
	FILE* fout_;
	std::vector<char> buffer_;
	net::queue<std::string, 8> input_;
};

class stdinput
	: public fileio
	, public std::thread
{
public:
	stdinput();
	void run();
	void update(int ms);
	void close();
	bool input(std::string& buf);
	void push_input(std::string&& rp);

private:
	net::queue<std::string, 8> preinput_;
};
