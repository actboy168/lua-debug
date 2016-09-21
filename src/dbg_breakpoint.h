#pragma once

#include <map> 
#include <set>
#include <string>				
#include "dbg_hybridarray.h"  

namespace vscode
{
	class breakpoint
		: private std::map<std::string, std::set<size_t>>
	{
	public:
		typedef std::map<std::string, std::set<size_t>> base_type;

	public:
		breakpoint()
			: base_type()
			, fast_table_()
		{
			fast_table_.fill(0);
		}

		void clear()
		{
			base_type::clear();
			fast_table_.clear();
		}

		void clear(const std::string& path)
		{
			auto it = find(path);
			if (it == end())
			{
				return;
			}
			auto& lines = it->second;
			for (size_t line : lines)
			{
				fast_table_[line]--;
			}
			lines.clear();
		}

		void insert(const std::string& path, size_t line)
		{
			auto it = find(path);
			if (it == end())
			{
				base_type::insert(std::make_pair(path, std::set<size_t> { line }));
			}
			else
			{
				auto& lines = it->second;
				if (lines.find(line) == lines.end())
				{
					lines.insert(line);
				}
				else
				{
					return;
				}
			}

			if (line >= fast_table_.size())
			{
				size_t oldsize = fast_table_.size();
				size_t newsize = line + 1;
				fast_table_.resize(newsize);
				std::fill_n(fast_table_.begin() + oldsize, newsize - oldsize, 0);
			}
			fast_table_[line]++;
		}

		bool has(size_t line) const
		{
			if (line < fast_table_.size())
			{
				return fast_table_[line] > 0;
			}
			return false;
		}

		bool has(const std::string& path, size_t line) const
		{
			auto it = find(path);
			if (it == end())
			{
				return false;
			}
			auto& lines = it->second;
			return lines.find(line) != lines.end();
		}

	private:
		hybridarray<size_t, 1024> fast_table_;
	};
}
