#include <gumpp.hpp>
namespace autoattach {

struct luajit_global_listener : Gum::NoLeaveInvocationListener {
  virtual ~luajit_global_listener() = default;
  virtual void on_enter(Gum::InvocationContext* context) override;
};

struct luajit_jit_listener : Gum::NoLeaveInvocationListener {
  virtual ~luajit_jit_listener() = default;
  virtual void on_enter(Gum::InvocationContext* context) override;
};

}  // namespace autoattach