#include <gumpp.hpp>
#include <hook/vm_watcher.hpp>
namespace luadebug::autoattach {

struct luajit_global_listener : Gum::NoLeaveInvocationListener {
    vm_watcher* watcher;
    luajit_global_listener(vm_watcher* w) : watcher{w} {}
    virtual ~luajit_global_listener() = default;
    virtual void on_enter(Gum::InvocationContext* context) override;
};

struct luajit_jit_listener : Gum::NoLeaveInvocationListener {
    vm_watcher* watcher;
    luajit_jit_listener(vm_watcher* w) : watcher{w} {}
    virtual ~luajit_jit_listener() = default;
    virtual void on_enter(Gum::InvocationContext* context) override;
};

}
