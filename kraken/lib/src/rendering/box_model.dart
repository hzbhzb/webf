/*
 * Copyright (C) 2019-present Alibaba Inc. All rights reserved.
 * Author: Kraken Team.
 */

import 'dart:ui';
import 'dart:math' as math;
import 'package:kraken/css.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/foundation.dart';
import 'package:kraken/element.dart';
import 'package:kraken/rendering.dart';
import 'padding.dart';

class RenderLayoutParentData extends ContainerBoxParentData<RenderBox> {
  /// The distance by which the child's top edge is inset from the top of the stack.
  double top;

  /// The distance by which the child's right edge is inset from the right of the stack.
  double right;

  /// The distance by which the child's bottom edge is inset from the bottom of the stack.
  double bottom;

  /// The distance by which the child's left edge is inset from the left of the stack.
  double left;

  /// The child's width.
  ///
  /// Ignored if both left and right are non-null.
  double width;

  /// The child's height.
  ///
  /// Ignored if both top and bottom are non-null.
  double height;

  bool isPositioned = false;

  /// Row index of child when wrapping
  int runIndex = 0;

  int zIndex = 0;
  CSSPositionType position = CSSPositionType.static;

  // Whether offset is already set
  bool isOffsetSet = false;

  @override
  String toString() {
    return 'zIndex=$zIndex; position=$position; isPositioned=$isPositioned; top=$top; left=$left; bottom=$bottom; right=$right; ${super.toString()}; runIndex: $runIndex;';
  }
}

class RenderLayoutBox extends RenderBoxModel
    with
        ContainerRenderObjectMixin<RenderBox, ContainerBoxParentData<RenderBox>>,
        RenderBoxContainerDefaultsMixin<RenderBox, ContainerBoxParentData<RenderBox>> {
  RenderLayoutBox({int targetId, CSSStyleDeclaration style, ElementManager elementManager})
      : super(targetId: targetId, style: style, elementManager: elementManager);

  @override
  void markNeedsLayout() {
    super.markNeedsLayout();

    // FlexItem layout must trigger flex container to layout.
    if (parent != null && parent is RenderFlexLayout) {
      markParentNeedsLayout();
    }
  }

  bool _needsSortChildren = true;
  bool get needsSortChildren {
    return _needsSortChildren;
  }
  // Mark this container to sort children by zIndex properties.
  // When children have positioned elements, which needs to reorder and paint earlier than flow layout renderObjects.
  void markNeedsSortChildren() {
    _needsSortChildren = true;
  }

  bool _isChildrenSorted = false;
  bool get isChildrenSorted => _isChildrenSorted;

  List<RenderObject> _sortedChildren;
  List<RenderObject> get sortedChildren {
    if (_sortedChildren == null) return [];
    return _sortedChildren;
  }
  set sortedChildren(List<RenderObject> value) {
    assert(value != null);
    _isChildrenSorted = true;
    _sortedChildren = value;
  }

  @override
  void insert(RenderBox child, { RenderBox after }) {
    super.insert(child, after: after);
    _isChildrenSorted = false;
  }

  @override
  void add(RenderBox child) {
    super.add(child);
    _isChildrenSorted = false;
  }

  @override
  void addAll(List<RenderBox> children) {
    super.addAll(children);
    _isChildrenSorted = false;
  }

  @override
  void remove(RenderBox child) {
    super.remove(child);
    _isChildrenSorted = false;
  }

  @override
  void removeAll() {
    super.removeAll();
    _isChildrenSorted = false;
  }

  void move(RenderBox child, { RenderBox after }) {
    super.move(child, after: after);
    _isChildrenSorted = false;
  }

  void sortChildrenByZIndex() {
    List<RenderObject> children = getChildrenAsList();
    children.sort((RenderObject prev, RenderObject next) {
      RenderLayoutParentData prevParentData = prev.parentData;
      RenderLayoutParentData nextParentData = next.parentData;
      // Place positioned element after non positioned element
      if (prevParentData.position == CSSPositionType.static && nextParentData.position != CSSPositionType.static) {
        return -1;
      }
      if (prevParentData.position != CSSPositionType.static && nextParentData.position == CSSPositionType.static) {
        return 1;
      }
      // z-index applies to flex-item ignoring position property
      int prevZIndex = prevParentData.zIndex ?? 0;
      int nextZIndex = nextParentData.zIndex ?? 0;
      return prevZIndex - nextZIndex;
    });
    sortedChildren = children;
  }

  // Get all children as a list and detach them all.
  List<RenderObject> getDetachedChildrenAsList() {
    List<RenderObject> children = getChildrenAsList();
    removeAll();
    return children;
  }
}

class RenderBoxModel extends RenderBox with
  RenderPaddingMixin,
  RenderMarginMixin,
  RenderBoxDecorationMixin,
  RenderTransformMixin,
  RenderOverflowMixin,
  RenderOpacityMixin,
  RenderIntersectionObserverMixin,
  RenderContentVisibility,
  RenderPointerListenerMixin {
  RenderBoxModel({
    this.targetId,
    this.style,
    this.elementManager,
  }) : assert(targetId != null),
    super();

  @override
  bool get alwaysNeedsCompositing => _boxModelAlwaysNeedsCompositing();

  bool _boxModelAlwaysNeedsCompositing() {
    return intersectionAlwaysNeedsCompositing() || opacityAlwaysNeedsCompositing();
  }

  RenderPositionHolder renderPositionHolder;

  bool _debugHasBoxLayout = false;

  BoxConstraints _contentConstraints;
  BoxConstraints get contentConstraints {
    assert(_debugHasBoxLayout, 'can not access contentConstraints, RenderBoxModel has not layout: ${toString()}');
    assert(_contentConstraints != null);
    return _contentConstraints;
  }

  CSSDisplay _display;
  get display => _display;
  set display(CSSDisplay value) {
    if (value == null) return;
    if (_display != value) {
      markNeedsLayout();
      _display = value;
    }
  }

  // id of current element
  int targetId;

  // Element style;
  CSSStyleDeclaration style;

  ElementManager elementManager;

  BoxSizeType widthSizeType;
  BoxSizeType heightSizeType;

  // Positioned holder box ref.
  RenderPositionHolder positionedHolder;

  RenderBoxModel copyWith(RenderBoxModel newBox) {
    // Copy Sizing
    newBox.width = width;
    newBox.height = height;
    newBox.minWidth = minWidth;
    newBox.minHeight = minHeight;
    newBox.maxWidth = maxWidth;
    newBox.maxHeight = maxHeight;

    // Copy size type
    newBox.widthSizeType = widthSizeType;
    newBox.heightSizeType = heightSizeType;

    // Copy padding
    newBox.padding = padding;

    // Copy margin
    newBox.margin = margin;

    // Copy Border
    newBox.borderEdge = borderEdge;
    newBox.decoration = decoration;
    newBox.cssBoxDecoration = cssBoxDecoration;
    newBox.position = position;
    newBox.configuration = configuration;
    newBox.boxPainter = boxPainter;

    // Copy overflow
    newBox.scrollListener = scrollListener;
    newBox.clipX = clipX;
    newBox.clipY = clipY;
    newBox.enableScrollX = enableScrollX;
    newBox.enableScrollY = enableScrollY;
    newBox.scrollOffsetX = scrollOffsetX;
    newBox.scrollOffsetY = scrollOffsetY;

    // Copy pointer listener
    newBox.onPointerDown = onPointerDown;
    newBox.onPointerCancel = onPointerCancel;
    newBox.onPointerUp = onPointerUp;
    newBox.onPointerMove = onPointerMove;
    newBox.onPointerSignal = onPointerSignal;

    // Copy transform
    newBox.transform = transform;
    newBox.origin = origin;
    newBox.alignment = alignment;

    // Copy display
    newBox.display = display;

    // Copy ContentVisibility
    newBox.contentVisibility = contentVisibility;

    // Copy renderPositionHolder
    newBox.renderPositionHolder = renderPositionHolder;
    if (renderPositionHolder != null) {
      renderPositionHolder.realDisplayedBox = newBox;
    }

    // Copy parentData
    newBox.parentData = parentData;

    return newBox;
  }

  double _width;
  double get width {
    return _width;
  }
  set width(double value) {
    if (_width == value) return;
    _width = value;
    markNeedsLayout();
  }

  double _height;
  double get height {
    return _height;
  }
  set height(double value) {
    if (_height == value) return;
    _height = value;
    markNeedsLayout();
  }

  // Boxes which have intrinsic ratio
  double _intrinsicWidth;
  double get intrinsicWidth {
    return _intrinsicWidth;
  }
  set intrinsicWidth(double value) {
    if (_intrinsicWidth == value) return;
    _intrinsicWidth = value;
    markNeedsLayout();
  }

  // Boxes which have intrinsic ratio
  double _intrinsicHeight;
  double get intrinsicHeight {
    return _intrinsicHeight;
  }
  set intrinsicHeight(double value) {
    if (_intrinsicHeight == value) return;
    _intrinsicHeight = value;
    markNeedsLayout();
  }

  double _intrinsicRatio;
  double get intrinsicRatio {
    return _intrinsicRatio;
  }
  set intrinsicRatio(double value) {
    if (_intrinsicRatio == value) return;
    _intrinsicRatio = value;
    markNeedsLayout();
  }

  double _minWidth;
  double get minWidth {
    return _minWidth;
  }
  set minWidth(double value) {
    if (_minWidth == value) return;
    _minWidth = value;
    markNeedsLayout();
  }

  double _maxWidth;
  double get maxWidth {
    return _maxWidth;
  }
  set maxWidth(double value) {
    if (_maxWidth == value) return;
    _maxWidth = value;
    markNeedsLayout();
  }

  double _minHeight;
  double get minHeight {
    return _minHeight;
  }
  set minHeight(double value) {
    if (_minHeight == value) return;
    _minHeight = value;
    markNeedsLayout();
  }

  double _maxHeight;
  double get maxHeight {
    return _maxHeight;
  }
  set maxHeight(double value) {
    if (_maxHeight == value) return;
    _maxHeight = value;
    markNeedsLayout();
  }


  double getContentWidth() {
    double cropWidth = 0;
    // @FIXME, need to remove elementManager in the future.
    Element hostElement = elementManager.getEventTargetByTargetId<Element>(targetId);
    CSSStyleDeclaration style = hostElement.style;
    CSSDisplay display = CSSSizing.getElementRealDisplayValue(targetId, elementManager);
    double width = _width;

    void cropMargin(Element childNode) {
      RenderBoxModel renderBoxModel = childNode.getRenderBoxModel();
      if (renderBoxModel.margin != null) {
        cropWidth += renderBoxModel.margin.horizontal;
      }
    }

    void cropPaddingBorder(Element childNode) {
      RenderBoxModel renderBoxModel = childNode.getRenderBoxModel();
      if (renderBoxModel.borderEdge != null) {
        cropWidth += renderBoxModel.borderEdge.horizontal;
      }
      if (renderBoxModel.padding != null) {
        cropWidth += renderBoxModel.padding.horizontal;
      }
    }

    switch (display) {
      case CSSDisplay.block:
      case CSSDisplay.flex:
        // Get own width if exists else get the width of nearest ancestor width width
        if (style.contains(WIDTH)) {
          cropPaddingBorder(hostElement);
        } else {
          while (true) {
            if (hostElement.parentNode != null) {
              cropMargin(hostElement);
              cropPaddingBorder(hostElement);
              hostElement = hostElement.parentNode;
            } else {
              break;
            }
            if (hostElement is Element) {
              CSSStyleDeclaration style = hostElement.style;
              CSSDisplay display = CSSSizing.getElementRealDisplayValue(hostElement.targetId, elementManager);

              // Set width of element according to parent display
              if (display != CSSDisplay.inline) {
                // Skip to find upper parent
                if (style.contains(WIDTH)) {
                  // Use style width
                  width = CSSLength.toDisplayPortValue(style[WIDTH]) ?? 0;
                  cropPaddingBorder(hostElement);
                  break;
                } else if (display == CSSDisplay.inlineBlock || display == CSSDisplay.inlineFlex) {
                  // Collapse width to children
                  width = null;
                  break;
                }
              }
            }
          }
        }
        break;
      case CSSDisplay.inlineBlock:
      case CSSDisplay.inlineFlex:
        if (style.contains(WIDTH)) {
          width = CSSLength.toDisplayPortValue(style[WIDTH]) ?? 0;
          cropPaddingBorder(hostElement);
        } else {
          width = null;
        }
        break;
      case CSSDisplay.inline:
        width = null;
        break;
      default:
        break;
    }

    if (maxWidth != null) {
      if (width == null) {
        if (intrinsicWidth == null || intrinsicWidth > maxWidth) {
          width = maxWidth;
        } else {
          width = intrinsicWidth;
        }
      } else if (width > maxWidth) {
        width = maxWidth;
      }
    }

    if (minWidth != null && minWidth > 0.0) {
      if (width == null) {
        if (intrinsicWidth == null || intrinsicWidth < minWidth) {
          width = minWidth;
        } else {
          width = intrinsicWidth;
        }
      } else if (width < minWidth) {
        width = minWidth;
      }
    }

    if (width == null && intrinsicRatio != null && heightSizeType == BoxSizeType.specified) {
      double height = getContentHeight();
      width = height * intrinsicRatio;
    }

    if (width != null) {
      return math.max(0, width - cropWidth);
    } else {
      return null;
    }
  }

  double getContentHeight() {
    Element hostElement = elementManager.getEventTargetByTargetId<Element>(targetId);
    CSSStyleDeclaration style = hostElement.style;
    CSSDisplay display = CSSSizing.getElementRealDisplayValue(targetId, elementManager);

    double height = _height;
    double cropHeight = 0;

    void cropMargin(Element childNode) {
      RenderBoxModel renderBoxModel = childNode.getRenderBoxModel();
      if (renderBoxModel.margin != null) {
        cropHeight += renderBoxModel.margin.vertical;
      }
    }

    void cropPaddingBorder(Element childNode) {
      RenderBoxModel renderBoxModel = childNode.getRenderBoxModel();
      if (renderBoxModel.borderEdge != null) {
        cropHeight += renderBoxModel.borderEdge.vertical;
      }
      if (renderBoxModel.padding != null) {
        cropHeight += renderBoxModel.padding.vertical;
      }
    }

    // Inline element has no height
    if (display == CSSDisplay.inline) {
      return null;
    } else if (style.contains(HEIGHT)) {
      cropPaddingBorder(hostElement);
    } else {
      while (true) {
        Element current;
        if (hostElement.parentNode != null) {
          cropMargin(hostElement);
          cropPaddingBorder(hostElement);
          current = hostElement;
          hostElement = hostElement.parentNode;
        } else {
          break;
        }
        if (hostElement is Element) {
          CSSStyleDeclaration style = hostElement.style;
          if (CSSSizing.isStretchChildHeight(hostElement, current)) {
            if (style.contains(HEIGHT)) {
              height = CSSLength.toDisplayPortValue(style[HEIGHT]) ?? 0;
              cropPaddingBorder(hostElement);
              break;
            }
          } else {
            break;
          }
        }
      }
    }

    if (maxHeight != null) {
      if (height == null) {
        if (intrinsicHeight == null || intrinsicHeight > maxHeight) {
          height = maxHeight;
        } else {
          height = intrinsicHeight;
        }
      } else if (height > maxHeight) {
        height = maxHeight;
      }
    }

    if (minHeight != null && minHeight > 0.0) {
      if (height == null) {
        if (intrinsicHeight == null || intrinsicHeight < minHeight) {
          height = minHeight;
        } else {
          height = intrinsicHeight;
        }
      } else if (height < minHeight) {
        height = minHeight;
      }
    }

    if (height == null && intrinsicRatio != null && widthSizeType == BoxSizeType.specified) {
      double width = getContentWidth();
      height = width * intrinsicRatio;
    }

    if (height != null) {
      return math.max(0, height - cropHeight);
    } else {
      return null;
    }
  }

  Size getBoxSize(Size contentSize) {
    // Set scrollable size from unconstrained size.
    maxScrollableX = contentSize.width + paddingLeft + paddingRight;
    maxScrollableY = contentSize.height + paddingTop + paddingBottom;

    Size boxSize = _contentSize = contentConstraints.constrain(contentSize);

    scrollableViewportWidth = _contentSize.width + paddingLeft + paddingRight;
    scrollableViewportHeight = _contentSize.height + paddingTop + paddingBottom;

    if (padding != null) {
      boxSize = wrapPaddingSize(boxSize);
    }
    if (borderEdge != null) {
      boxSize = wrapBorderSize(boxSize);
    }

    return constraints.constrain(boxSize);
  }

  // The contentSize of layout box
  Size _contentSize;
  Size get contentSize {
    if (_contentSize == null) {
      return Size(0, 0);
    }
    return _contentSize;
  }

  double get clientWidth {
    double width = contentSize.width;
    if (padding != null) {
      width += padding.horizontal;
    }
    return width;
  }

  double get clientHeight {
    double height = contentSize.height;
    if (padding != null) {
      height += padding.vertical;
    }
    return height;
  }

  // Base layout methods to compute content constraints before content box layout.
  // Call this method before content box layout.
  BoxConstraints beforeLayout() {
    _debugHasBoxLayout = true;
    BoxConstraints boxConstraints = constraints;
    // Deflate border constraints.
    boxConstraints = deflateBorderConstraints(boxConstraints);

    // Deflate overflow constraints.
    boxConstraints = deflateOverflowConstraints(boxConstraints);

    // Deflate padding constraints.
    boxConstraints = deflatePaddingConstraints(boxConstraints);

    final double contentWidth = getContentWidth();
    final double contentHeight = getContentHeight();

    if (contentWidth != null || contentHeight != null) {
      double minWidth;
      double maxWidth;
      double minHeight;
      double maxHeight;

      if (boxConstraints.hasTightWidth) {
        minWidth = maxWidth = boxConstraints.maxWidth;
      } else if (contentWidth != null) {
        minWidth = 0.0;
        maxWidth = contentWidth;
      } else {
        minWidth = 0.0;
        maxWidth = constraints.maxWidth;
      }

      if (boxConstraints.hasTightHeight) {
        minHeight = maxHeight = boxConstraints.maxHeight;
      } else if (contentHeight != null) {
        minHeight = 0.0;
        maxHeight = contentHeight;
      } else {
        minHeight = 0.0;
        maxHeight = boxConstraints.maxHeight;
      }

      _contentConstraints = BoxConstraints(
          minWidth: minWidth,
          maxWidth: maxWidth,
          minHeight: minHeight,
          maxHeight: maxHeight
      );
    } else {
      _contentConstraints = boxConstraints;
    }

    return _contentConstraints;
  }

  @override
  void applyPaintTransform(RenderBox child, Matrix4 transform) {
    super.applyPaintTransform(child, transform);
    applyOverflowPaintTransform(child, transform);
    applyEffectiveTransform(child, transform);
  }

  // The max scrollable size of X axis.
  double maxScrollableX;
  // The max scrollable size of Y axis.
  double maxScrollableY;

  double scrollableViewportWidth;
  double scrollableViewportHeight;

  // hooks when content box had layout.
  void didLayout() {
    Size scrollableSize = Size(maxScrollableX, maxScrollableY);
    Size viewportSize = Size(scrollableViewportWidth, scrollableViewportHeight);
    setUpOverflowScroller(scrollableSize, viewportSize);

    if (positionedHolder != null) {
      // Make position holder preferred size equal to current element boundary size.
      positionedHolder.preferredSize = Size.copy(size);
    }
  }

  void setMaximumScrollableHeightForPositionedChild(RenderLayoutParentData childParentData, Size childSize) {
    if (childParentData.top != null) {
      maxScrollableY = math.max(maxScrollableY, childParentData.top + childSize.height);
    }
    if (childParentData.bottom != null) {
      maxScrollableY = math.max(maxScrollableY, -childParentData.bottom + _contentSize.height);
    }
  }

  void setMaximumScrollableWidthForPositionedChild(RenderLayoutParentData childParentData, Size childSize) {
    if (childParentData.left != null) {
      maxScrollableX = math.max(maxScrollableX, childParentData.left + childSize.width);
    }

    if (childParentData.right != null) {
      maxScrollableX = math.max(maxScrollableX, -childParentData.right + _contentSize.width);
    }
  }

  void basePaint(PaintingContext context, Offset offset, PaintingContextCallback callback) {
    if (display != null && display == CSSDisplay.none) return;

    paintIntersectionObserver(context, offset, (PaintingContext context, Offset offset) {
      paintTransform(context, offset, (PaintingContext context, Offset offset) {
        paintOpacity(context, offset, (context, offset) {
          paintDecoration(context, offset);
          paintOverflow(
              context,
              offset,
              EdgeInsets.fromLTRB(borderLeft, borderTop, borderRight, borderLeft),
              decoration,
              Size(scrollableViewportWidth, scrollableViewportHeight),
              (context, offset) {
                paintContentVisibility(context, offset, callback);
              }
          );
        });
      });
    });
  }

  @override
  void detach() {
    disposePainter();
    super.detach();
  }

  @override
  bool hitTest(BoxHitTestResult result, { @required Offset position }) {
    if (!contentVisibilityHitTest(result, position: position)) {
      return false;
    }

    assert(() {
      if (!hasSize) {
        if (debugNeedsLayout) {
          throw FlutterError.fromParts(<DiagnosticsNode>[
            ErrorSummary('Cannot hit test a render box that has never been laid out.'),
            describeForError('The hitTest() method was called on this RenderBox'),
            ErrorDescription("Unfortunately, this object's geometry is not known at this time, "
              'probably because it has never been laid out. '
              'This means it cannot be accurately hit-tested.'),
            ErrorHint('If you are trying '
              'to perform a hit test during the layout phase itself, make sure '
              "you only hit test nodes that have completed layout (e.g. the node's "
              'children, after their layout() method has been called).'),
          ]);
        }
        throw FlutterError.fromParts(<DiagnosticsNode>[
          ErrorSummary('Cannot hit test a render box with no size.'),
          describeForError('The hitTest() method was called on this RenderBox'),
          ErrorDescription('Although this node is not marked as needing layout, '
            'its size is not set.'),
          ErrorHint('A RenderBox object must have an '
            'explicit size before it can be hit-tested. Make sure '
            'that the RenderBox in question sets its size during layout.'),
        ]);
      }
      return true;
    }());
    if (hitTestChildren(result, position: position) || hitTestSelf(position)) {
      result.add(BoxHitTestEntry(this, position));
      return true;
    }
    return false;
  }

  @override
  bool hitTestSelf(Offset position) {
    return size.contains(position);
  }

  Future<Image> toImage({ double pixelRatio = 1.0 }) {
    assert(!debugNeedsPaint);
    assert(isRepaintBoundary);
    final OffsetLayer offsetLayer = layer as OffsetLayer;
    return offsetLayer.toImage(Offset.zero & size, pixelRatio: pixelRatio);
  }

  @override
  void debugFillProperties(DiagnosticPropertiesBuilder properties) {
    super.debugFillProperties(properties);
    if (decoration != null) properties.add(decoration.toDiagnosticsNode(name: 'decoration'));
    if (configuration != null) properties.add(DiagnosticsProperty<ImageConfiguration>('configuration', configuration));
    properties.add(DiagnosticsProperty('clipX', clipX));
    properties.add(DiagnosticsProperty('clipY', clipY));
    properties.add(DiagnosticsProperty('padding', padding));
    properties.add(DiagnosticsProperty('width', width));
    properties.add(DiagnosticsProperty('height', height));
    properties.add(DiagnosticsProperty('intrinsicWidth', intrinsicWidth));
    properties.add(DiagnosticsProperty('intrinsicHeight', intrinsicHeight));
    properties.add(DiagnosticsProperty('intrinsicRatio', intrinsicRatio));
    properties.add(DiagnosticsProperty('maxWidth', maxWidth));
    properties.add(DiagnosticsProperty('minWidth', minWidth));
    properties.add(DiagnosticsProperty('maxHeight', maxHeight));
    properties.add(DiagnosticsProperty('minHeight', minHeight));
    properties.add(DiagnosticsProperty('contentSize', _contentSize));
    properties.add(DiagnosticsProperty('contentConstraints', _contentConstraints));
  }
}
