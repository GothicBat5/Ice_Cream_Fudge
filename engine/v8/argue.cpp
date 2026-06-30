#include "src/api/api-arguments.h" // <- Key
#include "src/api/api-arguments-inl.h"

namespace v8 {
namespace internal {

void FunctionCallbackArguments::IterateInstance(RootVisitor* v) 
{
  v->VisitRootPointer(Root::kRelocatable, nullptr, slot_at(T::kNewTargetIndex));
  v->VisitRootPointers(Root::kRelocatable, nullptr, slot_at(T::kFirstApiArgumentIndex), FullObjectSlot(values_.end()));
}

void PropertyCallbackArguments::IterateInstance(RootVisitor* v) 
{
  if (is_named()) 
  {
    v->VisitRootPointer(Root::kRelocatable, nullptr, slot_at(T::kPropertyKeyIndex));
  }

  v->VisitRootPointers(Root::kRelocatable, nullptr, slot_at(T::kFirstApiArgumentIndex), slot_at(kMandatoryArgsLength));
}

}  // namespace internal
}  // namespace v8
