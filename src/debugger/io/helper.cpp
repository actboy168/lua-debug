#include <debugger/io/helper.h>
#include <debugger/io/base.h>
#include <rapidjson/schema.h>
#include <rapidjson/error/en.h>
#include <bee/utility/unicode.h>
#include <fstream>

#if 1
#	define log(...)
#else

#include <base/util/format.h>
#include <Windows.h>

template <class... Args>
static void log(const char* fmt, const Args& ... args)
{
	auto s = base::format(fmt, args...);
	OutputDebugStringA(s.c_str());
}

#endif	 

namespace vscode {

	class file {
	public:
		file(const char* filename) {
#if defined(__MINGW32__)
			file_ = _wfopen(base::u2w(filename).c_str(), L"rb");
#elif defined(_WIN32)
			file_.open(bee::u2w(filename).c_str(), std::ios_base::binary | std::ios_base::in);
#else
			file_.open(filename, std::ios_base::binary | std::ios_base::in);
#endif
		}
		~file() {
#if defined(__MINGW32__)
			fclose(file_);
#else
			file_.close();
#endif
		}
		bool is_open() const {
			return !!file_;
		}
		template <class SequenceT>
		SequenceT read() {
#if defined(__MINGW32__)
			fseek (file_, 0, SEEK_END);
			long length = ftell(file_);
			fseek(file_, 0, SEEK_SET);
			SequenceT tmp;
			tmp.resize(length);
			fread(tmp.data(), 1, length, file_);
			return std::move(tmp);
#else
			return std::move(SequenceT((std::istreambuf_iterator<char>(file_)), (std::istreambuf_iterator<char>())));
#endif
		}
	private:
		file();
#if defined(__MINGW32__)
		FILE* file_;
#else
		std::fstream file_;
#endif
	};

	bool schema::open(const std::string& path)
	{
		file file(path.c_str());
		if (!file.is_open()) {
			return false;
		}
		std::string buf = file.read<std::string>();
		rapidjson::Document sd;
		if (sd.Parse(buf.data(), buf.size()).HasParseError()) {
			log("Input is not a valid JSON\n");
			log("Error(offset %u): %s\n", static_cast<unsigned>(sd.GetErrorOffset()), rapidjson::GetParseError_En(sd.GetParseError()));
			return false;
		}
		doc.reset(new rapidjson::SchemaDocument(sd));
		return true;
	}

	bool schema::accept(const rapidjson::Document& d) {
		if (!doc) {
			return true;
		}
		rapidjson::SchemaValidator validator(*doc);
		if (!d.Accept(validator))
		{
			rapidjson::StringBuffer sb;
			validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
			log("Invalid schema: %s\n", sb.GetString());
			log("Invalid keyword: %s\n", validator.GetInvalidSchemaKeyword());
			sb.Clear();
			validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
			log("Invalid document: %s\n", sb.GetString());
			return false;
		}
		return true;
	}

	schema::operator bool() const
	{
		return !!doc;
	}

	rprotocol io_input(io::base* io, schema* schema)
	{
		std::string buf;
		if (!io->input(buf)) {
			return rprotocol();
		}
		rapidjson::Document	d;
		if (d.Parse(buf.data(), buf.size()).HasParseError())
		{
			log("Input is not a valid JSON\n");
			log("Error(offset %u): %s\n", static_cast<unsigned>(d.GetErrorOffset()), rapidjson::GetParseError_En(d.GetParseError()));
			io->close();
			return rprotocol();
		}
		if (schema && !schema->accept(d))
		{
			io->close();
			return rprotocol();
		}
		log("%s\n", buf.data());
		return rprotocol(std::move(d));
	}

	void io_output(io::base* io, const wprotocol& wp)
	{
		if (!wp.IsComplete())
			return;
		if (!io->output(wp.data(), wp.size()))
			return;
		log("%s\n", wp.data());
	}

	void io_output(io::base* io, const rprotocol& rp)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		rp.Accept(writer);
		io->output(buffer.GetString(), buffer.GetSize());
	}

}
