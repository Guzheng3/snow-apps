mod engine;
mod history;
mod session;

pub mod arrow {
    pub use snow_draw_engine_core::arrow::{
        ArrowEndpointPosition, ArrowPathCommand, ArrowStrokeStyle, ArrowType, Arrowhead,
        ArrowheadFillMode, ArrowheadRenderPrimitive,
    };
    pub use snow_draw_engine_document::ArrowData;
}

pub use engine::{
    Engine, EngineConfig as RuntimeConfig, InputUpdate, MutationResult, StyleDefaults,
    TextElementInfo, ViewportConfig, ViewportId,
};
pub use snow_draw_engine_core::arrow::{ArrowPathCommand, ArrowStrokeStyle, ArrowType, Arrowhead};
pub use snow_draw_engine_core::*;
pub use snow_draw_engine_display::*;
pub use snow_draw_engine_document::{
    AnchorKind, AnchorRef, ArrowData, Binding, CanvasFilterType, ConnectorEndpoint,
    DEFAULT_ARROW_MAX_COORDINATE, DocumentRevision, ElementData, ElementId, ElementKind,
    ElementMeta, ElementRecord, FillStyle, FilterData, HighlightShape, Operation, RectangleData,
    RectangleElementKind, SerialNumberData, SerialNumberTextConnection, SpotlightConfig,
    StrokeStyle, TextData, TextHorizontalAlign, TextLayoutRect, TextLayoutSize, TextVerticalAlign,
    Transaction, WatermarkConfig, arrow_bounds, arrow_global_points, arrow_is_degenerate,
    arrow_length, arrow_point_global, arrow_segment_midpoints, element_id_key,
    normalize_corner_radii, normalize_font_family, resolve_serial_number_diameter,
    serial_number_bound_text_rect, text_with_content_and_layout, validate_arrow,
    validate_rectangle, validate_serial_number, validate_text,
};
pub use snow_draw_engine_editor::{
    ActiveTextDraftPresentation, ActiveTextDraftTarget, ActiveTool, ApplyTransactionCommand,
    ArrowStyle, DocumentSyncSnapshot, Editor, EditorCommand, EditorPresentationState,
    EditorSession, EditorSessionSnapshot, EditorStyleDefaults, EditorUpdate, EditorViewState,
    EditorViewportState, FILTER_STYLE_PROPERTY_ALL, FILTER_STYLE_PROPERTY_OPACITY,
    FILTER_STYLE_PROPERTY_STRENGTH, FILTER_STYLE_PROPERTY_TYPE, FilterStyle, HistoryState,
    RectangleShapeStyle, SERIAL_NUMBER_STYLE_MIXED_COLOR, SERIAL_NUMBER_STYLE_MIXED_FILL,
    SERIAL_NUMBER_STYLE_MIXED_FILL_STYLE, SERIAL_NUMBER_STYLE_MIXED_FONT_FAMILY,
    SERIAL_NUMBER_STYLE_MIXED_FONT_SIZE, SERIAL_NUMBER_STYLE_MIXED_NUMBER,
    SERIAL_NUMBER_STYLE_MIXED_OPACITY, SERIAL_NUMBER_STYLE_MIXED_STROKE_STYLE,
    SERIAL_NUMBER_STYLE_MIXED_STROKE_WIDTH, SHAPE_STYLE_MIXED_HIGHLIGHT_SHAPE,
    SHAPE_STYLE_PROPERTY_ARROW, SHAPE_STYLE_PROPERTY_HIGHLIGHT_SHAPE,
    SHAPE_STYLE_PROPERTY_RECTANGLE, SelectionBounds, SelectionRectState, SerialNumberStyle,
    SerialNumberTextOperation, SerialNumberToolbarState, ShapeKind, ShapeStyle, ShapeStylePatch,
    StyleToolbarSource, StyleToolbarState, TextCommitTarget, TextDraftCommit, TextLayoutOverride,
    TextStyle,
};
pub use snow_draw_engine_interaction::*;
pub use snow_draw_engine_model::DocumentModel;
pub use snow_draw_engine_scene::{DocumentSceneCache, ViewportComposer};

pub type Runtime = Engine;
