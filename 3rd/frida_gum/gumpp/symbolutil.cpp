#include "gumpp.hpp"

#include "podwrapper.hpp"
#include "runtime.hpp"

#include <frida-gum.h>
#include "string.hpp"

namespace Gum
{
  class SymbolPtrArray : public PodWrapper<SymbolPtrArray, PtrArray, GArray>
  {
  public:
    SymbolPtrArray (GArray * arr)
    {
      assign_handle (arr);
    }

    virtual ~SymbolPtrArray ()
    {
      g_array_free (handle, TRUE);

      Runtime::unref ();
    }

    virtual int length ()
    {
      return handle->len;
    }

    virtual void * nth (int n)
    {
      return g_array_index (handle, gpointer, n);
    }
  };

  void * find_function_ptr (const char * name)
  {
    Runtime::ref ();
    void * result = gum_find_function (name);
    Runtime::unref ();
    return result;
  }
  PtrArray * find_matching_functions_array (const char * str)
  {
    Runtime::ref ();
    return new SymbolPtrArray (gum_find_functions_matching (str));
  }

  String* get_function_name_from_addr(void* addr) {
    Runtime::ref ();
    gchar* name = gum_symbol_name_from_address(addr);
    Runtime::unref ();
    return new StringImpl(name);
  }
}