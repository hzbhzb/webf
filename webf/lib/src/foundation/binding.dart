/*
 * Copyright (C) 2019-2022 The Kraken authors. All rights reserved.
 * Copyright (C) 2022-present The WebF authors. All rights reserved.
 */
import 'package:flutter/foundation.dart';
import 'package:webf/bridge.dart';
import 'dart:ffi';

typedef BindingObjectOperation = void Function(BindingObject bindingObject);

class BindingContext {
  final int contextId;
  final Pointer<NativeBindingObject> pointer;
  const BindingContext(this.contextId, this.pointer);
}

abstract class BindingObject {
  static BindingObjectOperation? bind;
  static BindingObjectOperation? unbind;

  final BindingContext? _context;

  int? get contextId => _context?.contextId;
  Pointer<NativeBindingObject>? get pointer => _context?.pointer;

  BindingObject([BindingContext? context]) : _context = context {
    _bind();
  }

  // Bind dart side object method to receive invoking from native side.
  void _bind() {
    if (bind != null) {
      bind!(this);
    }
  }

  void _unbind() {
    if (unbind != null) {
      unbind!(this);
    }
  }

  // Get a property, eg:
  //   console.log(el.foo);
  dynamic getBindingProperty(String key) {
    return unimplemented_properties_[key];
  }

  Map<String, dynamic> unimplemented_properties_ = {};
  // Set a property, eg:
  //   el.foo = 'bar';
  void setBindingProperty(String key, value) {
    unimplemented_properties_[key] = value;
  }

  // Return a list contains all supported properties.
  void getAllBindingPropertyNames(List<String> properties) {
  }

  // Call a method, eg:
  //   el.getContext('2x');
  dynamic invokeBindingMethod(String method, List args) {}

  @mustCallSuper
  void dispose() {
    _unbind();
  }
}
