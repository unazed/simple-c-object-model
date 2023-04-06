# simple-c-object-model
tinker project to implement a custom object oriented system in c

## usage

Typical makefile build process; `make` generates the `bin/object` binary.
For (unrecommended) integration into your codebase, the entire project is header
-only, so you only need to include `instance.h` and continue to use the features
as demonstrated in `src/main.c`.

## purpose

As commented on in the tagline, this is just a tinker project designed by me to
better understand how to develop an object oriented system, and likely develop
my reverse engineering knowledge a little further by understanding fundamentals.

This is certainly not a statement against any object oriented languages, and my
ultimate recommendation is to use your favourite OOP-supporting language to
properly utilise object orientation.

I've purposely used macros almost exclusively because they allow redefining
language semantics, and allow for making subsequent C code look a little more
human readable when trying to write OOP.

## header api

`include/object.h` declares all the macros that involve creating an object 
(or type, as the code nomenclature goes.)

- `create_type_from_struct(struct_, method_struct, type_name)`
  This is the building block for the rest of the macros, it creates the initial
  structure that encompasses all the object metadata.

- `define_type_methods(method_type_list)`
  In the end, this became more of a descriptive macro, which just plants
  `method_type_list` as is, but had prior semantics to allow methods to be
  placed under a nesting member structure.

- `define_method(ret_type, method_name, ...)`
  To be used under `define_type_methods`: creates method entries in the internal
  object structure with corresponding metadata

- `declare_type_method(type_name, ret_type, method_name)`
  Used after a matching `create_type_from_struct` declaration to match object
  method definitions to their proper declarations. This semantic was a
  compromise to having declarations be done with the `define_method` macro
  because GCC doesn't support nested functions outside of block scopes,
  unfortunately.

`include/instance.h` declares all the instance-related macros to do with
run-time initialisation of objects, garbage collection and so forth. There are
only two primary macros:

- `new_type(type_name)`
  Creates a new object instance, and calls the initializer method (if defined.)
  Returns the instance object of `type_name`.

- `free_type(instance)`
  Calls the finalizer method of `instance` and destructs the object, and its
  stub methods.

## architectural analysis

I tried to keep a good habit of forwarding type definitions throughout the flow
of the object definition/initialisation/destruction phases, but in reality the
fact that the pivoting mechanism of the model depends on function pointers, and
dynamically generated stubs to forward the `self` argument to the class methods
clearly introduces a very poor type safety, and leans a lot on the end
programmer to keep type correctness.

The largest drawback of the macro-heavy system is the large preprocessing 
penalty, as the given `src/main.c` will bloat to a magnitude larger than the
source file post-preprocessing. And it also causes a bloated namespace, but name
collisions would (presumably) be rare, and are easily curable by tuning naming
macros in either header file.

My largest regret throughout the entire project was focusing on keeping the end
code as clean as possible, with some regard for the actual header code, but in
the end it became somewhat spaghetti, with no thanks to my extensive use of GCC
extensions and obscure coding practice.

As usual:

```c
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
```

This is likely the largest culprit of incompatibility between compilers,
platforms, standards, etc.. But, I have the most familiarity in programming
with GCC, and so this is my preferred solution over an alternative solution
bodging variadic arguments, and messing with the assembler/linker pair to
prevent this function from being generated with rip-relative addressing to allow
for regular C programming.

```c
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
```

This, being the crux of the header-only library as is, is deceivingly simple.
We forward declare the aggregated methods the programmer defines to avoid having
to nest methods under a named aggregate (like I originally planned to support)
in any resulting instance, this is necessary for later mechanisms that need to
keep type-correctness and preserve the method list's order in later accesses.

After defining the object's main aggregate, the matching header/footers
demarcate the method aggregate to allow for later access via pointer arithmetic
during run-time, where aggregate names may not be known or accessible.

The larger static structure, named by the `BASE_CLASS_NAME` macro defines the
base structure that will be `memcpy`d to subsequent instances in `new_type` in
order to preserve properties that could have only been set at run-time, and
would otherwise require a lot of run-time gimmicking.
The header/footer come in use in the simple pointer arithmetic to determine
the number of methods the user defined, and the `offsetof` macros to set
the pivotal attribute `method_list_offset` for the run-time--which exists 
primarily to avoid relying on type casts that might not necessarily be
compatible due to compilers inserting padding or reordering, or anything really.

```c
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
```

The bulk of this macro is just setting up the default constructor/destructor
method properties to ensure proper garbage collection later in `free_instance`.
Ottherwise, we're doing a little bit of black magic by using:

```c
#define TYPE_METHOD_CTOR_DECL(type_name, method_name) \
  __attribute__((constructor)) void __ctor_##type_name##_##method_name (void)
```

Which allows the header to populate the static structure that we previously
created in `create_type_from_struct` with the corresponding methods the user
defines using this macro; this all arises from the invalid code scenario of:

```c
struct foo
{
  int a;
}

static struct foo bar;
bar.a = <...>;  /* ??? */
```

Which is an understandable limitation, but eww.

In any case, I create a constructor per each method definition which initialises
each method definition at runtime inside the global object structure, to ensure
all future invocations of `new_type` have valid methods for which to generate
stubs.

The whole chunky portion of code that involves `strcmp`ing between literals is
purely due to allow overriding default constructors, as ugly as it is.