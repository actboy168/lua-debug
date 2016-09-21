#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <net/datetime/now.h>

namespace net { namespace log {
	enum level {
		debug = 0,
		info,
		warning,
		error,
		fatal
	};

	struct source_file
	{
		template <size_t N>
		inline source_file(const char (&arr)[N])
			: data_(arr)
			, size_(N-1)
		{
			static_assert(N > 0, "error: N == 0");
#ifdef WIN32
			const char* slash = strrchr(data_, '\\');
#else
			const char* slash = strrchr(data_, '/');
#endif
			if (slash)
			{
				data_ = slash + 1;
				size_ -= static_cast<size_t>(data_ - arr);
			}
		}
	
		explicit source_file(const char* filename)
			: data_(filename)
		{
#ifdef WIN32
			const char* slash = strrchr(data_, '\\');
#else
			const char* slash = strrchr(data_, '/');
#endif
			if (slash)
			{
				data_ = slash + 1;
			}
			size_ = static_cast<size_t>(strlen(data_));
		}
	
		const char* data_;
		size_t      size_;
	};
	
	inline char const* time_now_string()
	{
		static char str[64] = { 0 };
		long usec = 0;
		struct tm t = datetime::now(&usec);
#if defined _MSC_VER
		sprintf_s(str, 63,
#else
		sprintf(str,
#endif
			"%04d-%02d-%02d %02d:%02d:%02d.%3ld", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, usec / 1000);

		return str;
	}

	struct empty_backend
	{
		void consume(const std::string&) { }
		void flush() { }
	};

	struct stdio_backend
	{
		void consume(const std::string& message) { std::cout << message << std::endl; }
		void flush() { std::cout.flush(); }
	};

	template <class BackendT>
	struct sync_frontend
	{
		typedef BackendT backend_type;

		sync_frontend(backend_type& backend)
			: backend_(backend)
		{ }

		void set_backend(backend_type& backend)
		{
			backend_ = backend;
		}

		void consume(const std::string& message)
		{
			backend_.consume(message);
		}

		void flush(const std::string& message)
		{
			backend_.flush();
		}

	protected:
		backend_type& backend_;
	};

	template <class FrontendT>
	struct logger
	{
		typedef FrontendT frontend_type;
		typedef typename frontend_type::backend_type backend_type;
		
		logger()
			: backend_()
			, frontend_(backend_)
		{ }

		void set_frontend(frontend_type&& frontend)
		{
			frontend_ = frontend;
			frontend_.set_backend(backend_);
		}

		void set_backend(backend_type&& backend)
		{
			backend_ = backend;
			frontend_.set_backend(backend_);
		}

		void consume(const std::string& message)
		{
			frontend_.consume(message);
		}

		void flush()
		{
			frontend_.flush();
		}

	protected:
		backend_type backend_;
		frontend_type frontend_;
	};

	typedef sync_frontend<stdio_backend> stdio_frontend;
	typedef logger<stdio_frontend> stdio_logger;

	template <class LoggerT>
	struct record_pump
	{
		typedef LoggerT logger_type;
	
		explicit record_pump(logger_type& logger, const source_file& file, size_t line, size_t lvl)
			: logger_(std::addressof(logger))
			, stream_(new std::stringstream)
		{
			static const char* s_lvlstring[] = {
				"debug",
				"info ",
				"warn ",
				"error",
				"fatal"
			};
			*stream_ << "[" << time_now_string() << "] [" << s_lvlstring[lvl] << "] [" << file.data_ << ":" << line << "] ";
		}
	
		record_pump(record_pump&& that)
			: logger_(that.logger_)
			, stream_(that.stream_.release())
		{
			that.logger_ = 0;
		}
	
		~record_pump()
		{
			if (logger_ && stream_)
			{
				std::string str = stream_->str();
				while (!str.empty() && isspace((int)(unsigned char)str.back())) str.pop_back();
				logger_->consume(str);
			}
		}
	
		std::stringstream& stream()
		{
			return *stream_;
		}
	
	protected:
		logger_type*      logger_;
		std::unique_ptr<std::stringstream> stream_;

	private:
		record_pump(const record_pump&);
		const record_pump& operator=(const record_pump&);
	};

	template <class LoggerT>
	inline record_pump<LoggerT> make_record_pump(LoggerT& logger, const source_file& file, size_t line, size_t lvl)
	{
		return record_pump<LoggerT>(logger, file, line, lvl);
	}
	
	template <class LoggerT>
	struct global
	{
		typedef LoggerT logger_type;
		static logger_type value;
	};

	template <class LoggerT> LoggerT global<LoggerT>::value
#if !defined _MSC_VER
		= {}
#endif
	;
}}

#define NETLOG_SYNC_FRONTEND ::net::log::sync_frontend	
#define NETLOG_STDIO_BACKEND ::net::log::stdio_backend
#define NETLOG_EMPTY_BACKEND ::net::log::empty_backend

#ifndef NETLOG_FRONTEND
#	define NETLOG_FRONTEND NETLOG_SYNC_FRONTEND
#endif

#ifndef NETLOG_BACKEND
#	define NETLOG_BACKEND NETLOG_STDIO_BACKEND
#endif		 

#define NETLOG_STREAM(lg, lvl) for (bool net_f = true; net_f; net_f = false) ::net::log::make_record_pump((lg), __FILE__, __LINE__, (lvl)).stream()
#define NETLOG_LOGGER ::net::log::global<::net::log::logger<NETLOG_FRONTEND<NETLOG_BACKEND>>>::value 
#define NETLOG_DEBUG()   NETLOG_STREAM(NETLOG_LOGGER, ::net::log::debug)
#define NETLOG_INFO()    NETLOG_STREAM(NETLOG_LOGGER, ::net::log::info)
#define NETLOG_WARNING() NETLOG_STREAM(NETLOG_LOGGER, ::net::log::warning)
#define NETLOG_ERROR()   NETLOG_STREAM(NETLOG_LOGGER, ::net::log::error)
#define NETLOG_FATAL()   NETLOG_STREAM(NETLOG_LOGGER, ::net::log::fatal)
