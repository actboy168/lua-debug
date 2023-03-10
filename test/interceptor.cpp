
#include <gumpp.hpp>

#include <stdio.h>
#include <string.h>

#define LOG(fmt, ...) printf("[test_native] " fmt "\n", ##__VA_ARGS__)

extern "C" {
    void test_add(int a, int b) {
		printf("%d,%d,%d", a, b, a + b);
		a = a + 1;
		b = b + 1;
		printf("%d,%d,%d", a,b,a+b);
    }
}
static int on_enter = 1;
static int on_leave = 2;
struct MyInvocationListener : Gum::InvocationListener{
   virtual ~MyInvocationListener () {}

    virtual void on_enter (Gum::InvocationContext * context){
  	  LOG("on_enter");
      ::on_enter = 0;
    };
    virtual void on_leave (Gum::InvocationContext * context){
  	  LOG("on_leave");
      ::on_leave = 0;
    };
};

int main(int argc, char *argv[]) {
  LOG("test execve");
  Gum::RefPtr<Gum::Interceptor> interceptor (Gum::Interceptor_obtain ());
  MyInvocationListener listener;
  interceptor->attach((void*)&test_add, &listener, 0);

  test_add(1, 2);

  return on_enter + on_leave;
}
