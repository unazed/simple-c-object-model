#include <stdio.h>
#include <stdlib.h>
#include "instance.h"

create_type_from_struct (
  {
    int age;
  },
  define_type_methods ({
    define_method(void, init, void);
    define_method(void, bark, int);
  }),
  dog_t
);

declare_type_method(of_type(dog_t), void, init)(dog_t self)
{
  printf ("custom init: %p\n", self);
}

declare_type_method(of_type(dog_t), void, bark)(dog_t self, int input)
{
  printf ("self: %p, input: %d, age = %d\n", self, input, self->age);
  self->age = 5;
}

int
main (void)
{
  auto my_dog = new_type (dog_t);
  printf ("instance at: %p\n", my_dog);
  my_dog->bark (69);
  my_dog->bark (1337);
  free_type (my_dog);
  return EXIT_SUCCESS;
}