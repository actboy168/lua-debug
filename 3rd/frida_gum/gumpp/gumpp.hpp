#ifndef __GUMPP_HPP__
#define __GUMPP_HPP__

#if !defined (GUMPP_STATIC) && defined (WIN32)
#  ifdef GUMPP_EXPORTS
#    define GUMPP_API __declspec(dllexport)
#  else
#    define GUMPP_API __declspec(dllimport)
#  endif
#else
#  define GUMPP_API
#endif

#define GUMPP_CAPI extern "C" GUMPP_API

#define GUMPP_MAX_BACKTRACE_DEPTH 16
#define GUMPP_MAX_PATH            260
#define GUMPP_MAX_SYMBOL_NAME     2048

#include <cstddef>
#include <vector>
#include <memory>
#include <functional>
namespace Gum
{
  struct InvocationContext;
  struct InvocationListener;
  struct NoLeaveInvocationListener;
  struct CpuContext;
  struct ReturnAddressArray;

  struct Object
  {
    virtual ~Object () {}

    virtual void ref () = 0;
    virtual void unref () = 0;
    virtual void * get_handle () const = 0;
  };

  struct String : public Object
  {
    virtual const char * c_str () = 0;
    virtual size_t length () const = 0;
  };

  struct PtrArray : public Object
  {
    virtual int length () = 0;
    virtual void * nth (int n) = 0;
  };

  struct Interceptor : public Object
  {
    virtual bool attach (void * function_address, InvocationListener * listener, void * listener_function_data = 0) = 0;
    virtual void detach (InvocationListener * listener) = 0;

    virtual bool attach (void * function_address, NoLeaveInvocationListener * listener, void * listener_function_data = 0) = 0;
    virtual void detach (NoLeaveInvocationListener * listener) = 0;

    virtual void replace (void * function_address, void * replacement_address, void * replacement_data = 0, void** origin_function = 0) = 0;
    virtual void revert (void * function_address) = 0;

    virtual void begin_transaction () = 0;
    virtual void end_transaction () = 0;

    virtual InvocationContext * get_current_invocation () = 0;

    virtual void ignore_current_thread () = 0;
    virtual void unignore_current_thread () = 0;

    virtual void ignore_other_threads () = 0;
    virtual void unignore_other_threads () = 0;
  };

  Interceptor * Interceptor_obtain (void);

  struct InvocationContext
  {
    virtual ~InvocationContext () {}

    virtual void * get_function () const = 0;

    template <typename T>
    T get_nth_argument (unsigned int n) const
    {
      return reinterpret_cast<T> (get_nth_argument_ptr (n));
    }
    virtual void * get_nth_argument_ptr (unsigned int n) const = 0;
    virtual void replace_nth_argument (unsigned int n, void * value) = 0;
    template <typename T>
    T get_return_value () const
    {
      return static_cast<T> (get_return_value_ptr ());
    }
    virtual void * get_return_value_ptr () const = 0;

    virtual unsigned int get_thread_id () const = 0;

    template <typename T>
    T * get_listener_thread_data () const
    {
      return static_cast<T *> (get_listener_thread_data_ptr (sizeof (T)));
    }
    virtual void * get_listener_thread_data_ptr (size_t required_size) const = 0;
    template <typename T>
    T * get_listener_function_data () const
    {
      return static_cast<T *> (get_listener_function_data_ptr ());
    }
    virtual void * get_listener_function_data_ptr () const = 0;
    template <typename T>
    T * get_listener_invocation_data () const
    {
      return static_cast<T *> (get_listener_invocation_data_ptr (sizeof (T)));
    }
    virtual void * get_listener_invocation_data_ptr (size_t required_size) const = 0;

    template <typename T>
    T * get_replacement_data () const
    {
      return static_cast<T *> (get_replacement_data_ptr ());
    }
    virtual void * get_replacement_data_ptr () const = 0;

    virtual CpuContext * get_cpu_context () const = 0;
  };

  struct InvocationListener
  {
    virtual ~InvocationListener () {}

    virtual void on_enter (InvocationContext * context) = 0;
    virtual void on_leave (InvocationContext * context) = 0;
  };

  struct NoLeaveInvocationListener
  {
    virtual ~NoLeaveInvocationListener () {}

    virtual void on_enter (InvocationContext * context) = 0;
  };

  struct Backtracer : public Object
  {
    virtual void generate (const CpuContext * cpu_context, ReturnAddressArray & return_addresses) const = 0;
  };

  GUMPP_CAPI Backtracer * Backtracer_make_accurate ();
  GUMPP_CAPI Backtracer * Backtracer_make_fuzzy ();

  typedef void * ReturnAddress;

  struct ReturnAddressArray
  {
    unsigned int len;
    ReturnAddress items[GUMPP_MAX_BACKTRACE_DEPTH];
  };

  struct ReturnAddressDetails
  {
    ReturnAddress address;
    char module_name[GUMPP_MAX_PATH + 1];
    char function_name[GUMPP_MAX_SYMBOL_NAME + 1];
    char file_name[GUMPP_MAX_PATH + 1];
    unsigned int line_number;
    unsigned int column;
  };

  GUMPP_CAPI bool ReturnAddressDetails_from_address (ReturnAddress address, ReturnAddressDetails & details);

  void * find_function_ptr (const char * str);
  PtrArray * find_matching_functions_array (const char * str);
  String * get_function_name_from_addr(void* addr);

  template <typename T> class RefPtr
  {
  public:
    explicit RefPtr (T * ptr_) : ptr (ptr_) {}
    explicit RefPtr (const RefPtr<T> & other) : ptr (other.ptr)
    {
      if (ptr)
        ptr->ref ();
    }

    template <class U> RefPtr (const RefPtr<U> & other) : ptr (other.operator->())
    {
      if (ptr)
        ptr->ref ();
    }

    RefPtr () : ptr (0) {}

    bool is_null () const
    {
      return ptr == 0 || ptr->get_handle () == 0;
    }

    RefPtr & operator = (const RefPtr & other)
    {
      RefPtr tmp (other);
      swap (*this, tmp);
      return *this;
    }

    RefPtr & operator = (T * other)
    {
      RefPtr tmp (other);
      swap (*this, tmp);
      return *this;
    }

    T * operator-> () const
    {
      return ptr;
    }

    T & operator* () const
    {
      return *ptr;
    }

    operator T * ()
    {
      return ptr;
    }

    static void swap (RefPtr & a, RefPtr & b)
    {
      T * tmp = a.ptr;
      a.ptr = b.ptr;
      b.ptr = tmp;
    }

    ~RefPtr ()
    {
      if (ptr)
        ptr->unref ();
    }

  private:
    T * ptr;
  };
  struct ModuleDetails;
  class SymbolUtil
  {
  public:
    static void * find_function (const char * name)
    {
      return find_function_ptr (name);
    }

    static std::vector<void *> find_matching_functions (const char * str, bool skip_public_code)
    {
      RefPtr<PtrArray> functions = RefPtr<PtrArray> (find_matching_functions_array (str));
      std::vector<void *> result;
      result.reserve(functions->length());
      for (int i = functions->length () - 1; i >= 0; i--) {
        auto addr = functions->nth (i);
        if (!skip_public_code || !is_public_code(addr))
          result.push_back (addr);
      }

      return result;
    }
    static bool is_public_code(void* addr) {
#ifdef _WIN32
      uint8_t* byte = (uint8_t*)addr;
#if defined(_M_AMD64)
      if (byte[0] == 0xE9 || byte[0] == 0xEB)
        return true;
      if (byte[0] == 0xFF)
        if ((byte[1] & 0x07) == 4 || (byte[1] & 0x07) == 5)
            return true;
#elif defined(_M_IX86)
      if (byte[0] == 0xE9 || byte[0] == 0xEA || byte[0] == 0xEB)
        return true;
      if (byte[0] == 0xFF)
        if ((byte[1] & 0x07) == 4 || (byte[1] & 0x07) == 5)
            return true;
#else
#error "unsupport"
#endif
#endif
      return false;
    }
  };
  struct MemoryRange {
    void* base_address;
    size_t size;
  };

  struct ModuleDetails {
    virtual ~ModuleDetails() = default;
    virtual const char* name() const = 0;
    virtual MemoryRange range()  const= 0;
    virtual const char* path() const = 0;
  };
  
  using FoundModuleFunc = std::function<bool(const ModuleDetails& details)>;
  class Process {
  public:
    static void enumerate_modules(const FoundModuleFunc& func);
    static void* module_find_symbol_by_name(const char* module_name, const char* symbol_name);
  };

  void runtime_init();
  void runtime_deinit();
}

#endif
