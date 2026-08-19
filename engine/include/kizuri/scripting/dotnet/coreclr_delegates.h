


#ifndef HAVE_CORECLR_DELEGATES_H
#define HAVE_CORECLR_DELEGATES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_WIN32)
    #define CORECLR_DELEGATE_CALLTYPE __stdcall
    #ifdef _WCHAR_T_DEFINED
        typedef wchar_t char_t;
    #else
        typedef unsigned short char_t;
    #endif
#else
    #define CORECLR_DELEGATE_CALLTYPE
    typedef char char_t;
#endif

#define UNMANAGEDCALLERSONLY_METHOD ((const char_t*)-1)


typedef int (CORECLR_DELEGATE_CALLTYPE *load_assembly_and_get_function_pointer_fn)(
    const char_t *assembly_path      ,
    const char_t *type_name          ,
    const char_t *method_name        ,
    const char_t *delegate_type_name 

,
    void         *reserved           ,
     void **delegate          );


typedef int (CORECLR_DELEGATE_CALLTYPE *component_entry_point_fn)(void *arg, int32_t arg_size_in_bytes);

typedef int (CORECLR_DELEGATE_CALLTYPE *get_function_pointer_fn)(
    const char_t *type_name          ,
    const char_t *method_name        ,
    const char_t *delegate_type_name 

,
    void         *load_context       ,
    void         *reserved           ,
     void **delegate          );

typedef int (CORECLR_DELEGATE_CALLTYPE *load_assembly_fn)(
    const char_t *assembly_path     ,
    void         *load_context      ,
    void         *reserved          );

typedef int (CORECLR_DELEGATE_CALLTYPE *load_assembly_bytes_fn)(
    const void *assembly_bytes      ,
    size_t     assembly_bytes_len   ,
    const void *symbols_bytes       ,
    size_t     symbols_bytes_len    ,
    void       *load_context        ,
    void       *reserved            );

#ifdef __cplusplus
}
#endif 

#endif 
