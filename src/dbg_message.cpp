#include "dbg_message.h"  
#include "dbg_protocol.h"  	 
#include "dbg_file.h"  		 
#include "dbg_format.h"
#include <rapidjson/schema.h>	
#include <rapidjson/error/en.h>

#if 1	 
#	define log(...)
#else
template <class... Args>
static void log(const char* fmt, const Args& ... args)
{
	vscode::printf(fmt, args...);
}
#endif	 

namespace vscode
{
	session::session(server* server, net::poller_t* poll)
		: net::tcp::stream(poll)
		, server_(server)
		, stream_stat_(0)
		, stream_buf_()
		, stream_len_(0)
	{
	}

	bool session::input_empty() const
	{
		return input_queue_.empty();
	}

	bool session::output(const wprotocol& wp)
	{
		if (!wp.IsComplete())
			return false;
		log("%s\n", wp.data());
		send(format("Content-Length: %d\r\n\r\n", wp.size()));
		send(wp);
		return true;
	}

	rprotocol session::input()
	{
		rprotocol r = std::move(input_queue_.front());
		input_queue_.pop();
		return r;
	}

	void session::event_close()
	{
		base_type::event_close();
		server_->event_close();
	}

	bool session::event_in()
	{
		if (!base_type::event_in())
			return false;
		if (!unpack())
			return false;
		return true;
	}

	bool session::unpack()
	{
		for (size_t n = base_type::recv_size(); n; --n)
		{
			char c = 0;
			base_type::recv(&c, 1);
			stream_buf_.push_back(c);
			switch (stream_stat_)
			{
			case 0:
				if (c == '\r') stream_stat_ = 1;
				break;
			case 1:
				stream_stat_ = 0;
				if (c == '\n')
				{
					if (stream_buf_.substr(0, 16) != "Content-Length: ")
					{
						return false;
					}
					try {
						stream_len_ = (size_t)std::stol(stream_buf_.substr(16, stream_buf_.size() - 18));
						stream_stat_ = 2;
					}
					catch (...) {
						return false;
					}
					stream_buf_.clear();
				}
				break;
			case 2:
				if (stream_buf_.size() >= (stream_len_ + 2))
				{
					if (!event_message(stream_buf_, 2, stream_len_))
						return false;
					stream_buf_.clear();
					stream_stat_ = 0;
				}
				break;
			}
		}
		return true;
	}

	bool session::event_message(const std::string& buf, size_t start, size_t count)
	{
		rapidjson::Document	d;
		if (d.Parse(buf.data() + start, count).HasParseError())
		{
			log("Input is not a valid JSON\n");
			log("Error(offset %u): %s\n", static_cast<unsigned>(d.GetErrorOffset()), rapidjson::GetParseError_En(d.GetParseError()));
			return true;
		}
		if (schema_)
		{
			rapidjson::SchemaValidator validator(*schema_);
			if (!d.Accept(validator))
			{
				rapidjson::StringBuffer sb;
				validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
				log("Invalid schema: %s\n", sb.GetString());
				log("Invalid keyword: %s\n", validator.GetInvalidSchemaKeyword());
				sb.Clear();
				validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
				log("Invalid document: %s\n", sb.GetString());
				return true;
			}
		}
		log("%s\n", buf.substr(start, count).c_str());
		input_queue_.push(rprotocol(std::move(d)));
		return true;
	}

	bool session::open_schema(const std::string& schema_file)
	{
		file file(schema_file.c_str(), std::ios_base::in);
		if (!file.is_open())
		{
			return false;
		}
		std::string buf = file.read<std::string>();
		rapidjson::Document sd;
		if (sd.Parse(buf.data(), buf.size()).HasParseError())
		{
			log("Input is not a valid JSON\n");
			log("Error(offset %u): %s\n", static_cast<unsigned>(sd.GetErrorOffset()), rapidjson::GetParseError_En(sd.GetParseError()));
			return false;
		}
		schema_.reset(new rapidjson::SchemaDocument(sd));
		return true;
	}

	server::server(net::poller_t* poll)
		: base_type(poll)
		, session_()
	{

	}

	server::~server()
	{
	}

	void server::listen(const net::endpoint& ep)
	{
		base_type::open();
		base_type::listen(ep, std::bind(&server::event_accept, this, std::placeholders::_1, std::placeholders::_2));
	}

	void server::event_accept(net::socket::fd_t fd, const net::endpoint& ep)
	{
		if (session_)
			return;
		session_.reset(new session(this, get_poller()));
		if (!schema_file_.empty())
		{
			session_->open_schema(schema_file_);
		}
		session_->attach(fd, ep);
	}

	void server::event_close()
	{
		session_.reset();
	}

	bool server::output(const wprotocol& wp)
	{
		if (!session_)
			return false;
		return session_->output(wp);
	}

	rprotocol server::input()
	{
		if (!session_)
			return rprotocol();
		if (session_->input_empty())
			return rprotocol();
		return session_->input();
	}

	bool server::input_empty()	const
	{
		if (!session_)
			return false;
		return session_->input_empty();
	}

	void server::set_schema(const char* file)
	{
		schema_file_ = file;
	}
}
