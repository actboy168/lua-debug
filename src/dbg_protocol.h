#pragma once

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace vscode
{
	class rprotocol
		: public rapidjson::Document
	{
	public:
		rprotocol()
			: rapidjson::Document()
		{ }

		rprotocol(rapidjson::Document&& d)
			: rapidjson::Document(std::move(d))
		{ }

		rprotocol(rprotocol&& that)
			: rapidjson::Document(std::forward<rapidjson::Document>(that))
		{ }

		rprotocol& operator=(rprotocol&& that)
		{
			if (this == &that) {
				return *this;
			}
			static_cast<rapidjson::Document*>(this)->operator=(std::forward<rapidjson::Document>(that));
			return *this;
		}
	};

	class wprotocol
		: public rapidjson::Writer < rapidjson::StringBuffer >
	{
	public:
		typedef rapidjson::Writer<rapidjson::StringBuffer> base_type;

		wprotocol()
			: buf_()
			, base_type(buf_)
		{
		}

		const char* data() const
		{
			return buf_.GetString();
		}

		size_t size() const
		{
			return buf_.GetSize();
		}

		template <size_t n>
		wprotocol& operator() (const char(&str)[n])
		{
			base_type::Key(str, n - 1);
			return *this;
		}

		bool String(const rapidjson::Value& str)
		{
			return base_type::String(str.GetString(), str.GetStringLength());
		}

		bool String(const char* str)
		{
			return base_type::String(str);
		}

		template <size_t n>
		bool String(const char(&str)[n])
		{
			return base_type::String(str, n - 1);
		}

		template <class T>
		bool String(const T& str)
		{
			return base_type::String(str.data(), str.size());
		}

		typedef bool (wprotocol::*ItorT)();

		template <ItorT BeginT, ItorT EndT>
		struct iterator
		{
			iterator(wprotocol* self, size_t n)
				: self_(self)
				, n_(n)
			{
			}

			~iterator()
			{
				if (self_)
					(self_->*EndT)();
			}

			size_t operator* () const
			{
				return n_;
			}

			bool operator!= (const iterator& that) const
			{
				return n_ != that.n_;
			}

			bool operator== (const iterator& that) const
			{
				return n_ == that.n_;
			}

			iterator& operator++ ()
			{
				++n_;
				return *this;
			}

			const iterator& operator++ (int)
			{
				object_itor old = *this;
				++(*this);
				return old;
			}

			wprotocol* self_;
			size_t     n_;
		};

		template <ItorT BeginT, ItorT EndT>
		struct range
		{
			range(wprotocol* self, size_t n)
				: self_(self)
				, n_(n)
			{
			}

			iterator<BeginT, EndT> begin()
			{
				(self_->*BeginT)();
				return iterator<BeginT, EndT>(self_, 0);
			}

			iterator<BeginT, EndT> end()
			{
				return iterator<BeginT, EndT>(nullptr, n_);
			}

			wprotocol* self_;
			size_t     n_;
		};

		bool ObjectBegin() { return base_type::StartObject(); }
		bool ObjectEnd() { return base_type::EndObject(); }
		bool ArrayBegin() { return base_type::StartArray(); }
		bool ArrayEnd() { return base_type::EndArray(); }

		typedef range<&wprotocol::ObjectBegin, &wprotocol::ObjectEnd> ObjectRangeT;
		typedef range<&wprotocol::ArrayBegin, &wprotocol::ArrayEnd> ArrayRangeT;

		ObjectRangeT Object() { return ObjectRangeT(this, 1); }
		ArrayRangeT  Array(size_t n = 1) { return ArrayRangeT(this, n); }

	private:
		rapidjson::StringBuffer buf_;
	};
}
