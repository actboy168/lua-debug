#include <gumpp.hpp>
#include <hook/vmhook_template.hpp>
namespace luadebug::autoattach {

struct luajit_global_listener : Gum::NoLeaveInvocationListener {
    vmhook_template* hooker;
    luajit_global_listener(vmhook_template* h) : hooker{h} {}
    virtual ~luajit_global_listener() = default;
    virtual void on_enter(Gum::InvocationContext* context) override;
};

struct luajit_jit_listener : Gum::NoLeaveInvocationListener {
    vmhook_template* hooker;
    luajit_jit_listener(vmhook_template* h) : hooker{h} {}
    virtual ~luajit_jit_listener() = default;
    virtual void on_enter(Gum::InvocationContext* context) override;
};

}
