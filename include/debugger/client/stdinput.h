#pragma once

#include <thread>
#include <vector>
#include <readerwriterqueue.h>
#include <debugger/io/base.h>
#include <debugger/protocol.h>

class stdinput
	: public vscode::io::base
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
	void push_input(vscode::rprotocol& rp);
	void raw_output(const char* buf, size_t len);

private:
	moodycamel::ReaderWriterQueue<std::string, 8> input_;
	moodycamel::ReaderWriterQueue<std::string, 8> preinput_;
	std::vector<char>          buffer_;
	std::thread                thread_;
};
