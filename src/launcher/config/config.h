#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace luadebug::autoattach {

    enum class lua_version;
    class Config {
        std::unique_ptr<nlohmann::json> values;

    public:
        lua_version get_lua_version() const;

        std::string get_lua_module() const;

        static bool init_from_file();
    };

    extern Config config;
}  // namespace luadebug::autoattach