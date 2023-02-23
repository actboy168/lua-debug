#include "frida-gum.h"

#include <Windows.h>

#include <memory>
#include <functional>

static struct GumContext_Guard {
	GumContext_Guard() {
		gum_init_embedded();
	}
	~GumContext_Guard() {
		gum_deinit_embedded();
	}
}GumContext;

typedef struct _ExampleListener ExampleListener;
typedef enum _ExampleHookId ExampleHookId;

struct _ExampleListener
{
	GObject parent;

	guint num_calls;
};

enum _ExampleHookId
{
	EXAMPLE_HOOK_MESSAGE_BEEP,
	EXAMPLE_HOOK_SLEEP
};

#define EXAMPLE_TYPE_LISTENER (example_listener_get_type ())
G_DECLARE_FINAL_TYPE(ExampleListener, example_listener, EXAMPLE, LISTENER, GObject)
G_DEFINE_TYPE_EXTENDED(ExampleListener,
	example_listener,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE(GUM_TYPE_INVOCATION_LISTENER,
		example_listener_iface_init))

	using gobject_shared_ptr_t = std::shared_ptr<void>;

template<typename T>
std::shared_ptr<T> object_new(gpointer p) {
	return std::static_pointer_cast<T>(std::shared_ptr<void>{p, g_object_unref});
}

struct InvocationListener {
	virtual ~InvocationListener() = default;
};

struct Interceptor {
	virtual ~Interceptor() = default;
	virtual std::unique_ptr<InvocationListener> begin_transaction() = 0;
	virtual void end_transaction() = 0;
	virtual void attach(void* addr, void* userdata) = 0;
};

struct InvocationListenerImpl : InvocationListener {
	GumInvocationListener* handler = (GumInvocationListener*)g_object_new(EXAMPLE_TYPE_LISTENER, NULL);
	GumInterceptor* handler;
	InvocationListenerImpl() {

	}
	~InvocationListenerImpl() {
		g_object_unref(handler);
	}
};

struct InterceptorImpl : Interceptor {
	GumInterceptor* handler = gum_interceptor_obtain();
	InvocationListenerImpl* listener = nullptr;
	InterceptorImpl() {
	}
	~InterceptorImpl() override  {
		gum_interceptor_detach(handler, listener->handler);
		g_object_unref(handler);
	}

	std::unique_ptr<InvocationListener> begin_transaction() override {
		auto listener = std::make_unique<InvocationListenerImpl>();
		gum_interceptor_begin_transaction(handler);
	}

	void attach(void* addr, void* userdata) override {
		gum_interceptor_attach(handler, addr, listener->handler, userdata);
	}

	void end_transaction() override {
		gum_interceptor_end_transaction(handler);
	}
};

extern void* module_find_export_by_name(const char* image_name, const char* symbol_name) {
	return GSIZE_TO_POINTER(gum_module_find_export_by_name(image_name, symbol_name));
}

int test()
{
	GumInterceptor* interceptor;
	GumInvocationListener* listener;

	interceptor = gum_interceptor_obtain();
	listener = (GumInvocationListener*)g_object_new(EXAMPLE_TYPE_LISTENER, NULL);

	gum_interceptor_begin_transaction(interceptor);
	gum_interceptor_attach(interceptor,
		GSIZE_TO_POINTER(gum_module_find_export_by_name("user32.dll", "MessageBeep")),
		listener,
		GSIZE_TO_POINTER(EXAMPLE_HOOK_MESSAGE_BEEP));
	gum_interceptor_attach(interceptor,
		GSIZE_TO_POINTER(gum_module_find_export_by_name("kernel32.dll", "Sleep")),
		listener,
		GSIZE_TO_POINTER(EXAMPLE_HOOK_SLEEP));
	gum_interceptor_end_transaction(interceptor);

	MessageBeep(MB_ICONINFORMATION);
	Sleep(1);

	g_print("[*] listener got %u calls\n", EXAMPLE_LISTENER(listener)->num_calls);

	gum_interceptor_detach(interceptor, listener);

	MessageBeep(MB_ICONINFORMATION);
	Sleep(1);

	g_print("[*] listener still has %u calls\n", EXAMPLE_LISTENER(listener)->num_calls);

	g_object_unref(listener);
	g_object_unref(interceptor);

	return 0;
}

static void
example_listener_on_enter(GumInvocationListener* listener,
	GumInvocationContext* ic)
{
	ExampleListener* self = EXAMPLE_LISTENER(listener);
	ExampleHookId hook_id = GUM_IC_GET_FUNC_DATA(ic, ExampleHookId);

	switch (hook_id)
	{
	case EXAMPLE_HOOK_MESSAGE_BEEP:
		g_print("[*] MessageBeep(%u)\n", GPOINTER_TO_UINT(gum_invocation_context_get_nth_argument(ic, 0)));
		break;
	case EXAMPLE_HOOK_SLEEP:
		g_print("[*] Sleep(%u)\n", GPOINTER_TO_UINT(gum_invocation_context_get_nth_argument(ic, 0)));
		break;
	}

	self->num_calls++;
}

static void
example_listener_on_leave(GumInvocationListener* listener,
	GumInvocationContext* ic)
{

}

static void
example_listener_class_init(ExampleListenerClass* klass)
{
	(void)EXAMPLE_IS_LISTENER;
#ifndef _MSC_VER
	(void) glib_autoptr_cleanup_ExampleListener;
#endif
}

static void
example_listener_iface_init(gpointer g_iface,
	gpointer iface_data)
{
	auto iface = (GumInvocationListenerInterface*)g_iface;

	iface->on_enter = example_listener_on_enter;
	iface->on_leave = example_listener_on_leave;
}

static void
example_listener_init(ExampleListener* self)
{
}
