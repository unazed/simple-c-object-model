#ifndef __OBJECT_H
#define __OBJECT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#define auto __auto_type

/* naming macros */
#define BASE_STRUCT_NAME(type_name) __base_##type_name
#define BASE_CLASS_NAME(type_name) __cls_##type_name

#define METHOD_METADATA_ATTRIB(method_name) __metadata_##method_name
#define METHOD_METADATA_STRUCT \
  struct \
  { \
    char* identifier; \
    bool special; \
  }

#define GENERIC_METHOD_ENTRY \
  struct \
  { \
    METHOD_METADATA_STRUCT; \
    fptr_t method; \
  }

#define BASIC_METHOD_ENTRY(method_name) \
  struct \
  { \
    METHOD_METADATA_STRUCT METHOD_METADATA_ATTRIB(method_name); \
    void (*method_name)(); \
  }

#define INTERNAL_METADATA_STRUCT \
  struct \
  { \
    void* type_base; \
    const char* name; \
    uint16_t method_list_offset; \
    uint16_t nr_methods; \
    char cdtor_table_footer[0]; \
    struct \
    { \
      BASIC_METHOD_ENTRY(ctor); \
      BASIC_METHOD_ENTRY(dtor); \
    } CTOR_DTOR_TABLE; \
    char cdtor_table_header[0]; \
  }

#define METHOD_LIST_FWD_DECL_TYPE_NAME(type_name) \
  struct __method_fwd_decl_##type_name
#define METADATA_ATTRIB __cls_metadata
#define CTOR_DTOR_TABLE cdtors

#define get_cdtor_table(instance) (instance)->METADATA_ATTRIB.CTOR_DTOR_TABLE
#define get_method_metadata(instance, method_name) \
  (instance)->METHOD_METADATA_ATTRIB(method_name)
  
#define CTOR_METHOD_NAME ("init")
#define BASE_CTOR_METHOD_NAME ("base_init")
#define DTOR_METHOD_NAME ("deinit")
#define BASE_DTOR_METHOD_NAME ("base_deinit")

#define TYPE_METHOD_DECL_NAME(type_name, method_name) \
  __##type_name##_##method_name
#define TYPE_METHOD_CTOR_DECL(type_name, method_name) \
  __attribute__((constructor)) void __ctor_##type_name##_##method_name (void)

/* it might be preferable to use zero-length arrays */
#define METHOD_LIST_STUB(name) __cls_method_##name
#define METHOD_LIST_STUB_TYPE const void*

#define CLS_CTOR_TYPE void (*)()
#define CLS_DTOR_TYPE void (*)()

typedef void (*fptr_t)();
typedef uint8_t* pbyte_t;
typedef INTERNAL_METADATA_STRUCT *class_footer_t;
typedef GENERIC_METHOD_ENTRY method_entry_t;

/* internal macros */
#define __forward_decl_method_list(type_name, struct_) \
  METHOD_LIST_FWD_DECL_TYPE_NAME(type_name) struct_

#define __define_internal_metadata() \
  INTERNAL_METADATA_STRUCT METADATA_ATTRIB

#define __define_method_metadata(method_name) \
  METHOD_METADATA_STRUCT METHOD_METADATA_ATTRIB(method_name)

#define __define_method_list_footer() \
  METHOD_LIST_STUB_TYPE METHOD_LIST_STUB(footer)

#define __define_method_list_header() \
  METHOD_LIST_STUB_TYPE METHOD_LIST_STUB(header)

#define __get_nth_method_entry(instance, index) \
  ((GENERIC_METHOD_ENTRY *)( \
    (pbyte_t)instance \
    + instance->METADATA_ATTRIB.method_list_offset \
    + sizeof (GENERIC_METHOD_ENTRY) * index \
  ))

#define __get_cdtor_method_entry(instance, which) \
  (GENERIC_METHOD_ENTRY *)( \
    (pbyte_t)&get_cdtor_table(instance).which - sizeof (METHOD_METADATA_STRUCT) \
  )

/* type definition macros */
#define define_method(ret_type, method_name, ...) \
  struct \
  { \
    __define_method_metadata(method_name); \
    ret_type (*method_name)(__VA_ARGS__); \
  }

#define define_type_methods(method_type_list) \
  method_type_list

#define create_type_from_struct(struct_, method_struct, type_name) \
  __forward_decl_method_list(type_name, method_struct); \
  typedef struct BASE_STRUCT_NAME(type_name) \
  { \
    __define_internal_metadata (); \
    struct struct_; \
    __define_method_list_footer (); \
    struct method_struct __attribute__((packed)); \
    __define_method_list_header (); \
  } *type_name; \
  static struct BASE_STRUCT_NAME(type_name) BASE_CLASS_NAME(type_name) = { \
    .METADATA_ATTRIB.name = #type_name, \
    .METADATA_ATTRIB.method_list_offset \
      = offsetof(struct BASE_STRUCT_NAME(type_name), METHOD_LIST_STUB(footer)) \
      + sizeof (METHOD_LIST_STUB_TYPE), \
    .METADATA_ATTRIB.nr_methods = ( \
      offsetof(struct BASE_STRUCT_NAME(type_name), METHOD_LIST_STUB(header)) \
      - offsetof(struct BASE_STRUCT_NAME(type_name), METHOD_LIST_STUB(footer)) \
      - sizeof (METHOD_LIST_STUB_TYPE) \
    ) / sizeof (GENERIC_METHOD_ENTRY), \
    .METADATA_ATTRIB.CTOR_DTOR_TABLE.ctor = NULL, \
    .METADATA_ATTRIB.CTOR_DTOR_TABLE.dtor = NULL \
  };

#define declare_type_method(type_name, ret_type, method_name) \
  ret_type TYPE_METHOD_DECL_NAME(type_name, method_name)(); \
  TYPE_METHOD_CTOR_DECL(type_name, method_name) \
  { \
    auto methods = (METHOD_LIST_FWD_DECL_TYPE_NAME(type_name)*)( \
      (pbyte_t)&BASE_CLASS_NAME(type_name) \
      + BASE_CLASS_NAME(type_name).METADATA_ATTRIB.method_list_offset \
    ); \
    if (!strcmp (CTOR_METHOD_NAME, #method_name)) \
      { \
        get_cdtor_table(&BASE_CLASS_NAME(type_name)).ctor \
          = &TYPE_METHOD_DECL_NAME(type_name, method_name); \
        get_cdtor_table(&BASE_CLASS_NAME(type_name)) \
          .METHOD_METADATA_ATTRIB(ctor).identifier = #method_name; \
        get_method_metadata(&BASE_CLASS_NAME(type_name), method_name) \
          .special = true; \
      } \
    else if (!strcmp (DTOR_METHOD_NAME, #method_name)) \
      { \
        get_cdtor_table(&BASE_CLASS_NAME(type_name)).dtor \
          = &TYPE_METHOD_DECL_NAME(type_name, method_name); \
        get_cdtor_table(&BASE_CLASS_NAME(type_name)) \
          .METHOD_METADATA_ATTRIB(dtor).identifier = #method_name; \
        get_method_metadata(&BASE_CLASS_NAME(type_name), method_name) \
          .special = true; \
      } \
    methods->method_name = &TYPE_METHOD_DECL_NAME(type_name, method_name); \
    get_method_metadata(&BASE_CLASS_NAME(type_name), method_name) \
      .identifier = #method_name; \
  } \
  ret_type TYPE_METHOD_DECL_NAME(type_name, method_name)

/* descriptive macros */
#define of_type(x) x

#endif /* __OBJECT_H */