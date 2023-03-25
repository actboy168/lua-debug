#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace luadebug::autoattach {

    enum class lua_version;
    struct signature;

    class Config {
        std::unique_ptr<nlohmann::json> values;

    public:
        lua_version get_lua_version() const;

        std::optional<signature> get_lua_signature(const std::string& key) const;

        std::string get_lua_module() const;

        bool is_signature_mode() const;

        static bool init_from_file();
    };

    extern Config config;
}  // namespace luadebug::autoattach