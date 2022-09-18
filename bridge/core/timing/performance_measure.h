/*
 * Copyright (C) 2022-present The WebF authors. All rights reserved.
 */

#ifndef WEBF_CORE_TIMING_PERFORMANCE_MEASURE_H_
#define WEBF_CORE_TIMING_PERFORMANCE_MEASURE_H_

#include "core/executing_context.h"
#include "bindings/qjs/script_value.h"
#include "performance_entry.h"

namespace webf {

class PerformanceMeasure : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PerformanceMeasure* Create(ExecutingContext* context,
                                    const AtomicString& name,
                                    int64_t start_time,
                                    int64_t end_time,
                                    const ScriptValue& detail,
                                    ExceptionState& exception_state);

  explicit PerformanceMeasure(ExecutingContext* context,
                              const AtomicString& name,
                              int64_t start_time,
                              int64_t end_time,
                              const ScriptValue& detail,
                              ExceptionState& exception_state);

  ScriptValue detail() const;

  AtomicString entryType() const override;

 private:
  ScriptValue detail_;
};

}  // namespace webf

#endif  // WEBF_CORE_TIMING_PERFORMANCE_MEASURE_H_
