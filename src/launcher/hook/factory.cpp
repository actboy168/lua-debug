
#include <vector>
#include <string_view>
#include <string>

#include <hook/vmhook_template.hpp>
namespace luadebug::autoattach{

    struct vmhook_list : public vmhook {
        std::vector<std::unique_ptr<vmhook>> vmhooks;
        vmhook* vaild_vmhook;
        ~vmhook_list() = default;

        bool hook() override {
            if (vaild_vmhook) 
                return vaild_vmhook->hook();
            return false;
        }

        void unhook() override {
            if (vaild_vmhook) 
                vaild_vmhook->unhook();
        }

        bool get_symbols(const lua::resolver& resolver) override {
            for (auto&& hook : vmhooks){
                if (hook->get_symbols(resolver)) {
                    vaild_vmhook = hook.get();
                    return true;
                }
            }
            return false;
        }
    };

    static std::vector <std::string_view> get_hook_entry_points(lua_version version) {
        switch (version) {
            case lua_version::luajit:
                return {
                        "lj_dispatch_ins",
                        "lj_dispatch_call",
                        "lj_vm_call",
                        "lj_vm_pcall",
                        "lj_vm_cpcall",
                        "lj_vm_resume",
                        "lj_meta_tget",
                };
            case lua_version::lua51:
                return {
                        "luaD_precall",
                        "luaV_gettable",
                        "luaV_settable"
                };
            case lua_version::lua54:
                return {
                        "luaD_poscall",
                        "luaH_finishset",
                        "luaV_finishget",
                };
            default:
                return {
                        "lua_settop",
                        "luaL_openlibs",
                        "lua_newthread",
                        "lua_close",
                        "lua_type",
                        "lua_pushnil",
                        "lua_pushnumber",
                        "lua_pushlstring",
                        "lua_pushboolean"
                };
        }
    }

    static struct hook_context
    {
        hook_context(lua_version version);
        Gum::RefPtr<Gum::Interceptor> interceptor = Gum::Interceptor_obtain();
        std::unique_ptr<vmhook_list> hooker_list = std::make_unique<vmhook_list>();
        vmhook_template* now_hook;
    }* context;
    
    static void attach_lua_Hooker(lua::state L, lua::debug ar) {
        if (context) {
            context->now_hook->hooker.call_origin_hook(L, ar);
            //inject success disable hook
            context->now_hook->unhook();
        }
        
        attach_lua_vm(L);
        //TODO: how to free so
        //TODO: free all resources
        //delete context;
        //context = nullptr;
    }

    
    std::unique_ptr<vmhook_template> create_luajit_vmhook(Gum::RefPtr<Gum::Interceptor>);

    void init_wathers(vmhook_template* hk, lua_version version) {
        hk->wather_points.clear();
        auto entry = get_hook_entry_points(version);
        for (auto &&e: entry) {
            hk->wather_points.emplace_back(watch_point{std::string{e}, 0});
        }
    }
    
    hook_context::hook_context(lua_version version) {
        auto real_hooker = version == lua_version::luajit ? create_luajit_vmhook(interceptor) : std::make_unique<vmhook_template>(interceptor);
        now_hook = real_hooker.get();

        init_wathers(real_hooker.get(), version);
        hooker_list->vmhooks.emplace_back(std::move(real_hooker));

        if (version != lua_version::unknown) {
            auto real_hooker = std::make_unique<vmhook_template>(interceptor);
            init_wathers(real_hooker.get(), lua_version::unknown);
            hooker_list->vmhooks.emplace_back(std::move(real_hooker));
        }
    }

    vmhook *create_vmhook(lua_version version) {
        context = new hook_context(version);
        return context->now_hook;
    }
}