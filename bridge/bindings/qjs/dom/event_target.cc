/*
 * Copyright (C) 2021 Alibaba Inc. All rights reserved.
 * Author: Kraken Team.
 */

#include "event_target.h"

#include <utility>
#include "bindings/qjs/bom/window.h"
#include "bindings/qjs/dom/text_node.h"
#include "bindings/qjs/qjs_patch.h"
#include "document.h"
#include "element.h"
#include "event.h"
#include "kraken_bridge.h"

namespace kraken::binding::qjs {

static std::atomic<int32_t> globalEventTargetId{0};
std::once_flag kEventTargetInitFlag;
#define GetPropertyCallPreFix "_getProperty_"

void bindEventTarget(std::unique_ptr<JSContext>& context) {
  auto* constructor = EventTarget::instance(context.get());
  // Set globalThis and Window's prototype to EventTarget's prototype to support EventTarget methods in global.
  JS_SetPrototype(context->ctx(), context->global(), constructor->jsObject);
  context->defineGlobalProperty("EventTarget", constructor->jsObject);
}

JSClassID EventTarget::kEventTargetClassId{0};

EventTarget::EventTarget(JSContext* context, const char* name) : HostClass(context, name) {}
EventTarget::EventTarget(JSContext* context) : HostClass(context, "EventTarget") {
  std::call_once(kEventTargetInitFlag, []() { JS_NewClassID(&kEventTargetClassId); });
}

JSValue EventTarget::instanceConstructor(QjsContext* ctx, JSValue func_obj, JSValue this_val, int argc, JSValue* argv) {
  auto eventTarget = new EventTargetInstance(this, kEventTargetClassId, "EventTarget");
  return eventTarget->jsObject;
}

JSClassID EventTarget::classId() {
  assert_m(false, "classId is not implemented");
  return 0;
}

JSClassID EventTarget::classId(JSValue& value) {
  JSClassID classId = JSValueGetClassId(value);
  return classId;
}

JSValue EventTarget::addEventListener(QjsContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Failed to addEventListener: type and listener are required.");
  }

  auto* eventTargetInstance = static_cast<EventTargetInstance*>(JS_GetOpaque(this_val, EventTarget::classId(this_val)));
  if (eventTargetInstance == nullptr) {
    return JS_ThrowTypeError(ctx, "Failed to addEventListener: this is not an EventTarget object.");
  }

  JSValue eventTypeValue = argv[0];
  JSValue callback = argv[1];

  if (!JS_IsString(eventTypeValue) || !JS_IsObject(callback) || !JS_IsFunction(ctx, callback)) {
    return JS_UNDEFINED;
  }

  JSAtom eventTypeAtom = JS_ValueToAtom(ctx, eventTypeValue);

  // Init list.
  if (eventTargetInstance->m_eventHandlers.count(eventTypeAtom) == 0) {
    eventTargetInstance->m_eventHandlers[eventTypeAtom] = std::vector<JSValue>();
  }

  auto &eventHandlers = eventTargetInstance->m_eventHandlers[eventTypeAtom];

  // Dart needs to be notified for the first registration event.
  if (eventHandlers.empty() || JS_HasProperty(ctx, eventTargetInstance->m_propertyEventHandler.value(), eventTypeAtom)) {
    int32_t contextId = eventTargetInstance->prototype()->contextId();

    NativeString args_01{};
    buildUICommandArgs(ctx, eventTypeValue, args_01);

    foundation::UICommandBuffer::instance(contextId)->addCommand(eventTargetInstance->m_eventTargetId, UICommand::addEvent, args_01, nullptr);
  }

  eventHandlers.emplace_back(JS_DupValue(ctx, callback));
  JS_FreeAtom(ctx, eventTypeAtom);

  return JS_UNDEFINED;
}

JSValue EventTarget::removeEventListener(QjsContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Failed to removeEventListener: at least type and listener are required.");
  }

  auto* eventTargetInstance = static_cast<EventTargetInstance*>(JS_GetOpaque(this_val, EventTarget::classId(this_val)));
  if (eventTargetInstance == nullptr) {
    return JS_ThrowTypeError(ctx, "Failed to addEventListener: this is not an EventTarget object.");
  }

  JSValue eventTypeValue = argv[0];
  JSValue callback = argv[1];

  if (!JS_IsString(eventTypeValue) || !JS_IsObject(callback) || !JS_IsObject(callback)) {
    return JS_ThrowTypeError(ctx, "Failed to removeEventListener: eventName should be an string.");
  }

  JSAtom eventTypeAtom = JS_ValueToAtom(ctx, eventTypeValue);

  if (eventTargetInstance->m_eventHandlers.count(eventTypeAtom) == 0) {
    JS_FreeAtom(ctx, eventTypeAtom);
    return JS_UNDEFINED;
  }

  auto &eventHandles = eventTargetInstance->m_eventHandlers[eventTypeAtom];
  auto idx = std::find_if(eventHandles.begin(), eventHandles.end(), [&callback](JSValue v) {
    return JS_VALUE_GET_PTR(v) == JS_VALUE_GET_PTR(callback);
  });

  if (idx != eventHandles.end()) {
    JS_FreeValue(ctx, *idx);
    eventHandles.erase(idx);
  }

  if (eventHandles.empty() && JS_HasProperty(ctx, eventTargetInstance->m_propertyEventHandler.value(), eventTypeAtom)) {
    // Dart needs to be notified for handles is empty.
    int32_t contextId = eventTargetInstance->prototype()->contextId();

    NativeString args_01{};
    buildUICommandArgs(ctx, eventTypeValue, args_01);

    foundation::UICommandBuffer::instance(contextId)->addCommand(eventTargetInstance->m_eventTargetId, UICommand::removeEvent, args_01, nullptr);
  }

  JS_FreeAtom(ctx, eventTypeAtom);
  return JS_UNDEFINED;
}

JSValue EventTarget::dispatchEvent(QjsContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  if (argc != 1) {
    return JS_ThrowTypeError(ctx, "Failed to dispatchEvent: first arguments should be an event object");
  }

  auto* eventTargetInstance = static_cast<EventTargetInstance*>(JS_GetOpaque(this_val, EventTarget::classId(this_val)));
  if (eventTargetInstance == nullptr) {
    return JS_ThrowTypeError(ctx, "Failed to addEventListener: this is not an EventTarget object.");
  }

  JSValue eventValue = argv[0];
  auto eventInstance = reinterpret_cast<EventInstance*>(JS_GetOpaque(eventValue, EventTarget::classId(eventValue)));
  return JS_NewBool(ctx, eventTargetInstance->dispatchEvent(eventInstance));
}

bool EventTargetInstance::dispatchEvent(EventInstance* event) {
  std::u16string u16EventType = std::u16string(reinterpret_cast<const char16_t*>(event->nativeEvent->type->string), event->nativeEvent->type->length);
  std::string eventType = toUTF8(u16EventType);

  // protect this util event trigger finished.
  JS_DupValue(m_ctx, jsObject);

  internalDispatchEvent(event);

  // Bubble event to root event target.
  if (event->nativeEvent->bubbles == 1 && !event->propagationStopped()) {
    auto node = reinterpret_cast<NodeInstance*>(this);
    auto* parent = static_cast<NodeInstance*>(JS_GetOpaque(node->parentNode, Node::classId(node->parentNode)));

    if (parent != nullptr) {
      parent->dispatchEvent(event);
    } else {
      // Window does not inherit from Node, so it is not in the Node tree and needs to continue passing to the Window when it bubbles to Document.
      JSValue globalObjectValue = JS_GetGlobalObject(m_context->ctx());
      auto* window = static_cast<WindowInstance*>(JS_GetOpaque(globalObjectValue, Window::classId()));
      window->internalDispatchEvent(event);
      JS_FreeValue(m_ctx, globalObjectValue);
    }
  }

  JS_FreeValue(m_ctx, jsObject);

  return event->cancelled();
}

bool EventTargetInstance::internalDispatchEvent(EventInstance* eventInstance) {
  std::u16string u16EventType = std::u16string(reinterpret_cast<const char16_t*>(eventInstance->nativeEvent->type->string), eventInstance->nativeEvent->type->length);
  std::string eventType = toUTF8(u16EventType);
  JSAtom eventTypeAtom = JS_NewAtom(m_ctx, eventType.c_str());

  // Modify the currentTarget to this.
  eventInstance->nativeEvent->currentTarget = this;

  // Dispatch event listeners writen by addEventListener
  auto _dispatchEvent = [&eventInstance, this](JSValue& handler) {
    if (eventInstance->propagationImmediatelyStopped())
      return;
    // The third params `thisObject` to null equals global object.
    JSValue returnedValue = JS_Call(m_ctx, handler, JS_NULL, 1, &eventInstance->jsObject);
    m_context->handleException(&returnedValue);
    m_context->drainPendingPromiseJobs();
    JS_FreeValue(m_ctx, returnedValue);
  };

  if (m_eventHandlers.count(eventTypeAtom) > 0) {
    auto& eventHandlers = m_eventHandlers[eventTypeAtom];
    for (auto& eventHandler : eventHandlers) {
      _dispatchEvent(eventHandler);
    }
  }

  // Dispatch event listener white by 'on' prefix property.
  if (JS_HasProperty(m_ctx, m_propertyEventHandler.value(), eventTypeAtom)) {
    if (eventType == "error") {
      auto _dispatchErrorEvent = [&eventInstance, this, eventType](JSValue& handler) {
        JSValue error = JS_GetPropertyStr(m_ctx, eventInstance->jsObject, "error");
        JSValue messageValue = JS_GetPropertyStr(m_ctx, error, "message");
        JSValue lineNumberValue = JS_GetPropertyStr(m_ctx, error, "lineNumber");
        JSValue fileNameValue = JS_GetPropertyStr(m_ctx, error, "fileName");
        JSValue columnValue = JS_NewUint32(m_ctx, 0);

        JSValue args[]{messageValue, fileNameValue, lineNumberValue, columnValue, error};
        JS_Call(m_ctx, handler, eventInstance->jsObject, 5, args);
        m_context->drainPendingPromiseJobs();

        JS_FreeValue(m_ctx, error);
        JS_FreeValue(m_ctx, messageValue);
        JS_FreeValue(m_ctx, fileNameValue);
        JS_FreeValue(m_ctx, lineNumberValue);
        JS_FreeValue(m_ctx, columnValue);
      };
      JSValue v = JS_GetProperty(m_ctx, m_propertyEventHandler.value(), eventTypeAtom);
      _dispatchErrorEvent(v);
      JS_FreeValue(m_ctx, v);
    } else {
      JSValue v = JS_GetProperty(m_ctx, m_propertyEventHandler.value(), eventTypeAtom);
      _dispatchEvent(v);
      JS_FreeValue(m_ctx, v);
    }
  }

  JS_FreeAtom(m_ctx, eventTypeAtom);

  // do not dispatch event when event has been canceled
  // true is prevented.
  return eventInstance->cancelled();
}

EventTargetInstance::EventTargetInstance(EventTarget* eventTarget, JSClassID classId, JSClassExoticMethods& exoticMethods, std::string name)
    : Instance(eventTarget, name, &exoticMethods, classId, finalize) {
  m_eventTargetId = globalEventTargetId++;
}

EventTargetInstance::EventTargetInstance(EventTarget* eventTarget, JSClassID classId, std::string name) : Instance(eventTarget, std::move(name), nullptr, classId, finalize) {
  m_eventTargetId = globalEventTargetId++;
}

EventTargetInstance::EventTargetInstance(EventTarget* eventTarget, JSClassID classId, std::string name, int64_t eventTargetId)
    : Instance(eventTarget, std::move(name), nullptr, classId, finalize), m_eventTargetId(eventTargetId) {}

JSClassID EventTargetInstance::classId() {
  assert_m(false, "classId is not implemented");
  return 0;
}

EventTargetInstance::~EventTargetInstance() {
  foundation::UICommandBuffer::instance(m_contextId)->addCommand(m_eventTargetId, UICommand::disposeEventTarget, nullptr, false);
#if FLUTTER_BACKEND
  getDartMethod()->flushUICommand();
  delete nativeEventTarget;
#endif
}

int EventTargetInstance::hasProperty(QjsContext* ctx, JSValue obj, JSAtom atom) {
  auto* eventTarget = static_cast<EventTargetInstance*>(JS_GetOpaque(obj, JSValueGetClassId(obj)));
  auto* prototype = static_cast<EventTarget*>(eventTarget->prototype());

  if (JS_HasProperty(ctx, prototype->m_prototypeObject, atom))
    return true;

  JSValue atomString = JS_AtomToString(ctx, atom);
  JSString* p = JS_VALUE_GET_STRING(atomString);
  // There are still one reference_count in atom. It's safe to free here.
  JS_FreeValue(ctx, atomString);

  if (!p->is_wide_char && p->u.str8[0] == 'o' && p->u.str8[1] == 'n') {
    return !JS_IsNull(eventTarget->getPropertyHandler(p));
  }

  return JS_HasProperty(ctx, eventTarget->m_properties.value(), atom);
}

JSValue EventTargetInstance::getProperty(QjsContext* ctx, JSValue obj, JSAtom atom, JSValue receiver) {
  auto* eventTarget = static_cast<EventTargetInstance*>(JS_GetOpaque(obj, JSValueGetClassId(obj)));
  JSValue prototype = JS_GetPrototype(ctx, eventTarget->jsObject);
  if (JS_HasProperty(ctx, prototype, atom)) {
    JSValue ret = JS_GetPropertyInternal(ctx, prototype, atom, eventTarget->jsObject, 0);
    JS_FreeValue(ctx, prototype);
    return ret;
  }
  JS_FreeValue(ctx, prototype);

  JSValue atomString = JS_AtomToString(ctx, atom);
  JSString* p = JS_VALUE_GET_STRING(atomString);
  // There are still one reference_count in atom. It's safe to free here.
  JS_FreeValue(ctx, atomString);

  if (!p->is_wide_char && p->u.str8[0] == 'o' && p->u.str8[1] == 'n') {
    return eventTarget->getPropertyHandler(p);
  }

  if (JS_HasProperty(ctx, eventTarget->m_properties.value(), atom)) {
    return JS_GetProperty(ctx, eventTarget->m_properties.value(), atom);
  }

  // For plugin elements, try to auto generate properties and functions from dart response.
  if (isJavaScriptExtensionElementInstance(eventTarget->context(), eventTarget->jsObject)) {
    const char* cmethod = JS_AtomToCString(eventTarget->m_ctx, atom);
    // Property starts with underscore are taken as private property in javascript object.
    if (cmethod[0] == '_') {
      JS_FreeCString(eventTarget->m_ctx, cmethod);
      return JS_UNDEFINED;
    }
    JSValue result = eventTarget->getNativeProperty(cmethod);
    JS_FreeCString(ctx, cmethod);
    return result;
  }

  return JS_UNDEFINED;
}

int EventTargetInstance::setProperty(QjsContext* ctx, JSValue obj, JSAtom atom, JSValue value, JSValue receiver, int flags) {
  auto* eventTarget = static_cast<EventTargetInstance*>(JS_GetOpaque(obj, JSValueGetClassId(obj)));
  JSValue prototype = JS_GetPrototype(ctx, eventTarget->jsObject);

  // Check there are setter functions on prototype.
  if (JS_HasProperty(ctx, prototype, atom)) {
    // Read setter function from prototype Object.
    JSPropertyDescriptor descriptor;
    JS_GetOwnProperty(ctx, &descriptor, prototype, atom);
    JSValue setterFunc = descriptor.setter;
    assert_m(JS_IsFunction(ctx, setterFunc), "Setter on prototype should be an function.");
    JSValue ret = JS_Call(ctx, setterFunc, eventTarget->jsObject, 1, &value);
    if (JS_IsException(ret))
      return -1;

    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, descriptor.setter);
    JS_FreeValue(ctx, descriptor.getter);
    JS_FreeValue(ctx, prototype);
    return 1;
  }

  JS_FreeValue(ctx, prototype);

  JSValue atomString = JS_AtomToString(ctx, atom);
  JSString* p = JS_VALUE_GET_STRING(atomString);

  if (!p->is_wide_char && p->u.str8[0] == 'o' && p->u.str8[1] == 'n') {
    eventTarget->setPropertyHandler(p, value);
  } else {
    if (!JS_HasProperty(ctx, eventTarget->m_properties.value(), atom)) {
      auto* atomJob = new AtomJob{atom};
      list_add_tail(&atomJob->link, &eventTarget->m_context->atom_job_list);
      // Increase one reference count for atom to hold this atom value until eventTarget disposed.
      JS_DupAtom(ctx, atom);
    }

    JS_SetProperty(ctx, eventTarget->m_properties.value(), atom, JS_DupValue(ctx, value));

    if (isJavaScriptExtensionElementInstance(eventTarget->context(), eventTarget->jsObject) && !p->is_wide_char && p->u.str8[0] != '_') {
      std::unique_ptr<NativeString> args_01 = atomToNativeString(ctx, atom);
      std::unique_ptr<NativeString> args_02 = jsValueToNativeString(ctx, value);
      foundation::UICommandBuffer::instance(eventTarget->m_contextId)->addCommand(eventTarget->m_eventTargetId, UICommand::setProperty, *args_01, *args_02, nullptr);
    }
  }

  JS_FreeValue(ctx, atomString);

  return 0;
}

int EventTargetInstance::deleteProperty(QjsContext* ctx, JSValue obj, JSAtom prop) {
  return 0;
}

JSValue EventTargetInstance::callNativeMethods(const char* method, int32_t argc, NativeValue* argv) {
  if (nativeEventTarget->callNativeMethods == nullptr) {
    return JS_ThrowTypeError(m_ctx, "Failed to call native dart methods: callNativeMethods not initialized.");
  }

  std::u16string methodString;
  fromUTF8(method, methodString);

  NativeString m{reinterpret_cast<const uint16_t*>(methodString.c_str()), static_cast<uint32_t>(methodString.size())};

  NativeValue nativeValue{};
  nativeEventTarget->callNativeMethods(nativeEventTarget, &nativeValue, &m, argc, argv);
  JSValue returnValue = nativeValueToJSValue(m_context, nativeValue);
  return returnValue;
}

void EventTargetInstance::setPropertyHandler(JSString* p, JSValue value) {
  char eventType[p->len + 1 - 2];
  memcpy(eventType, &p->u.str8[2], p->len + 1 - 2);
  JSAtom atom = JS_NewAtom(m_ctx, eventType);
  auto* atomJob = new AtomJob{atom};
  list_add_tail(&atomJob->link, &m_context->atom_job_list);

  // When evaluate scripts like 'element.onclick = null', we needs to remove the event handlers callbacks
  if (JS_IsNull(value)) {
    JS_FreeAtom(m_ctx, atom);
    list_del(&atomJob->link);
    JS_DeleteProperty(m_ctx, m_propertyEventHandler.value(), atom, 0);
    return;
  }

  if (!JS_IsFunction(m_ctx, value)) {
    JS_FreeAtom(m_ctx, atom);
    list_del(&atomJob->link);
    return;
  }

  JSValue newCallback = JS_DupValue(m_ctx, value);
  JS_SetProperty(m_ctx, m_propertyEventHandler.value(), atom, newCallback);
  if (m_eventHandlers.empty()) {
    int32_t contextId = m_context->getContextId();
    std::unique_ptr<NativeString> args_01 = atomToNativeString(m_ctx, atom);
    int32_t type = JS_IsFunction(m_ctx, value) ? UICommand::addEvent : UICommand::removeEvent;
    foundation::UICommandBuffer::instance(contextId)->addCommand(m_eventTargetId, type, *args_01, nullptr);
  }
}

JSValue EventTargetInstance::getPropertyHandler(JSString* p) {
  char eventType[p->len + 1 - 2];
  memcpy(eventType, &p->u.str8[2], p->len + 1 - 2);
  JSAtom atom = JS_NewAtom(m_ctx, eventType);
  if (!JS_HasProperty(m_ctx, m_propertyEventHandler.value(), atom)) {
    return JS_NULL;
  }
  return JS_GetProperty(m_ctx, m_propertyEventHandler.value(), atom);
}

void EventTargetInstance::finalize(JSRuntime* rt, JSValue val) {
  auto* eventTarget = static_cast<EventTargetInstance*>(JS_GetOpaque(val, EventTarget::classId(val)));
  delete eventTarget;
}

JSValue EventTargetInstance::getNativeProperty(const char* prop) {
  std::string method = GetPropertyCallPreFix + std::string(prop);
  getDartMethod()->flushUICommand();
  JSValue result = callNativeMethods(method.c_str(), 0, nullptr);
  return result;
}

// JSValues are stored in this class are no visible to QuickJS GC.
// We needs to gc which JSValues are still holding.
void EventTargetInstance::trace(JSRuntime* rt, JSValue val, JS_MarkFunc* mark_func) {
  // Trace m_eventHandlers
  for(auto &eventHandlers : m_eventHandlers) {
    for (auto &eventHandler : eventHandlers.second) {
      JS_MarkValue(rt, eventHandler, mark_func);
    }
  }
}

void EventTargetInstance::copyNodeProperties(EventTargetInstance* newNode, EventTargetInstance* referenceNode) {
  QjsContext* ctx = referenceNode->m_ctx;
  JSValue propKeys = objectGetKeys(ctx, referenceNode->m_properties.value());
  uint32_t propKeyLen = arrayGetLength(ctx, propKeys);

  for (int i = 0; i < propKeyLen; i++) {
    JSValue k = JS_GetPropertyUint32(ctx, propKeys, i);
    JSAtom kt = JS_ValueToAtom(ctx, k);
    JSValue v = JS_GetProperty(ctx, referenceNode->m_properties.value(), kt);
    JS_SetProperty(ctx, newNode->m_properties.value(), kt, JS_DupValue(ctx, v));

    JS_FreeAtom(ctx, kt);
    JS_FreeValue(ctx, k);
  }

  JS_FreeValue(ctx, propKeys);
}

void NativeEventTarget::dispatchEventImpl(NativeEventTarget* nativeEventTarget, NativeString* nativeEventType, void* rawEvent, int32_t isCustomEvent) {
  assert_m(nativeEventTarget->instance != nullptr, "NativeEventTarget should have owner");
  EventTargetInstance* eventTargetInstance = nativeEventTarget->instance;
  JSContext* context = eventTargetInstance->context();
  std::u16string u16EventType = std::u16string(reinterpret_cast<const char16_t*>(nativeEventType->string), nativeEventType->length);
  std::string eventType = toUTF8(u16EventType);
  auto* raw = static_cast<RawEvent*>(rawEvent);
  // NativeEvent members are memory aligned corresponding to NativeEvent.
  // So we can reinterpret_cast raw bytes pointer to NativeEvent type directly.
  auto* nativeEvent = reinterpret_cast<NativeEvent*>(raw->bytes);
  EventInstance* eventInstance = Event::buildEventInstance(eventType, context, nativeEvent, isCustomEvent == 1);
  eventInstance->nativeEvent->target = eventTargetInstance;
  eventTargetInstance->dispatchEvent(eventInstance);
  JS_FreeValue(context->ctx(), eventInstance->jsObject);
}

}  // namespace kraken::binding::qjs
