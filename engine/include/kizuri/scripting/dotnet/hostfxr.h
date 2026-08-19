


#ifndef HAVE_HOSTFXR_H
#define HAVE_HOSTFXR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif 

#if defined(_WIN32)
    #define HOSTFXR_CALLTYPE __cdecl
    #ifdef _WCHAR_T_DEFINED
        typedef wchar_t char_t;
    #else
        typedef unsigned short char_t;
    #endif
#else
    #define HOSTFXR_CALLTYPE
    typedef char char_t;
#endif

enum hostfxr_delegate_type
{
    hdt_com_activation,
    hdt_load_in_memory_assembly,
    hdt_winrt_activation,
    hdt_com_register,
    hdt_com_unregister,
    hdt_load_assembly_and_get_function_pointer,
    hdt_get_function_pointer,
    hdt_load_assembly,
    hdt_load_assembly_bytes,
};

typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_main_fn)(const int argc, const char_t **argv);
typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_main_startupinfo_fn)(
    const int argc,
    const char_t **argv,
    const char_t *host_path,
    const char_t *dotnet_root,
    const char_t *app_path);
typedef int32_t(HOSTFXR_CALLTYPE* hostfxr_main_bundle_startupinfo_fn)(
    const int argc,
    const char_t** argv,
    const char_t* host_path,
    const char_t* dotnet_root,
    const char_t* app_path,
    int64_t bundle_header_offset);

typedef void(HOSTFXR_CALLTYPE *hostfxr_error_writer_fn)(const char_t *message);
























typedef hostfxr_error_writer_fn(HOSTFXR_CALLTYPE *hostfxr_set_error_writer_fn)(hostfxr_error_writer_fn error_writer);

typedef void* hostfxr_handle;
struct hostfxr_initialize_parameters
{
    size_t size;
    const char_t *host_path;
    const char_t *dotnet_root;
};




























typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_initialize_for_dotnet_command_line_fn)(
    int argc,
    const char_t **argv,
    const struct hostfxr_initialize_parameters *parameters,
     hostfxr_handle *host_context_handle);































typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_initialize_for_runtime_config_fn)(
    const char_t *runtime_config_path,
    const struct hostfxr_initialize_parameters *parameters,
     hostfxr_handle *host_context_handle);
























typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_get_runtime_property_value_fn)(
    const hostfxr_handle host_context_handle,
    const char_t *name,
     const char_t **value);




















typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_set_runtime_property_value_fn)(
    const hostfxr_handle host_context_handle,
    const char_t *name,
    const char_t *value);




























typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_get_runtime_properties_fn)(
    const hostfxr_handle host_context_handle,
     size_t * count,
     const char_t **keys,
     const char_t **values);















typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_run_app_fn)(const hostfxr_handle host_context_handle);






















typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_get_runtime_delegate_fn)(
    const hostfxr_handle host_context_handle,
    enum hostfxr_delegate_type type,
     void **delegate);











typedef int32_t(HOSTFXR_CALLTYPE *hostfxr_close_fn)(const hostfxr_handle host_context_handle);

struct hostfxr_dotnet_environment_sdk_info
{
    size_t size;
    const char_t* version;
    const char_t* path;
};

struct hostfxr_dotnet_environment_framework_info
{
    size_t size;
    const char_t* name;
    const char_t* version;
    const char_t* path;
};

struct hostfxr_dotnet_environment_info
{
    size_t size;

    const char_t* hostfxr_version;
    const char_t* hostfxr_commit_hash;

    size_t sdk_count;
    const struct hostfxr_dotnet_environment_sdk_info* sdks;

    size_t framework_count;
    const struct hostfxr_dotnet_environment_framework_info* frameworks;
};

typedef void(HOSTFXR_CALLTYPE* hostfxr_get_dotnet_environment_info_result_fn)(
    const struct hostfxr_dotnet_environment_info* info,
    void* result_context);




































typedef int32_t(HOSTFXR_CALLTYPE* hostfxr_get_dotnet_environment_info_fn)(
    const char_t* dotnet_root,
    void* reserved,
    hostfxr_get_dotnet_environment_info_result_fn result,
    void* result_context);

struct hostfxr_framework_result
{
    size_t size;
    const char_t* name;
    const char_t* requested_version;
    const char_t* resolved_version;
    const char_t* resolved_path;
};

struct hostfxr_resolve_frameworks_result
{
    size_t size;

    size_t resolved_count;
    const struct hostfxr_framework_result* resolved_frameworks;

    size_t unresolved_count;
    const struct hostfxr_framework_result* unresolved_frameworks;
};

typedef void (HOSTFXR_CALLTYPE* hostfxr_resolve_frameworks_result_fn)(
    const struct hostfxr_resolve_frameworks_result* result,
    void* result_context);























typedef int32_t(HOSTFXR_CALLTYPE* hostfxr_resolve_frameworks_for_runtime_config_fn)(
    const char_t* runtime_config_path,
     const struct hostfxr_initialize_parameters* parameters,
     hostfxr_resolve_frameworks_result_fn callback,
     void* result_context);

#ifdef __cplusplus
}
#endif 

#endif 
