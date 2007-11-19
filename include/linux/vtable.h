/* Virtual Method Table support - experimental */

/* A way to reference vtable */
#define VTABLE(interface) interface##_methods
/* A way to have vtable definined. This should be put as the first member of enclosing struct */
#define VTABLE_DEFINE(interface) struct interface##_methods interface##_methods
/* A way to reference method. Result is pointer to method, so it can be called, checked for NULL, etc */
#define METHOD(device, interface, method) (((struct interface##_methods*)((device)->platform_data))->method)

/* This is attempt to summport multiple interfaces */
//#define METHOD(device, interface, method) (((struct interface##_methods*)(device->platform_data))->interface##_methods.method)
