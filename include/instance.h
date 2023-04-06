#ifndef __INSTANCE_H
#define __INSTANCE_H

#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include "object.h"

#define type_sizeof(type_name) sizeof (*(type_name)NULL)
#define stubcode_to_malloc_ptr(code_ptr) \
  ((void *)((pbyte_t)(code_ptr) - sizeof (__generic_preamble)))

#define PAGESIZE (sysconf (_SC_PAGESIZE))
#define PAGE_START(P) ((uintptr_t)(P) & ~(PAGESIZE-1))
#define PAGE_END(P)   (((uintptr_t)(P) + PAGESIZE - 1) & ~(PAGESIZE-1))
#define PREAMBLE_STRUCT(instance, method_entry) \
  struct \
  { \
    typeof (instance) self; \
    typeof (method_entry->method) method; \
    char function_bytes[0]; \
  }
#define GENERIC_PREAMBLE_STRUCT \
  PREAMBLE_STRUCT(NULL, ((struct { void* method; } *)NULL) )

GENERIC_PREAMBLE_STRUCT __generic_preamble;

static void
default_type_ctor (void* self)
{ (void)self; }

#include <stdio.h>

static void
default_type_dtor (class_footer_t self)
{
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic push
  method_entry_t *method_entries
    = (pbyte_t)self->type_base + self->method_list_offset;
  method_entry_t *spec_method_entries = (pbyte_t)&self->cdtors;
#pragma GCC diagnostic pop
  /* free regular method stubs */
  for (size_t i = 0; i < self->nr_methods; ++i)
    if (!method_entries[i].special)
      free (stubcode_to_malloc_ptr(method_entries[i].method));
  size_t nr_spec_methods = (ptrdiff_t)(
    self->cdtor_table_header - self->cdtor_table_footer
  ) / sizeof (method_entry_t);
  for (size_t i = 0; i < nr_spec_methods; ++i)
    free (stubcode_to_malloc_ptr(spec_method_entries[i].method));
}

/* internal macros */
#define __rwx_heap_permissions_at(object, size) \
  ({ \
    mprotect ( \
      (void*)PAGE_START(object), \
      PAGE_END(object + size) - PAGE_START(object), \
      PROT_READ | PROT_WRITE | PROT_EXEC \
    ); \
  })

#define __allocate_dynamic_stub(instance, method_entry) \
  ({ \
    ptrdiff_t stub_size = __stop_trampoline_stub - __start_trampoline_stub; \
    size_t preamble_size = sizeof (PREAMBLE_STRUCT(instance, method_entry)); \
    auto stub_closure = (PREAMBLE_STRUCT(instance, method_entry)*) \
                        calloc (1, preamble_size + stub_size); \
    memcpy ( \
      &stub_closure->function_bytes, trampoline_hook_stub, stub_size \
    ); \
    auto preamble = (PREAMBLE_STRUCT(instance, method_entry)){ \
      .self = instance, \
      .method = method_entry->method \
    }; \
    if (__rwx_heap_permissions_at (stub_closure, sizeof (preamble) + stub_size)) \
      { \
        fprintf (stderr, "failed to set heap permissions\n"); \
        exit (EXIT_FAILURE); \
      } \
    (PREAMBLE_STRUCT(instance, method_entry) *) \
      memcpy (stub_closure, &preamble, preamble_size); \
  })

#define __create_trampoline_hook(instance, method_entry) \
  ({ \
    auto stub = __allocate_dynamic_stub (instance, method_entry); \
    method_entry->method = (typeof (method_entry->method))&stub->function_bytes; \
  })

#define __initialize_base_ctor(instance) \
  ({ \
    get_cdtor_table(instance).ctor = default_type_ctor; \
    auto ctor = __get_cdtor_method_entry (instance, ctor); \
    ctor->special = true; \
    ctor->identifier = BASE_CTOR_METHOD_NAME; \
    __create_trampoline_hook (instance, ctor); \
  })

#define __initialize_base_dtor(instance) \
  ({ \
    get_cdtor_table(instance).dtor = default_type_dtor; \
    auto dtor = __get_cdtor_method_entry (instance, dtor); \
    dtor->special = true; \
    dtor->identifier = BASE_DTOR_METHOD_NAME; \
    __create_trampoline_hook (instance, dtor); \
  })

/* type construction macros */
#define new_type(type_name) \
  ({ \
    auto instance = (type_name)malloc (type_sizeof (type_name)); \
    memcpy ( \
      instance, &BASE_CLASS_NAME(type_name), \
      sizeof (struct BASE_STRUCT_NAME(type_name) \
    )); \
    instance->METADATA_ATTRIB.type_base = instance; \
    for (size_t i = 0; i < instance->METADATA_ATTRIB.nr_methods; ++i) \
    { \
      auto entry = __get_nth_method_entry (instance, i); \
      __create_trampoline_hook (instance, entry); \
      if (__builtin_expect (!strcmp (entry->identifier, CTOR_METHOD_NAME), 0)) \
        get_cdtor_table(instance).ctor = entry->method; \
      else if (__builtin_expect ( \
                !strcmp (entry->identifier, DTOR_METHOD_NAME), 0 \
              )) \
        get_cdtor_table(instance).dtor = entry->method; \
    } \
    auto ctor = get_cdtor_table(instance).ctor; \
    auto dtor = get_cdtor_table(instance).dtor; \
    if (ctor) \
      ctor (); \
    else \
      { \
        __initialize_base_ctor (instance); \
        get_cdtor_table(instance).ctor (); \
      } \
    if (dtor == NULL) \
      __initialize_base_dtor (instance); \
    instance; \
  })

#define free_type(instance) \
  ({ \
    get_cdtor_table(instance).dtor (); \
    free (instance); \
  })

extern unsigned char __start_trampoline_stub[];
extern unsigned char __stop_trampoline_stub[];

static void
__attribute__((naked, noinline, section("trampoline_stub")))
trampoline_hook_stub (void) {asm (
  "1: lea 1b(%%rip), %%r15;\n\t"
  "subq %[preamble_size], %%r15;\n\t"
  "movl %%r8d, %%r9d;\n\t"
  "movq %%rcx, %%r8;\n\t"
  "movq %%rdx, %%rcx;\n\t"
  "movq %%rsi, %%rdx;\n\t"
  "movq %%rdi, %%rsi;\n\t"
  "movq (%%r15), %%rdi;\n\t"
  "addq %[method_offset], %%r15;\n\t"
  "jmp *(%%r15);\n\t"
  :: [preamble_size] "n" (sizeof (__generic_preamble)),
     [method_offset] "n" (offsetof (typeof (__generic_preamble), method))
);}

#endif /* __INSTANCE_H */