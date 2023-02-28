#ifdef _WIN32
#define EXPORT_HIDDEN 
#define EXPORT __declspec( dllexport )
#else
#define EXPORT_HIDDEN __attribute__((visibility("hidden")))
#define EXPORT __attribute__((visibility("default")))
#endif
static const char* static_data= "static_data";
const char* extern_data = "data2";


static void static_function(const char* fmt,...) {}
EXPORT_HIDDEN extern void hidden_extern_function(const char* fmt,...) {static_function(static_data);}
EXPORT extern void extern_function(const char* fmt,...) {static_function(static_data);}