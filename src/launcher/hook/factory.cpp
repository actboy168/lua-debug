#include <vector>
#include <string_view>
#include <string>
#include <hook/vmhook_template.hpp>
#include <autoattach/lua_module.h>
#include <bee/nonstd/unreachable.h>

namespace luadebug::autoattach {

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
            for (auto&& hook : vmhooks) {
                if (hook->get_symbols(resolver)) {
                    vaild_vmhook = hook.get();
                    return true;
                }
            }
            return false;
        }
    };

    static struct hook_context
    {
        hook_context(lua_version version);
        Gum::RefPtr<Gum::Interceptor> interceptor = Gum::Interceptor_obtain();
        std::unique_ptr<vmhook_list> hooker_list = std::make_unique<vmhook_list>();
        vmhook_template* now_hook;
    }* context;

    attach_status attach_lua_vm(lua::state L);
    void attach_lua_Hooker(lua::state L, lua::debug ar) {
        if (!context)
            return; //TODO: maybe throw an exception
        auto now_hook = context->now_hook;
        switch (attach_lua_vm(L)) {
        case attach_status::fatal:
        case attach_status::success:
            //inject success disable hook
            now_hook->unhook();
            //TODO: how to free so
            //TODO: free all resources
            //delete context;
            //context = nullptr;
            break;
        case attach_status::wait:
            break;
        default:
            std::unreachable();
        }
    }

    static std::vector<watch_point> get_watch_points(lua_version version) {
        switch (version) {
        case lua_version::luajit:
            return {
                {watch_point::type::common, "lj_dispatch_ins"},
                {watch_point::type::common, "lj_dispatch_call"},
                {watch_point::type::common, "lj_vm_call"},
                {watch_point::type::common, "lj_vm_pcall"},
                {watch_point::type::common, "lj_vm_cpcall"},
                {watch_point::type::common, "lj_vm_resume"},
                {watch_point::type::common, "lj_meta_tget"},
                {watch_point::type::luajit_global, "lj_dispatch_update"},
                {watch_point::type::luajit_jit, "lj_dispatch_stitch"},
            };
        case lua_version::lua51:
            return {
                {watch_point::type::common, "luaD_precall"},
                {watch_point::type::common, "luaV_gettable"},
                {watch_point::type::common, "luaV_settable"},
            };
        case lua_version::lua54:
            return {
                {watch_point::type::common, "luaD_poscall"},
                {watch_point::type::common, "luaH_finishset"},
                {watch_point::type::common, "luaV_finishget"},
            };
        default:
            return {
                {watch_point::type::common, "lua_settop"},
                {watch_point::type::common, "luaL_openlibs"},
                {watch_point::type::common, "lua_newthread"},
                {watch_point::type::common, "lua_close"},
                {watch_point::type::common, "lua_type"},
                {watch_point::type::common, "lua_pushnil"},
                {watch_point::type::common, "lua_pushnumber"},
                {watch_point::type::common, "lua_pushlstring"},
                {watch_point::type::common, "lua_pushboolean"},
            };
        }
    }

    void init_wathers(vmhook_template* hk, lua_version version) {
        hk->wather_points = get_watch_points(version);
    }
    
    hook_context::hook_context(lua_version version) {
        auto real_hooker = std::make_unique<vmhook_template>(interceptor);
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
