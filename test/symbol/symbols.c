static const char* static_data= "static_data";
const char* extern_data = "data2";


static void static_function(const char*,...) {}
__attribute__((visibility("hidden"))) extern void hidden_extern_function(const char*,...) {static_function(static_data);}
__attribute__((visibility("default"))) extern void extern_function(const char*,...) {static_function(static_data);}