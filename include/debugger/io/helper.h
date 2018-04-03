#pragma once

#include <debugger/protocol.h>
#include <debugger/io/base.h>
#include <rapidjson/schema.h> 

namespace vscode {
	class schema
	{
	public:
		bool open(const std::wstring& path);
		bool accept(const rapidjson::Document& d);
		operator bool() const;

	private:
		std::unique_ptr<rapidjson::SchemaDocument> doc;
	};

	rprotocol io_input (io::base* io, schema* schema = nullptr);
	void      io_output(io::base* io, const wprotocol& wp);
	void      io_output(io::base* io, const rprotocol& rp);
}

