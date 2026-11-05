#include "beadsy_standalone_first_mouse.hpp"

#import <AppKit/AppKit.h>
#include <objc/runtime.h>

#include <cstring>
#include <vector>

namespace beadsy {

void accept_first_mouse_in_standalone() {
  const int count = objc_getClassList(nullptr, 0);
  if (count <= 0) return;
  std::vector<Class> classes(static_cast<std::size_t>(count));
  objc_getClassList(classes.data(), count);
  const IMP yes = imp_implementationWithBlock(^BOOL(id, NSEvent *) { return YES; });
  for (Class cls : classes) {
    const char *name = class_getName(cls);
    // The window host compiles its view classes with a per-binary suffix
    // (PulpView_<target>, PulpMetalView_<target>); match the family.
    if (std::strncmp(name, "PulpView", 8) != 0 &&
        std::strncmp(name, "PulpMetalView", 13) != 0)
      continue;
    // Replace on the class itself so a subclass keeps its own override and a
    // base-class implementation is not shared by accident.
    class_replaceMethod(cls, @selector(acceptsFirstMouse:), yes, "B@:@");
  }
}

} // namespace beadsy
