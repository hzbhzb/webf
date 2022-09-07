/*
 * Copyright (C) 2022-present The WebF authors. All rights reserved.
 */

#include "focus_event.h"
#include "core/dom/events/event_target.h"
#include "core/frame/window.h"
#include "qjs_focus_event.h"

namespace webf {

FocusEvent* FocusEvent::Create(ExecutingContext* context, const AtomicString& type, ExceptionState& exception_state) {
  return MakeGarbageCollected<FocusEvent>(context, type, exception_state);
}

FocusEvent* FocusEvent::Create(ExecutingContext* context,
                               const AtomicString& type,
                               double detail,
                               Window* view,
                               double which,
                               EventTarget* relatedTarget,
                               ExceptionState& exception_state) {
  return MakeGarbageCollected<FocusEvent>(context, type, detail, view, which, relatedTarget, exception_state);
}

FocusEvent* FocusEvent::Create(ExecutingContext* context,
                               const AtomicString& type,
                               const std::shared_ptr<FocusEventInit>& initializer,
                               ExceptionState& exception_state) {
  return MakeGarbageCollected<FocusEvent>(context, type, initializer, exception_state);
}

FocusEvent::FocusEvent(ExecutingContext* context, const AtomicString& type, ExceptionState& exception_state)
    : UIEvent(context, type, exception_state) {}

FocusEvent::FocusEvent(ExecutingContext *context,
                       const AtomicString &type,
                       double detail,
                       Window *view,
                       double which,
                       EventTarget *relatedTarget,
                       ExceptionState &exception_state)
    : UIEvent(context, type, detail, view, which, exception_state), related_target_(relatedTarget) {}

FocusEvent::FocusEvent(ExecutingContext *context,
                       const AtomicString &type,
                       const std::shared_ptr<FocusEventInit> &initializer,
                       ExceptionState &exception_state)
    : UIEvent(context, type, initializer, exception_state), related_target_(initializer->relatedTarget()) {}

FocusEvent::FocusEvent(ExecutingContext *context, const AtomicString &type, NativeFocusEvent *native_focus_event)
    : UIEvent(context, type, &native_focus_event->native_event),
      related_target_(DynamicTo<EventTarget>(BindingObject::From(native_focus_event->relatedTarget))) {}

EventTarget *FocusEvent::relatedTarget() const {
  return related_target_;
}

bool FocusEvent::IsFocusEvent() const {
  return true;
}

}  // namespace webf