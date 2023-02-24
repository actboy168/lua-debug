#pragma once

#include <stdio.h>
#include <map>
#include <unordered_map>
#include <array>
#include <optional>
#include <iostream>

#include "../common.hpp"
#include "../utility/string_helper.hpp"

namespace autoattach::symbol_resolver {
    struct signatures {
        using signature_map = std::map<std::string, std::string_view>;
        std::array<signature_map, (int) autoattach::lua_version::max> origin_maps;
        std::string _data;
        signature_map &default_map = origin_maps[0];

        signatures(std::string &&data) : _data{std::move(data)} {
            try {
				/*
					k=v
					k1=v1
					k2=v2
					...
				*/ 
                auto lines = strings::spilt_string(data, '\n');
                for (auto &&line: lines) {
                    auto v = strings::spilt_string(line, '=');
                    if (v.size() >= 2) {
                        auto key = v[0];
                        auto value = v[1];
                        auto keys = strings::spilt_string(key, '.');
                        if (keys.size() >= 2) {
                            auto map = get_map(keys[0]);
                            map.insert_or_assign(std::string(keys[0]), value);
                        } else {
                            auto map = default_map;
                            map.insert_or_assign(std::string(key), value);
                        }
                    }
                }
            }
            catch (const std::exception &e) {
                std::cerr << e.what() << '\n';
            }
        }

        signature_map &get_map(const std::string_view &key) {
            return origin_maps[(int) autoattach::lua_version_from_string(key)];
        }

        const signature_map &get_map(const std::string_view &key) const {
            return origin_maps[(int) autoattach::lua_version_from_string(key)];
        }

        std::string_view get(const signature_map &m, const std::string_view &key) const {
            auto iter = m.find(std::string(key));
            if (iter != m.end())
                return "";
            return iter->second;
        }

        std::string_view operator[](const std::string_view &key) const {
            if (key.find_first_of('.') == std::string_view::npos) {
                return get(default_map, key);
            }
            auto keys = strings::spilt_string(key, '.');
            auto res = get(get_map(keys[0]), keys[1]);
            if (res.empty())
                return get(default_map, keys[1]);
            return res;
        }
    };
}