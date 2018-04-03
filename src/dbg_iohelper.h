#pragma once

#include "dbg_protocol.h"
#include "dbg_io.h"
#include <rapidjson/schema.h> 

namespace vscode {
	typedef rapidjson::SchemaDocument schema;

	schema*   io_schema(const std::wstring& schemafile);
	rprotocol io_input (io::base* io, schema* schema = nullptr);
	void      io_output(io::base* io, const wprotocol& wp);
}

