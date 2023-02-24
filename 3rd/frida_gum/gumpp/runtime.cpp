#include "runtime.hpp"

#include <frida-gum.h>
#ifdef HAVE_WINDOWS
#include <windows.h>
#endif

namespace Gum
{
  volatile int Runtime::ref_count = 0;

  static void init ()
  {
    gum_init_embedded ();
  }

  static void deinit ()
  {
    gum_deinit_embedded ();
  }

#if defined(HAVE_WINDOWS) && !defined(GUMPP_STATIC)

  extern "C" BOOL WINAPI DllMain (HINSTANCE inst_dll, DWORD reason, LPVOID reserved)
  {
    switch (reason)
    {
      case DLL_PROCESS_ATTACH:
        init ();
        break;
      case DLL_PROCESS_DETACH:
        if (reserved == NULL)
          deinit ();
        break;
    }

    return TRUE;
  }

  void Runtime::ref ()
  {
  }

  void Runtime::unref ()
  {
  }

#else

  void Runtime::ref ()
  {
    if (g_atomic_int_add (&ref_count, 1) == 0)
      init ();
  }

  void Runtime::unref ()
  {
    if (g_atomic_int_dec_and_test (&ref_count))
      deinit ();
  }
#endif

  void runtime_init(){
    Runtime::ref();
  }
  void runtime_deinit(){
    Runtime::unref();
  }
}
