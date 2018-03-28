#pragma once

#include "dbg_protocol.h"
#include <rapidjson/schema.h> 

namespace vscode {
	struct io;
	typedef rapidjson::SchemaDocument schema;

	schema*   io_schema(const std::wstring& schemafile);
	rprotocol io_input (io* io, schema* schema = nullptr);
	void      io_output(io* io, const wprotocol& wp);
}

