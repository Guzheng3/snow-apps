use crate::{
    ArrowPatch, ArrowState, ElementId, ElementKind, FillStyle, FixedPointBinding,
    arrow_engine as editing, arrow_geom as geometry, arrow_render_core as rendering,
    element_schema::{ARROW_TYPE_ID, LINE_TYPE_ID, PEN_HIGHLIGHT_TYPE_ID, field},
};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Number, Value};
use snow_draw_engine_core::arrow::{
    ArrowEndpointPosition, ArrowPathCommand, ArrowStrokeStyle, ArrowType, Arrowhead,
    ArrowheadFillMode, ArrowheadRenderPrimitive, BindMode, CurvePathOp, FixedSegment,
};
use snow_draw_engine_core::{ColorRgba8, DrawRect, ErrorCode, Point};

pub const DEFAULT_ARROW_MAX_COORDINATE: f64 = 1e6;
const ELBOW_ARROW_CORNER_RADIUS: f64 = 16.0;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum LinearElementKind {
    #[default]
    Arrow,
    Line,
    PenHighlight,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ArrowEndpointBinding {
    pub element_id: ElementId,
    pub fixed_point: [f64; 2],
    pub mode: snow_draw_engine_core::arrow::BindMode,
}

impl ArrowEndpointBinding {
    pub(crate) fn from_fixed_point_binding(binding: &FixedPointBinding) -> Self {
        Self {
            element_id: binding.element_id,
            fixed_point: binding.fixed_point,
            mode: binding.mode,
        }
    }

    pub(crate) fn to_fixed_point_binding(&self) -> FixedPointBinding {
        FixedPointBinding {
            element_id: self.element_id,
            fixed_point: self.fixed_point,
            mode: self.mode,
        }
    }
}

impl From<ArrowEndpointBinding> for FixedPointBinding {
    fn from(binding: ArrowEndpointBinding) -> Self {
        binding.to_fixed_point_binding()
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ArrowData {
    pub linear_kind: LinearElementKind,
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
    /// The accumulated canvas rotation transform applied to this arrow, in radians.
    ///
    /// Arrow points remain stored in their rendered canvas positions. Keeping the
    /// angle separately preserves the transform after a selection edit commits.
    pub rotation: f64,
    pub points: Vec<[f64; 2]>,
    pub start_binding: Option<ArrowEndpointBinding>,
    pub end_binding: Option<ArrowEndpointBinding>,
    pub start_arrowhead: Option<Arrowhead>,
    pub end_arrowhead: Option<Arrowhead>,
    pub arrow_type: ArrowType,
    pub fixed_segments: Option<Vec<FixedSegment>>,
    pub start_is_special: Option<bool>,
    pub end_is_special: Option<bool>,
    pub stroke: ColorRgba8,
    pub stroke_width: f64,
    pub stroke_style: ArrowStrokeStyle,
    pub fill: ColorRgba8,
    pub fill_style: FillStyle,
    pub opacity: f64,
}

impl ArrowData {
    pub fn from_global_points(
        points: &[Point<f64>],
        stroke: ColorRgba8,
        stroke_width: f64,
        stroke_style: ArrowStrokeStyle,
        arrow_type: ArrowType,
        start_arrowhead: Option<Arrowhead>,
        end_arrowhead: Option<Arrowhead>,
    ) -> Option<Self> {
        if points.len() < 2 {
            return None;
        }
        let normalized = geometry::normalize_arrow_from_global_points(
            &points
                .iter()
                .map(|point| [point.x, point.y])
                .collect::<Vec<_>>(),
            DEFAULT_ARROW_MAX_COORDINATE,
        );
        Some(Self {
            linear_kind: LinearElementKind::Arrow,
            x: normalized.x,
            y: normalized.y,
            width: normalized.width,
            height: normalized.height,
            rotation: 0.0,
            points: normalized.points,
            start_binding: None,
            end_binding: None,
            start_arrowhead,
            end_arrowhead,
            arrow_type,
            fixed_segments: None,
            start_is_special: None,
            end_is_special: None,
            stroke,
            stroke_width,
            stroke_style,
            fill: ColorRgba8::default(),
            fill_style: FillStyle::Solid,
            opacity: 1.0,
        })
    }

    pub fn into_line(mut self, fill: ColorRgba8, fill_style: FillStyle) -> Self {
        self.linear_kind = LinearElementKind::Line;
        self.start_arrowhead = None;
        self.end_arrowhead = None;
        self.arrow_type = ArrowType::Curve;
        self.fill = fill;
        self.fill_style = fill_style;
        self
    }

    pub fn into_pen_highlight(mut self) -> Self {
        self.linear_kind = LinearElementKind::PenHighlight;
        self.start_arrowhead = None;
        self.end_arrowhead = None;
        self.arrow_type = ArrowType::Straight;
        self.stroke_style = ArrowStrokeStyle::Solid;
        self.fill = ColorRgba8::default();
        self.fill_style = FillStyle::Solid;
        self.start_binding = None;
        self.end_binding = None;
        self.fixed_segments = None;
        self.start_is_special = None;
        self.end_is_special = None;
        self
    }

    pub fn to_arrow_json_payload(&self) -> Result<Map<String, Value>, ErrorCode> {
        if self.linear_kind != LinearElementKind::Arrow || validate_arrow(self).is_err() {
            return Err(ErrorCode::InvalidArgument);
        }
        let (min_x, min_y, max_x, max_y) = point_extents(&self.points)?;
        let width = max_x - min_x;
        let height = max_y - min_y;
        let mut payload = Map::new();
        payload.insert(
            field::TYPE_ID.to_owned(),
            Value::String(ARROW_TYPE_ID.to_owned()),
        );
        payload.insert(
            field::POINTS.to_owned(),
            normalized_points_json(&self.points, min_x, min_y, width, height)?,
        );
        payload.insert(field::COLOR.to_owned(), argb_json_value(self.stroke));
        payload.insert(
            field::STROKE_WIDTH.to_owned(),
            json_number(self.stroke_width)?,
        );
        payload.insert(
            field::STROKE_STYLE.to_owned(),
            Value::String(stroke_style_json_name(self.stroke_style).to_owned()),
        );
        payload.insert(
            field::ARROW_TYPE.to_owned(),
            Value::String(arrow_type_json_name(self.arrow_type).to_owned()),
        );
        payload.insert(
            field::START_ARROWHEAD.to_owned(),
            Value::String(arrowhead_json_name(self.start_arrowhead)?.to_owned()),
        );
        payload.insert(
            field::END_ARROWHEAD.to_owned(),
            Value::String(arrowhead_json_name(self.end_arrowhead)?.to_owned()),
        );
        payload.insert(
            field::START_BINDING.to_owned(),
            binding_json_value(self.start_binding.as_ref())?,
        );
        payload.insert(
            field::END_BINDING.to_owned(),
            binding_json_value(self.end_binding.as_ref())?,
        );
        payload.insert(
            field::FIXED_SEGMENTS.to_owned(),
            fixed_segments_json_value(self.fixed_segments.as_deref(), min_x, min_y, width, height)?,
        );
        payload.insert(
            field::START_IS_SPECIAL.to_owned(),
            self.start_is_special
                .map(Value::Bool)
                .unwrap_or(Value::Null),
        );
        payload.insert(
            field::END_IS_SPECIAL.to_owned(),
            self.end_is_special.map(Value::Bool).unwrap_or(Value::Null),
        );
        Ok(payload)
    }

    pub fn from_arrow_json_payload(
        payload: &Map<String, Value>,
        bounds: DrawRect,
    ) -> Result<Self, ErrorCode> {
        if required_json_string(payload, field::TYPE_ID)? != ARROW_TYPE_ID {
            return Err(ErrorCode::InvalidArgument);
        }
        validate_payload_bounds(bounds)?;
        let points = decode_normalized_points_json(payload, bounds)?;
        let stroke_width = required_json_f64(payload, field::STROKE_WIDTH)?;
        if stroke_width < 0.0 {
            return Err(ErrorCode::InvalidArgument);
        }
        let mut arrow = Self::from_global_points(
            &points,
            required_json_color(payload, field::COLOR)?,
            stroke_width,
            decode_stroke_style_json(required_json_string(payload, field::STROKE_STYLE)?)?,
            decode_arrow_type_json(required_json_string(payload, field::ARROW_TYPE)?)?,
            decode_arrowhead_json(required_json_string(payload, field::START_ARROWHEAD)?)?,
            decode_arrowhead_json(required_json_string(payload, field::END_ARROWHEAD)?)?,
        )
        .ok_or(ErrorCode::InvalidArgument)?;
        arrow.start_binding = decode_binding_json(payload, field::START_BINDING)?;
        arrow.end_binding = decode_binding_json(payload, field::END_BINDING)?;
        arrow.fixed_segments = decode_fixed_segments_json(payload, field::FIXED_SEGMENTS, bounds)?;
        arrow.start_is_special = required_nullable_bool(payload, field::START_IS_SPECIAL)?;
        arrow.end_is_special = required_nullable_bool(payload, field::END_IS_SPECIAL)?;
        validate_arrow(&arrow)?;
        Ok(arrow)
    }

    pub fn inherit_linear_metadata_from(&mut self, source: &Self) {
        self.linear_kind = source.linear_kind;
        self.fill = source.fill;
        self.fill_style = source.fill_style;
        self.opacity = source.opacity;
        if self.is_line() || self.is_pen_highlight() {
            self.start_arrowhead = None;
            self.end_arrowhead = None;
            self.arrow_type = if self.is_pen_highlight() {
                ArrowType::Straight
            } else {
                ArrowType::Curve
            };
        }
    }

    pub fn to_pen_highlight_json_payload(&self) -> Result<Map<String, Value>, ErrorCode> {
        if !self.is_pen_highlight() || self.points.len() != 2 || validate_arrow(self).is_err() {
            return Err(ErrorCode::InvalidArgument);
        }
        let (min_x, min_y, max_x, max_y) = point_extents(&self.points)?;
        let mut payload = Map::new();
        payload.insert(
            field::TYPE_ID.to_owned(),
            Value::String(PEN_HIGHLIGHT_TYPE_ID.to_owned()),
        );
        payload.insert(
            field::POINTS.to_owned(),
            normalized_points_json(&self.points, min_x, min_y, max_x - min_x, max_y - min_y)?,
        );
        payload.insert(field::COLOR.to_owned(), argb_json_value(self.stroke));
        payload.insert(
            field::STROKE_WIDTH.to_owned(),
            json_number(self.stroke_width)?,
        );
        Ok(payload)
    }

    pub fn from_pen_highlight_json_payload(
        payload: &Map<String, Value>,
        bounds: DrawRect,
    ) -> Result<Self, ErrorCode> {
        if required_json_string(payload, field::TYPE_ID)? != PEN_HIGHLIGHT_TYPE_ID {
            return Err(ErrorCode::InvalidArgument);
        }
        let point_values = payload
            .get(field::POINTS)
            .and_then(Value::as_array)
            .filter(|points| points.len() == 2)
            .ok_or(ErrorCode::InvalidArgument)?;
        let points = point_values
            .iter()
            .map(|value| {
                let point = value.as_object().ok_or(ErrorCode::InvalidArgument)?;
                let x = required_json_f64(point, "x")?;
                let y = required_json_f64(point, "y")?;
                if !(0.0..=1.0).contains(&x) || !(0.0..=1.0).contains(&y) {
                    return Err(ErrorCode::InvalidArgument);
                }
                Ok(Point::new(
                    bounds.min_x + x * (bounds.max_x - bounds.min_x),
                    bounds.min_y + y * (bounds.max_y - bounds.min_y),
                ))
            })
            .collect::<Result<Vec<_>, ErrorCode>>()?;
        let stroke_width = required_json_f64(payload, field::STROKE_WIDTH)?;
        if !(stroke_width.is_finite() && stroke_width > 0.0) {
            return Err(ErrorCode::InvalidArgument);
        }
        let pen = Self::from_global_points(
            &points,
            required_json_color(payload, field::COLOR)?,
            stroke_width,
            ArrowStrokeStyle::Solid,
            ArrowType::Straight,
            None,
            None,
        )
        .ok_or(ErrorCode::InvalidArgument)?
        .into_pen_highlight();
        validate_arrow(&pen)?;
        Ok(pen)
    }

    pub fn to_line_json_payload(&self) -> Result<Map<String, Value>, ErrorCode> {
        if !self.is_line() || validate_arrow(self).is_err() {
            return Err(ErrorCode::InvalidArgument);
        }
        let (min_x, min_y, max_x, max_y) = self.points.iter().fold(
            (
                f64::INFINITY,
                f64::INFINITY,
                f64::NEG_INFINITY,
                f64::NEG_INFINITY,
            ),
            |(min_x, min_y, max_x, max_y), point| {
                (
                    min_x.min(point[0]),
                    min_y.min(point[1]),
                    max_x.max(point[0]),
                    max_y.max(point[1]),
                )
            },
        );
        let width = max_x - min_x;
        let height = max_y - min_y;
        let mut payload = Map::new();
        payload.insert(
            field::TYPE_ID.to_owned(),
            Value::String(LINE_TYPE_ID.to_owned()),
        );
        payload.insert(
            field::POINTS.to_owned(),
            Value::Array(
                self.points
                    .iter()
                    .map(|point| {
                        let mut value = Map::new();
                        value.insert(
                            "x".to_owned(),
                            json_number(normalize_axis(point[0] - min_x, width))?,
                        );
                        value.insert(
                            "y".to_owned(),
                            json_number(normalize_axis(point[1] - min_y, height))?,
                        );
                        Ok(Value::Object(value))
                    })
                    .collect::<Result<Vec<_>, ErrorCode>>()?,
            ),
        );
        payload.insert(field::COLOR.to_owned(), argb_json_value(self.stroke));
        payload.insert(field::FILL_COLOR.to_owned(), argb_json_value(self.fill));
        payload.insert(
            field::STROKE_WIDTH.to_owned(),
            json_number(self.stroke_width)?,
        );
        payload.insert(
            field::STROKE_STYLE.to_owned(),
            Value::String(stroke_style_json_name(self.stroke_style).to_owned()),
        );
        payload.insert(
            field::FILL_STYLE.to_owned(),
            Value::String(fill_style_json_name(self.fill_style).to_owned()),
        );
        payload.insert(
            field::ARROW_TYPE.to_owned(),
            Value::String("curved".to_owned()),
        );
        payload.insert(
            field::START_ARROWHEAD.to_owned(),
            Value::String("none".to_owned()),
        );
        payload.insert(
            field::END_ARROWHEAD.to_owned(),
            Value::String("none".to_owned()),
        );
        payload.insert(
            field::START_BINDING.to_owned(),
            binding_json_value(self.start_binding.as_ref())?,
        );
        payload.insert(
            field::END_BINDING.to_owned(),
            binding_json_value(self.end_binding.as_ref())?,
        );
        payload.insert(
            field::FIXED_SEGMENTS.to_owned(),
            fixed_segments_json_value(self.fixed_segments.as_deref(), min_x, min_y, width, height)?,
        );
        payload.insert(
            field::START_IS_SPECIAL.to_owned(),
            self.start_is_special
                .map(Value::Bool)
                .unwrap_or(Value::Null),
        );
        payload.insert(
            field::END_IS_SPECIAL.to_owned(),
            self.end_is_special.map(Value::Bool).unwrap_or(Value::Null),
        );
        Ok(payload)
    }

    pub fn from_line_json_payload(
        payload: &Map<String, Value>,
        bounds: DrawRect,
    ) -> Result<Self, ErrorCode> {
        if required_json_string(payload, field::TYPE_ID)? != LINE_TYPE_ID
            || required_json_string(payload, field::ARROW_TYPE)? != "curved"
            || required_json_string(payload, field::START_ARROWHEAD)? != "none"
            || required_json_string(payload, field::END_ARROWHEAD)? != "none"
            || ![bounds.min_x, bounds.min_y, bounds.max_x, bounds.max_y]
                .into_iter()
                .all(f64::is_finite)
            || bounds.max_x < bounds.min_x
            || bounds.max_y < bounds.min_y
        {
            return Err(ErrorCode::InvalidArgument);
        }
        let width = bounds.max_x - bounds.min_x;
        let height = bounds.max_y - bounds.min_y;
        let point_values = payload
            .get(field::POINTS)
            .and_then(Value::as_array)
            .ok_or(ErrorCode::InvalidArgument)?;
        if point_values.len() < 2 {
            return Err(ErrorCode::InvalidArgument);
        }
        let points = point_values
            .iter()
            .map(|value| {
                let point = value.as_object().ok_or(ErrorCode::InvalidArgument)?;
                let x = required_json_f64(point, "x")?;
                let y = required_json_f64(point, "y")?;
                if !(0.0..=1.0).contains(&x) || !(0.0..=1.0).contains(&y) {
                    return Err(ErrorCode::InvalidArgument);
                }
                Ok(Point::new(
                    bounds.min_x + x * width,
                    bounds.min_y + y * height,
                ))
            })
            .collect::<Result<Vec<_>, ErrorCode>>()?;
        let stroke = required_json_color(payload, field::COLOR)?;
        let fill = required_json_color(payload, field::FILL_COLOR)?;
        let stroke_width = required_json_f64(payload, field::STROKE_WIDTH)?;
        if stroke_width < 0.0 {
            return Err(ErrorCode::InvalidArgument);
        }
        let stroke_style =
            decode_stroke_style_json(required_json_string(payload, field::STROKE_STYLE)?)?;
        let fill_style = decode_fill_style_json(required_json_string(payload, field::FILL_STYLE)?)?;
        let mut line = Self::from_global_points(
            &points,
            stroke,
            stroke_width,
            stroke_style,
            ArrowType::Curve,
            None,
            None,
        )
        .ok_or(ErrorCode::InvalidArgument)?
        .into_line(fill, fill_style);
        line.start_binding = decode_binding_json(payload, field::START_BINDING)?;
        line.end_binding = decode_binding_json(payload, field::END_BINDING)?;
        line.fixed_segments = decode_fixed_segments_json(payload, field::FIXED_SEGMENTS, bounds)?;
        line.start_is_special = required_nullable_bool(payload, field::START_IS_SPECIAL)?;
        line.end_is_special = required_nullable_bool(payload, field::END_IS_SPECIAL)?;
        validate_arrow(&line)?;
        Ok(line)
    }

    pub fn element_kind(&self) -> ElementKind {
        match self.linear_kind {
            LinearElementKind::Arrow => ElementKind::Arrow,
            LinearElementKind::Line => ElementKind::Line,
            LinearElementKind::PenHighlight => ElementKind::PenHighlight,
        }
    }

    pub fn is_line(&self) -> bool {
        self.linear_kind == LinearElementKind::Line
    }

    pub fn fill_path_is_closed(&self) -> bool {
        self.is_line() && path_is_closed_with_tolerance(&arrow_visual_points(self), 1e-9)
    }

    pub fn is_pen_highlight(&self) -> bool {
        self.linear_kind == LinearElementKind::PenHighlight
    }

    pub fn start(&self) -> Point<f64> {
        arrow_point_global(self, 0).unwrap_or_default()
    }

    pub fn end(&self) -> Point<f64> {
        arrow_point_global(self, self.points.len().saturating_sub(1)).unwrap_or_default()
    }

    pub fn global_points(&self) -> Vec<Point<f64>> {
        arrow_global_points(self)
    }

    pub fn is_elbow(&self) -> bool {
        self.arrow_type.is_elbow()
    }

    pub fn is_curve(&self) -> bool {
        self.arrow_type.is_curve()
    }

    fn curve_tension(&self) -> f64 {
        1.0
    }

    pub fn start_element_binding(&self) -> Option<ArrowEndpointBinding> {
        self.start_binding.clone()
    }

    pub fn end_element_binding(&self) -> Option<ArrowEndpointBinding> {
        self.end_binding.clone()
    }

    pub fn set_start_element_binding(&mut self, binding: Option<ArrowEndpointBinding>) {
        self.start_binding = binding;
    }

    pub fn set_end_element_binding(&mut self, binding: Option<ArrowEndpointBinding>) {
        self.end_binding = binding;
    }

    pub fn bound_element_ids(&self) -> Vec<ElementId> {
        let mut ids = Vec::new();
        for id in [
            self.start_element_binding()
                .map(|binding| binding.element_id),
            self.end_element_binding().map(|binding| binding.element_id),
        ]
        .into_iter()
        .flatten()
        {
            if !ids.contains(&id) {
                ids.push(id);
            }
        }
        ids
    }

    pub fn path_commands(&self) -> Vec<ArrowPathCommand> {
        if self.is_elbow() {
            return rendering::generate_elbow_arrow_path_commands(
                &self
                    .global_points()
                    .iter()
                    .map(|point| [point.x, point.y])
                    .collect::<Vec<_>>(),
                ELBOW_ARROW_CORNER_RADIUS,
            );
        }

        let global_points = self.global_points();
        if self.is_curve() {
            return curve_path_commands(
                &global_points,
                self.curve_tension(),
                false,
                self.fixed_segments.as_deref(),
            );
        }
        linear_path_commands(&global_points)
    }

    pub(crate) fn apply_patch(&self, patch: &ArrowPatch) -> Self {
        let points = patch.points.clone().unwrap_or_else(|| self.points.clone());
        Self {
            linear_kind: self.linear_kind,
            x: patch.x.unwrap_or(self.x),
            y: patch.y.unwrap_or(self.y),
            width: patch.width.unwrap_or(self.width),
            height: patch.height.unwrap_or(self.height),
            rotation: self.rotation,
            points,
            start_binding: match &patch.start_binding {
                Some(binding) => binding
                    .as_ref()
                    .map(ArrowEndpointBinding::from_fixed_point_binding),
                None => self.start_binding.clone(),
            },
            end_binding: match &patch.end_binding {
                Some(binding) => binding
                    .as_ref()
                    .map(ArrowEndpointBinding::from_fixed_point_binding),
                None => self.end_binding.clone(),
            },
            start_arrowhead: self.start_arrowhead,
            end_arrowhead: self.end_arrowhead,
            arrow_type: self.arrow_type,
            fixed_segments: match &patch.fixed_segments {
                Some(segments) => segments.clone(),
                None => self.fixed_segments.clone(),
            },
            start_is_special: match patch.start_is_special {
                Some(value) => value,
                None => self.start_is_special,
            },
            end_is_special: match patch.end_is_special {
                Some(value) => value,
                None => self.end_is_special,
            },
            stroke: self.stroke,
            stroke_width: self.stroke_width,
            stroke_style: self.stroke_style,
            fill: self.fill,
            fill_style: self.fill_style,
            opacity: self.opacity,
        }
    }
}

fn normalize_axis(offset: f64, extent: f64) -> f64 {
    if extent.abs() <= f64::EPSILON {
        0.0
    } else {
        offset / extent
    }
}

fn json_number(value: f64) -> Result<Value, ErrorCode> {
    Number::from_f64(value)
        .map(Value::Number)
        .ok_or(ErrorCode::InvalidArgument)
}

fn point_extents(points: &[[f64; 2]]) -> Result<(f64, f64, f64, f64), ErrorCode> {
    if points.len() < 2 || points.iter().flatten().any(|value| !value.is_finite()) {
        return Err(ErrorCode::InvalidArgument);
    }
    Ok(points.iter().fold(
        (
            f64::INFINITY,
            f64::INFINITY,
            f64::NEG_INFINITY,
            f64::NEG_INFINITY,
        ),
        |(min_x, min_y, max_x, max_y), point| {
            (
                min_x.min(point[0]),
                min_y.min(point[1]),
                max_x.max(point[0]),
                max_y.max(point[1]),
            )
        },
    ))
}

fn normalized_points_json(
    points: &[[f64; 2]],
    min_x: f64,
    min_y: f64,
    width: f64,
    height: f64,
) -> Result<Value, ErrorCode> {
    Ok(Value::Array(
        points
            .iter()
            .map(|point| {
                let mut value = Map::new();
                value.insert(
                    "x".to_owned(),
                    json_number(normalize_axis(point[0] - min_x, width))?,
                );
                value.insert(
                    "y".to_owned(),
                    json_number(normalize_axis(point[1] - min_y, height))?,
                );
                Ok(Value::Object(value))
            })
            .collect::<Result<Vec<_>, ErrorCode>>()?,
    ))
}

fn validate_payload_bounds(bounds: DrawRect) -> Result<(), ErrorCode> {
    if [bounds.min_x, bounds.min_y, bounds.max_x, bounds.max_y]
        .into_iter()
        .all(f64::is_finite)
        && bounds.max_x >= bounds.min_x
        && bounds.max_y >= bounds.min_y
    {
        Ok(())
    } else {
        Err(ErrorCode::InvalidArgument)
    }
}

fn decode_normalized_points_json(
    payload: &Map<String, Value>,
    bounds: DrawRect,
) -> Result<Vec<Point<f64>>, ErrorCode> {
    let point_values = payload
        .get(field::POINTS)
        .and_then(Value::as_array)
        .ok_or(ErrorCode::InvalidArgument)?;
    if point_values.len() < 2 {
        return Err(ErrorCode::InvalidArgument);
    }
    let width = bounds.max_x - bounds.min_x;
    let height = bounds.max_y - bounds.min_y;
    point_values
        .iter()
        .map(|value| {
            let point = value.as_object().ok_or(ErrorCode::InvalidArgument)?;
            let x = required_json_f64(point, "x")?;
            let y = required_json_f64(point, "y")?;
            if !(0.0..=1.0).contains(&x) || !(0.0..=1.0).contains(&y) {
                return Err(ErrorCode::InvalidArgument);
            }
            Ok(Point::new(
                bounds.min_x + x * width,
                bounds.min_y + y * height,
            ))
        })
        .collect()
}

fn fixed_segments_json_value(
    segments: Option<&[FixedSegment]>,
    min_x: f64,
    min_y: f64,
    width: f64,
    height: f64,
) -> Result<Value, ErrorCode> {
    let Some(segments) = segments.filter(|segments| !segments.is_empty()) else {
        return Ok(Value::Null);
    };
    Ok(Value::Array(
        segments
            .iter()
            .map(|segment| {
                let point = |point: [f64; 2]| -> Result<Value, ErrorCode> {
                    let mut value = Map::new();
                    value.insert(
                        "x".to_owned(),
                        json_number(normalize_axis(point[0] - min_x, width))?,
                    );
                    value.insert(
                        "y".to_owned(),
                        json_number(normalize_axis(point[1] - min_y, height))?,
                    );
                    Ok(Value::Object(value))
                };
                let mut value = Map::new();
                value.insert("index".to_owned(), Value::from(segment.index));
                value.insert("start".to_owned(), point(segment.start)?);
                value.insert("end".to_owned(), point(segment.end)?);
                Ok(Value::Object(value))
            })
            .collect::<Result<Vec<_>, ErrorCode>>()?,
    ))
}

fn argb_json_value(color: ColorRgba8) -> Value {
    Value::from(
        ((color.a as u64) << 24)
            | ((color.r as u64) << 16)
            | ((color.g as u64) << 8)
            | color.b as u64,
    )
}

fn required_json_string<'a>(
    payload: &'a Map<String, Value>,
    field: &str,
) -> Result<&'a str, ErrorCode> {
    payload
        .get(field)
        .and_then(Value::as_str)
        .ok_or(ErrorCode::InvalidArgument)
}

fn required_json_f64(payload: &Map<String, Value>, field: &str) -> Result<f64, ErrorCode> {
    payload
        .get(field)
        .and_then(Value::as_f64)
        .filter(|value| value.is_finite())
        .ok_or(ErrorCode::InvalidArgument)
}

fn required_json_color(payload: &Map<String, Value>, field: &str) -> Result<ColorRgba8, ErrorCode> {
    let argb = payload
        .get(field)
        .and_then(Value::as_u64)
        .filter(|value| *value <= u32::MAX as u64)
        .ok_or(ErrorCode::InvalidArgument)? as u32;
    Ok(ColorRgba8 {
        a: ((argb >> 24) & 0xff) as u8,
        r: ((argb >> 16) & 0xff) as u8,
        g: ((argb >> 8) & 0xff) as u8,
        b: (argb & 0xff) as u8,
    })
}

fn required_nullable_bool(
    payload: &Map<String, Value>,
    field: &str,
) -> Result<Option<bool>, ErrorCode> {
    match payload.get(field).ok_or(ErrorCode::InvalidArgument)? {
        Value::Null => Ok(None),
        Value::Bool(value) => Ok(Some(*value)),
        _ => Err(ErrorCode::InvalidArgument),
    }
}

fn decode_fixed_segments_json(
    payload: &Map<String, Value>,
    field: &str,
    bounds: DrawRect,
) -> Result<Option<Vec<FixedSegment>>, ErrorCode> {
    match payload.get(field).ok_or(ErrorCode::InvalidArgument)? {
        Value::Null => Ok(None),
        Value::Array(values) => {
            let width = bounds.max_x - bounds.min_x;
            let height = bounds.max_y - bounds.min_y;
            let segments = values
                .iter()
                .map(|value| {
                    let segment = value.as_object().ok_or(ErrorCode::InvalidArgument)?;
                    let index = segment
                        .get("index")
                        .and_then(Value::as_u64)
                        .and_then(|index| usize::try_from(index).ok())
                        .ok_or(ErrorCode::InvalidArgument)?;
                    let decode_point = |field: &str| -> Result<[f64; 2], ErrorCode> {
                        let point = segment
                            .get(field)
                            .and_then(Value::as_object)
                            .ok_or(ErrorCode::InvalidArgument)?;
                        let x = required_json_f64(point, "x")?;
                        let y = required_json_f64(point, "y")?;
                        if !(0.0..=1.0).contains(&x) || !(0.0..=1.0).contains(&y) {
                            return Err(ErrorCode::InvalidArgument);
                        }
                        Ok([x * width, y * height])
                    };
                    Ok(FixedSegment {
                        index,
                        start: decode_point("start")?,
                        end: decode_point("end")?,
                    })
                })
                .collect::<Result<Vec<_>, ErrorCode>>()?;
            Ok((!segments.is_empty()).then_some(segments))
        }
        _ => Err(ErrorCode::InvalidArgument),
    }
}

fn arrow_type_json_name(arrow_type: ArrowType) -> &'static str {
    match arrow_type {
        ArrowType::Straight => "straight",
        ArrowType::Curve => "curved",
        ArrowType::Elbow => "elbow",
    }
}

fn decode_arrow_type_json(value: &str) -> Result<ArrowType, ErrorCode> {
    match value {
        "straight" => Ok(ArrowType::Straight),
        "curved" => Ok(ArrowType::Curve),
        "elbow" => Ok(ArrowType::Elbow),
        _ => Err(ErrorCode::InvalidArgument),
    }
}

fn arrowhead_json_name(arrowhead: Option<Arrowhead>) -> Result<&'static str, ErrorCode> {
    match arrowhead {
        None => Ok("none"),
        Some(Arrowhead::Arrow) => Ok("standard"),
        Some(Arrowhead::Triangle) => Ok("triangle"),
        Some(Arrowhead::Circle) => Ok("circle"),
        Some(Arrowhead::Diamond) => Ok("diamond"),
        Some(Arrowhead::Bar) => Ok("verticalLine"),
        Some(Arrowhead::Square) => Ok("square"),
        Some(Arrowhead::InvertedTriangle) => Ok("invertedTriangle"),
        Some(
            Arrowhead::Dot
            | Arrowhead::CircleOutline
            | Arrowhead::TriangleOutline
            | Arrowhead::DiamondOutline
            | Arrowhead::CrowfootOne
            | Arrowhead::CrowfootMany
            | Arrowhead::CrowfootOneOrMany,
        ) => Err(ErrorCode::InvalidArgument),
    }
}

fn decode_arrowhead_json(value: &str) -> Result<Option<Arrowhead>, ErrorCode> {
    match value {
        "none" => Ok(None),
        "standard" => Ok(Some(Arrowhead::Arrow)),
        "triangle" => Ok(Some(Arrowhead::Triangle)),
        "circle" => Ok(Some(Arrowhead::Circle)),
        "diamond" => Ok(Some(Arrowhead::Diamond)),
        "verticalLine" => Ok(Some(Arrowhead::Bar)),
        "square" => Ok(Some(Arrowhead::Square)),
        "invertedTriangle" => Ok(Some(Arrowhead::InvertedTriangle)),
        _ => Err(ErrorCode::InvalidArgument),
    }
}

fn stroke_style_json_name(style: ArrowStrokeStyle) -> &'static str {
    match style {
        ArrowStrokeStyle::Solid => "solid",
        ArrowStrokeStyle::Dashed => "dashed",
        ArrowStrokeStyle::Dotted => "dotted",
    }
}

fn decode_stroke_style_json(value: &str) -> Result<ArrowStrokeStyle, ErrorCode> {
    match value {
        "solid" => Ok(ArrowStrokeStyle::Solid),
        "dashed" => Ok(ArrowStrokeStyle::Dashed),
        "dotted" => Ok(ArrowStrokeStyle::Dotted),
        _ => Err(ErrorCode::InvalidArgument),
    }
}

fn fill_style_json_name(style: FillStyle) -> &'static str {
    match style {
        FillStyle::Solid => "solid",
        FillStyle::Line => "line",
        FillStyle::CrossLine => "crossLine",
    }
}

fn decode_fill_style_json(value: &str) -> Result<FillStyle, ErrorCode> {
    match value {
        "solid" => Ok(FillStyle::Solid),
        "line" => Ok(FillStyle::Line),
        "crossLine" => Ok(FillStyle::CrossLine),
        _ => Err(ErrorCode::InvalidArgument),
    }
}

fn bind_mode_json_name(mode: BindMode) -> &'static str {
    match mode {
        BindMode::Inside => "inside",
        BindMode::Orbit => "orbit",
        BindMode::Skip => "skip",
    }
}

fn decode_bind_mode_json(value: &str) -> Result<BindMode, ErrorCode> {
    match value {
        "inside" => Ok(BindMode::Inside),
        "orbit" => Ok(BindMode::Orbit),
        "skip" => Ok(BindMode::Skip),
        _ => Err(ErrorCode::InvalidArgument),
    }
}

fn binding_json_value(binding: Option<&ArrowEndpointBinding>) -> Result<Value, ErrorCode> {
    let Some(binding) = binding else {
        return Ok(Value::Null);
    };
    if !binding.fixed_point.into_iter().all(f64::is_finite) {
        return Err(ErrorCode::InvalidArgument);
    }
    let mut fixed_point = Map::new();
    fixed_point.insert("x".to_owned(), json_number(binding.fixed_point[0])?);
    fixed_point.insert("y".to_owned(), json_number(binding.fixed_point[1])?);
    let mut value = Map::new();
    value.insert(
        "elementId".to_owned(),
        Value::String(binding.element_id.stable_key()),
    );
    value.insert("fixedPoint".to_owned(), Value::Object(fixed_point));
    value.insert(
        "mode".to_owned(),
        Value::String(bind_mode_json_name(binding.mode).to_owned()),
    );
    Ok(Value::Object(value))
}

fn decode_binding_json(
    payload: &Map<String, Value>,
    field: &str,
) -> Result<Option<ArrowEndpointBinding>, ErrorCode> {
    let value = payload.get(field).ok_or(ErrorCode::InvalidArgument)?;
    if value.is_null() {
        return Ok(None);
    }
    let binding = value.as_object().ok_or(ErrorCode::InvalidArgument)?;
    let element_id = ElementId::from_stable_key(required_json_string(binding, "elementId")?)
        .ok_or(ErrorCode::InvalidArgument)?;
    let fixed_point = binding
        .get("fixedPoint")
        .and_then(Value::as_object)
        .ok_or(ErrorCode::InvalidArgument)?;
    Ok(Some(ArrowEndpointBinding {
        element_id,
        fixed_point: [
            required_json_f64(fixed_point, "x")?,
            required_json_f64(fixed_point, "y")?,
        ],
        mode: decode_bind_mode_json(required_json_string(binding, "mode")?)?,
    }))
}

impl ArrowState {
    pub(crate) fn from_arrow_data(id: ElementId, arrow: &ArrowData) -> Self {
        Self {
            id,
            x: arrow.x,
            y: arrow.y,
            width: arrow.width,
            height: arrow.height,
            points: arrow.points.clone(),
            start_binding: arrow
                .start_binding
                .as_ref()
                .map(ArrowEndpointBinding::to_fixed_point_binding),
            end_binding: arrow
                .end_binding
                .as_ref()
                .map(ArrowEndpointBinding::to_fixed_point_binding),
            start_arrowhead: arrow.start_arrowhead,
            end_arrowhead: arrow.end_arrowhead,
            elbowed: arrow.is_elbow(),
            fixed_segments: arrow.fixed_segments.clone(),
            start_is_special: arrow.start_is_special,
            end_is_special: arrow.end_is_special,
        }
    }
}

pub(crate) fn apply_arrow_patch(arrow: &ArrowData, patch: &ArrowPatch) -> ArrowData {
    arrow.apply_patch(patch)
}

pub fn arrow_point_global(arrow: &ArrowData, index: usize) -> Option<Point<f64>> {
    let point = arrow.points.get(index)?;
    Some(Point::new(arrow.x + point[0], arrow.y + point[1]))
}

pub fn arrow_global_points(arrow: &ArrowData) -> Vec<Point<f64>> {
    arrow
        .points
        .iter()
        .map(|point| Point::new(arrow.x + point[0], arrow.y + point[1]))
        .collect()
}

pub fn arrow_is_degenerate(arrow: &ArrowData) -> bool {
    if arrow.points.len() < 2 {
        return true;
    }

    !arrow
        .points
        .windows(2)
        .any(|segment| point_arrays_distance(segment[0], segment[1]) > 1e-6)
}

pub fn arrow_length(arrow: &ArrowData) -> f64 {
    arrow_visual_points(arrow)
        .windows(2)
        .map(|segment| point_distance(segment[0], segment[1]))
        .sum()
}

pub fn arrow_segment_midpoints(arrow: &ArrowData) -> Vec<(usize, Point<f64>)> {
    let global_points = arrow_global_points(arrow);
    if arrow.is_curve() && global_points.len() >= 3 {
        return curve_bezier_segments(
            &global_points,
            arrow.curve_tension(),
            false,
            arrow.fixed_segments.as_deref(),
        )
        .into_iter()
        .enumerate()
        .map(|(index, [start, control_1, control_2, end])| {
            (
                index + 1,
                point_at_bezier(start, control_1, control_2, end, 0.5),
            )
        })
        .collect();
    }
    global_points
        .windows(2)
        .enumerate()
        .map(|(index, segment)| {
            (
                index + 1,
                Point::new(
                    f64::midpoint(segment[0].x, segment[1].x),
                    f64::midpoint(segment[0].y, segment[1].y),
                ),
            )
        })
        .collect()
}

pub fn validate_arrow(arrow: &ArrowData) -> Result<(), ErrorCode> {
    let scalar_fields = [
        arrow.x,
        arrow.y,
        arrow.width,
        arrow.height,
        arrow.rotation,
        arrow.stroke_width,
        arrow.opacity,
    ];
    if scalar_fields
        .iter()
        .any(|value| !value.is_finite() || (*value < 0.0 && value == &arrow.stroke_width))
    {
        return Err(ErrorCode::InvalidArgument);
    }
    if arrow.stroke_width < 0.0
        || arrow.opacity < 0.0
        || arrow.opacity > 1.0
        || arrow
            .points
            .iter()
            .flatten()
            .any(|value| !value.is_finite())
    {
        return Err(ErrorCode::InvalidArgument);
    }

    let report = editing::validate_arrow_invariant(&ArrowState::from_arrow_data(
        ElementId::default(),
        arrow,
    ));
    if !report.valid {
        return Err(ErrorCode::InvalidArgument);
    }
    Ok(())
}

pub fn arrow_bounds(arrow: &ArrowData) -> DrawRect {
    let global_points = arrow_visual_points(arrow);
    let half_stroke = arrow.stroke_width.max(0.0) / 2.0;

    let mut bounds = global_points
        .iter()
        .fold(None, |bounds, point| {
            Some(union_point_bounds(bounds, *point, half_stroke))
        })
        .unwrap_or_default();

    for primitive in arrowhead_render_primitives(arrow, ArrowEndpointPosition::Start) {
        bounds = union_draw_rect(bounds, arrowhead_primitive_bounds(&primitive, half_stroke));
    }
    for primitive in arrowhead_render_primitives(arrow, ArrowEndpointPosition::End) {
        bounds = union_draw_rect(bounds, arrowhead_primitive_bounds(&primitive, half_stroke));
    }
    bounds
}

pub fn arrow_hit_test(arrow: &ArrowData, point: Point<f64>, hit_tolerance: f64) -> bool {
    if arrow_is_degenerate(arrow) {
        return false;
    }

    let global_points = arrow_visual_points(arrow);
    if arrow.is_line()
        && arrow.fill.a != 0
        && arrow.opacity > 0.0
        && arrow.fill_path_is_closed()
        && point_in_polygon(&global_points, point)
    {
        return true;
    }

    if arrow.stroke.a == 0 || arrow.stroke_width <= 0.0 {
        return false;
    }

    let threshold = arrow.stroke_width / 2.0 + hit_tolerance.max(0.0);
    if global_points
        .windows(2)
        .any(|segment| distance_point_to_segment(point, segment[0], segment[1]) <= threshold)
    {
        return true;
    }

    arrowhead_render_primitives(arrow, ArrowEndpointPosition::Start)
        .into_iter()
        .chain(arrowhead_render_primitives(
            arrow,
            ArrowEndpointPosition::End,
        ))
        .any(|primitive| arrowhead_primitive_hit_test(&primitive, point, threshold))
}

fn path_is_closed_with_tolerance(points: &[Point<f64>], tolerance: f64) -> bool {
    points.len() > 3
        && points
            .first()
            .zip(points.last())
            .is_some_and(|(first, last)| {
                let dx = first.x - last.x;
                let dy = first.y - last.y;
                dx * dx + dy * dy <= tolerance * tolerance
            })
}

pub fn arrow_curve_ops(arrow: &ArrowData) -> Vec<CurvePathOp> {
    let global_points = arrow_global_points(arrow);
    if arrow.is_curve() {
        return curve_curve_ops(
            &global_points,
            arrow.curve_tension(),
            false,
            arrow.fixed_segments.as_deref(),
        );
    }
    linear_curve_ops(&global_points)
}

pub fn arrowhead_render_primitives(
    arrow: &ArrowData,
    position: ArrowEndpointPosition,
) -> Vec<ArrowheadRenderPrimitive> {
    let arrowhead = match position {
        ArrowEndpointPosition::Start => arrow.start_arrowhead,
        ArrowEndpointPosition::End => arrow.end_arrowhead,
    };
    let Some(arrowhead) = arrowhead else {
        return Vec::new();
    };

    rendering::get_arrowhead_render_primitives(&rendering::ArrowheadRenderPrimitivesInput {
        arrow_points: arrow
            .global_points()
            .iter()
            .map(|point| [point.x, point.y])
            .collect(),
        stroke_width: arrow.stroke_width,
        curve_ops: arrow_curve_ops(arrow),
        position,
        arrowhead,
        stroke_style: arrow.stroke_style,
    })
}

fn cubic_line_controls(start: Point<f64>, end: Point<f64>) -> [Point<f64>; 2] {
    [
        Point::new(
            start.x + (end.x - start.x) / 3.0,
            start.y + (end.y - start.y) / 3.0,
        ),
        Point::new(
            start.x + (end.x - start.x) * (2.0 / 3.0),
            start.y + (end.y - start.y) * (2.0 / 3.0),
        ),
    ]
}

fn linear_curve_ops(points: &[Point<f64>]) -> Vec<CurvePathOp> {
    let Some(first) = points.first().copied() else {
        return Vec::new();
    };

    let mut ops = vec![CurvePathOp {
        op: "move".to_owned(),
        data: vec![first.x, first.y],
    }];
    for segment in points.windows(2) {
        let [control_1, control_2] = cubic_line_controls(segment[0], segment[1]);
        ops.push(CurvePathOp {
            op: "bcurveTo".to_owned(),
            data: vec![
                control_1.x,
                control_1.y,
                control_2.x,
                control_2.y,
                segment[1].x,
                segment[1].y,
            ],
        });
    }
    ops
}

fn curve_curve_ops(
    points: &[Point<f64>],
    tension: f64,
    use_open_endpoint_phantom_points: bool,
    fixed_segments: Option<&[FixedSegment]>,
) -> Vec<CurvePathOp> {
    let Some(first) = points.first().copied() else {
        return Vec::new();
    };

    let mut ops = vec![CurvePathOp {
        op: "move".to_owned(),
        data: vec![first.x, first.y],
    }];
    for [start, control_1, control_2, end] in curve_bezier_segments(
        points,
        tension,
        use_open_endpoint_phantom_points,
        fixed_segments,
    ) {
        ops.push(CurvePathOp {
            op: "bcurveTo".to_owned(),
            data: vec![
                control_1.x,
                control_1.y,
                control_2.x,
                control_2.y,
                end.x,
                end.y,
            ],
        });
        let _ = start;
    }
    ops
}

fn curve_bezier_segments(
    points: &[Point<f64>],
    tension: f64,
    use_open_endpoint_phantom_points: bool,
    fixed_segments: Option<&[FixedSegment]>,
) -> Vec<[Point<f64>; 4]> {
    if points.len() < 2 {
        return Vec::new();
    }
    let mut fixed_segment_mask = vec![false; points.len()];
    for segment in fixed_segments.into_iter().flatten() {
        if let Some(fixed) = fixed_segment_mask.get_mut(segment.index) {
            *fixed = true;
        }
    }

    let closed = points.len() >= 4 && points.first() == points.last();
    if closed {
        let unique = &points[..points.len() - 1];
        let count = unique.len();
        let mut segments = Vec::with_capacity(count);
        for index in 0..count {
            let previous = unique[(index + count - 1) % count];
            let start = unique[index];
            let end = unique[(index + 1) % count];
            let next = unique[(index + 2) % count];
            let [control_1, control_2] = curve_segment_controls(
                previous,
                start,
                end,
                next,
                CurveSegmentContext {
                    tension,
                    fixed_segment_mask: &fixed_segment_mask,
                    segment_index: index + 1,
                    previous_segment_index: if index == 0 { count } else { index },
                    next_segment_index: if index + 2 > count { 1 } else { index + 2 },
                },
            );
            segments.push([start, control_1, control_2, end]);
        }
        return segments;
    }

    let mut segments = Vec::with_capacity(points.len().saturating_sub(1));
    let phantom_first = Point::new(
        points[0].x + (points[0].x - points[1].x),
        points[0].y + (points[0].y - points[1].y),
    );
    let last_index = points.len() - 1;
    let phantom_last = Point::new(
        points[last_index].x + (points[last_index].x - points[last_index - 1].x),
        points[last_index].y + (points[last_index].y - points[last_index - 1].y),
    );
    for index in 0..points.len() - 1 {
        let previous = if index == 0 && use_open_endpoint_phantom_points {
            phantom_first
        } else {
            points[index.saturating_sub(1)]
        };
        let start = points[index];
        let end = points[index + 1];
        let next = if index + 2 < points.len() {
            points[index + 2]
        } else if use_open_endpoint_phantom_points {
            phantom_last
        } else {
            end
        };
        let [control_1, control_2] = curve_segment_controls(
            previous,
            start,
            end,
            next,
            CurveSegmentContext {
                tension,
                fixed_segment_mask: &fixed_segment_mask,
                segment_index: index + 1,
                previous_segment_index: index,
                next_segment_index: index + 2,
            },
        );
        segments.push([start, control_1, control_2, end]);
    }
    segments
}

struct CurveSegmentContext<'a> {
    tension: f64,
    fixed_segment_mask: &'a [bool],
    segment_index: usize,
    previous_segment_index: usize,
    next_segment_index: usize,
}

fn curve_segment_controls(
    mut previous: Point<f64>,
    start: Point<f64>,
    end: Point<f64>,
    mut next: Point<f64>,
    context: CurveSegmentContext<'_>,
) -> [Point<f64>; 2] {
    let is_fixed = |index: usize| {
        context
            .fixed_segment_mask
            .get(index)
            .copied()
            .unwrap_or(false)
    };
    if is_fixed(context.segment_index) {
        return [
            Point::new(
                start.x + (end.x - start.x) / 3.0,
                start.y + (end.y - start.y) / 3.0,
            ),
            Point::new(
                start.x + (end.x - start.x) * 2.0 / 3.0,
                start.y + (end.y - start.y) * 2.0 / 3.0,
            ),
        ];
    }
    if is_fixed(context.previous_segment_index) {
        previous = start;
    }
    if is_fixed(context.next_segment_index) {
        next = end;
    }
    catmull_rom_cubic_controls(previous, start, end, next, context.tension)
}

fn catmull_rom_cubic_controls(
    previous: Point<f64>,
    start: Point<f64>,
    end: Point<f64>,
    next: Point<f64>,
    tension: f64,
) -> [Point<f64>; 2] {
    [
        Point::new(
            start.x + (end.x - previous.x) * (tension / 6.0),
            start.y + (end.y - previous.y) * (tension / 6.0),
        ),
        Point::new(
            end.x - (next.x - start.x) * (tension / 6.0),
            end.y - (next.y - start.y) * (tension / 6.0),
        ),
    ]
}

fn point_at_bezier(
    start: Point<f64>,
    control_1: Point<f64>,
    control_2: Point<f64>,
    end: Point<f64>,
    t: f64,
) -> Point<f64> {
    let one_minus_t = 1.0 - t;
    let x = one_minus_t.powi(3) * start.x
        + 3.0 * one_minus_t.powi(2) * t * control_1.x
        + 3.0 * one_minus_t * t.powi(2) * control_2.x
        + t.powi(3) * end.x;
    let y = one_minus_t.powi(3) * start.y
        + 3.0 * one_minus_t.powi(2) * t * control_1.y
        + 3.0 * one_minus_t * t.powi(2) * control_2.y
        + t.powi(3) * end.y;
    Point::new(x, y)
}

fn point_to_path_point(point: Point<f64>) -> [f64; 2] {
    [point.x, point.y]
}

fn linear_path_commands(points: &[Point<f64>]) -> Vec<ArrowPathCommand> {
    let Some(first) = points.first().copied() else {
        return Vec::new();
    };
    let mut commands = vec![ArrowPathCommand::MoveTo {
        point: point_to_path_point(first),
    }];
    commands.extend(points.iter().skip(1).map(|point| ArrowPathCommand::LineTo {
        point: point_to_path_point(*point),
    }));
    commands
}

fn curve_path_commands(
    points: &[Point<f64>],
    tension: f64,
    use_open_endpoint_phantom_points: bool,
    fixed_segments: Option<&[FixedSegment]>,
) -> Vec<ArrowPathCommand> {
    let Some(first) = points.first().copied() else {
        return Vec::new();
    };
    let mut commands = vec![ArrowPathCommand::MoveTo {
        point: point_to_path_point(first),
    }];
    for [_start, control_1, control_2, end] in curve_bezier_segments(
        points,
        tension,
        use_open_endpoint_phantom_points,
        fixed_segments,
    ) {
        commands.push(ArrowPathCommand::CubicTo {
            control_1: point_to_path_point(control_1),
            control_2: point_to_path_point(control_2),
            end: point_to_path_point(end),
        });
    }
    commands
}

fn arrow_visual_points(arrow: &ArrowData) -> Vec<Point<f64>> {
    if !arrow.is_curve() {
        return arrow_global_points(arrow);
    }
    sampled_curve_points(
        &arrow_global_points(arrow),
        16,
        arrow.curve_tension(),
        false,
        arrow.fixed_segments.as_deref(),
    )
}

fn sampled_curve_points(
    points: &[Point<f64>],
    steps_per_segment: usize,
    tension: f64,
    use_open_endpoint_phantom_points: bool,
    fixed_segments: Option<&[FixedSegment]>,
) -> Vec<Point<f64>> {
    let Some(first) = points.first().copied() else {
        return Vec::new();
    };

    let segments = curve_bezier_segments(
        points,
        tension,
        use_open_endpoint_phantom_points,
        fixed_segments,
    );
    if segments.is_empty() {
        return vec![first];
    }

    let steps_per_segment = steps_per_segment.max(1);
    let mut sampled = vec![first];
    for [start, control_1, control_2, end] in segments {
        let _ = start;
        for step in 1..=steps_per_segment {
            let t = step as f64 / steps_per_segment as f64;
            sampled.push(point_at_bezier(start, control_1, control_2, end, t));
        }
    }
    sampled
}

fn point_arrays_distance(left: [f64; 2], right: [f64; 2]) -> f64 {
    (left[0] - right[0]).hypot(left[1] - right[1])
}

fn point_distance(left: Point<f64>, right: Point<f64>) -> f64 {
    (left.x - right.x).hypot(left.y - right.y)
}

fn union_point_bounds(bounds: Option<DrawRect>, point: Point<f64>, radius: f64) -> DrawRect {
    let point_bounds = DrawRect::new(
        point.x - radius,
        point.y - radius,
        point.x + radius,
        point.y + radius,
    );
    bounds.map_or(point_bounds, |bounds| union_draw_rect(bounds, point_bounds))
}

fn union_draw_rect(left: DrawRect, right: DrawRect) -> DrawRect {
    DrawRect::new(
        left.min_x.min(right.min_x),
        left.min_y.min(right.min_y),
        left.max_x.max(right.max_x),
        left.max_y.max(right.max_y),
    )
}

fn distance_point_to_segment(point: Point<f64>, start: Point<f64>, end: Point<f64>) -> f64 {
    let dx = end.x - start.x;
    let dy = end.y - start.y;
    let length_sq = dx * dx + dy * dy;
    if length_sq <= 1e-12 {
        return ((point.x - start.x).powi(2) + (point.y - start.y).powi(2)).sqrt();
    }

    let t = (((point.x - start.x) * dx + (point.y - start.y) * dy) / length_sq).clamp(0.0, 1.0);
    let projection = Point::new(start.x + dx * t, start.y + dy * t);
    ((point.x - projection.x).powi(2) + (point.y - projection.y).powi(2)).sqrt()
}

fn arrowhead_primitive_bounds(
    primitive: &ArrowheadRenderPrimitive,
    stroke_padding: f64,
) -> DrawRect {
    match primitive {
        ArrowheadRenderPrimitive::Line(line) => DrawRect::new(
            line.from[0].min(line.to[0]) - stroke_padding,
            line.from[1].min(line.to[1]) - stroke_padding,
            line.from[0].max(line.to[0]) + stroke_padding,
            line.from[1].max(line.to[1]) + stroke_padding,
        ),
        ArrowheadRenderPrimitive::Polygon(polygon) => {
            let mut min_x = f64::INFINITY;
            let mut min_y = f64::INFINITY;
            let mut max_x = f64::NEG_INFINITY;
            let mut max_y = f64::NEG_INFINITY;
            for point in &polygon.points {
                min_x = min_x.min(point[0]);
                min_y = min_y.min(point[1]);
                max_x = max_x.max(point[0]);
                max_y = max_y.max(point[1]);
            }
            DrawRect::new(
                min_x - stroke_padding,
                min_y - stroke_padding,
                max_x + stroke_padding,
                max_y + stroke_padding,
            )
        }
        ArrowheadRenderPrimitive::Circle(circle) => {
            let radius = circle.diameter / 2.0 + stroke_padding;
            DrawRect::new(
                circle.center[0] - radius,
                circle.center[1] - radius,
                circle.center[0] + radius,
                circle.center[1] + radius,
            )
        }
    }
}

fn arrowhead_primitive_hit_test(
    primitive: &ArrowheadRenderPrimitive,
    point: Point<f64>,
    threshold: f64,
) -> bool {
    match primitive {
        ArrowheadRenderPrimitive::Line(line) => {
            distance_point_to_segment(
                point,
                Point::new(line.from[0], line.from[1]),
                Point::new(line.to[0], line.to[1]),
            ) <= threshold
        }
        ArrowheadRenderPrimitive::Polygon(polygon) => {
            let points = polygon
                .points
                .iter()
                .map(|point| Point::new(point[0], point[1]))
                .collect::<Vec<_>>();
            polygon_edges_hit(&points, point, threshold)
                || matches!(
                    polygon.fill_mode,
                    ArrowheadFillMode::Stroke | ArrowheadFillMode::Background
                ) && point_in_polygon(&points, point)
        }
        ArrowheadRenderPrimitive::Circle(circle) => {
            let center = Point::new(circle.center[0], circle.center[1]);
            let distance = ((point.x - center.x).powi(2) + (point.y - center.y).powi(2)).sqrt();
            distance <= circle.diameter / 2.0 + threshold
        }
    }
}

fn polygon_edges_hit(points: &[Point<f64>], point: Point<f64>, threshold: f64) -> bool {
    points
        .windows(2)
        .any(|segment| distance_point_to_segment(point, segment[0], segment[1]) <= threshold)
}

fn point_in_polygon(points: &[Point<f64>], point: Point<f64>) -> bool {
    if points.len() < 3 {
        return false;
    }

    let mut inside = false;
    let mut previous = points[points.len() - 1];
    for current in points {
        let intersects = ((current.y > point.y) != (previous.y > point.y))
            && (point.x
                < (previous.x - current.x) * (point.y - current.y)
                    / (previous.y - current.y + 1e-12)
                    + current.x);
        if intersects {
            inside = !inside;
        }
        previous = *current;
    }
    inside
}

#[cfg(test)]
mod line_tests {
    use std::collections::HashSet;

    use super::*;
    use crate::LINE_SCHEMA;

    fn line(points: &[Point<f64>], fill: ColorRgba8) -> ArrowData {
        ArrowData::from_global_points(
            points,
            ColorRgba8 {
                r: 30,
                g: 30,
                b: 30,
                a: 255,
            },
            2.0,
            ArrowStrokeStyle::Dashed,
            ArrowType::Curve,
            None,
            None,
        )
        .unwrap()
        .into_line(fill, FillStyle::CrossLine)
    }

    #[test]
    fn arrow_patch_resets_point_metadata_when_the_route_gains_a_point() {
        let arrow = ArrowData::from_global_points(
            &[
                Point::new(0.0, 100.0),
                Point::new(100.0, 100.0),
                Point::new(100.0, 0.0),
                Point::new(200.0, 0.0),
            ],
            ColorRgba8::default(),
            2.0,
            ArrowStrokeStyle::Solid,
            ArrowType::Elbow,
            None,
            None,
        )
        .unwrap();
        let patched = apply_arrow_patch(
            &arrow,
            &ArrowPatch {
                points: Some(vec![
                    [0.0, 0.0],
                    [100.0, 0.0],
                    [100.0, -70.0],
                    [200.0, -70.0],
                    [200.0, -100.0],
                ]),
                ..ArrowPatch::default()
            },
        );

        assert_eq!(patched.points.len(), 5);
        assert!(validate_arrow(&patched).is_ok());
    }

    #[test]
    fn line_json_round_trip_normalizes_points_and_preserves_bindings() {
        let mut original = line(
            &[
                Point::new(30.0, 40.0),
                Point::new(10.0, 80.0),
                Point::new(30.0, 60.0),
            ],
            ColorRgba8 {
                r: 1,
                g: 2,
                b: 3,
                a: 128,
            },
        );
        original.start_binding = Some(ArrowEndpointBinding {
            element_id: ElementId {
                index: 7,
                generation: 3,
            },
            fixed_point: [0.25, 0.75],
            mode: BindMode::Orbit,
        });
        original.start_is_special = Some(true);
        original.end_is_special = Some(false);
        let payload = original.to_line_json_payload().unwrap();
        assert_eq!(
            payload.keys().map(String::as_str).collect::<HashSet<_>>(),
            LINE_SCHEMA.fields.iter().copied().collect::<HashSet<_>>()
        );
        assert_eq!(payload["typeId"], "line");
        assert_eq!(payload["arrowType"], "curved");
        assert_eq!(payload["startArrowhead"], "none");
        assert_eq!(payload["endArrowhead"], "none");
        let normalized = payload["points"].as_array().unwrap();
        assert_eq!(normalized[0]["x"], 1.0);
        assert_eq!(normalized[1]["x"], 0.0);

        let decoded =
            ArrowData::from_line_json_payload(&payload, DrawRect::new(10.0, 40.0, 30.0, 80.0))
                .unwrap();
        assert_eq!(decoded.global_points(), original.global_points());
        assert_eq!(decoded.linear_kind, LinearElementKind::Line);
        assert_eq!(decoded.fill, original.fill);
        assert_eq!(decoded.fill_style, original.fill_style);
        assert_eq!(decoded.start_binding, original.start_binding);
        assert_eq!(decoded.start_is_special, original.start_is_special);
        assert_eq!(decoded.end_is_special, original.end_is_special);
        assert_eq!(decoded.start_arrowhead, None);
        assert_eq!(decoded.end_arrowhead, None);
        assert_eq!(decoded.arrow_type, ArrowType::Curve);
    }

    #[test]
    fn horizontal_line_json_round_trip_avoids_division_by_zero() {
        let original = line(
            &[Point::new(50.0, 10.0), Point::new(10.0, 10.0)],
            ColorRgba8::default(),
        );
        let payload = original.to_line_json_payload().unwrap();
        let points = payload["points"].as_array().unwrap();
        assert_eq!(points[0]["y"], 0.0);
        assert_eq!(points[1]["y"], 0.0);
        let decoded =
            ArrowData::from_line_json_payload(&payload, DrawRect::new(10.0, 10.0, 50.0, 10.0))
                .unwrap();
        assert_eq!(decoded.global_points(), original.global_points());
    }

    #[test]
    fn curved_line_segment_handles_follow_the_rendered_curve() {
        let line = line(
            &[
                Point::new(0.0, 0.0),
                Point::new(100.0, 100.0),
                Point::new(200.0, 0.0),
            ],
            ColorRgba8::default(),
        );

        let midpoints = arrow_segment_midpoints(&line);

        assert_eq!(midpoints.len(), 2);
        assert_ne!(midpoints[0].1, Point::new(50.0, 50.0));
        let [start, control_1, control_2, end] =
            curve_bezier_segments(&line.global_points(), line.curve_tension(), false, None)[0];
        let expected = point_at_bezier(start, control_1, control_2, end, 0.5);
        assert_eq!(midpoints[0].1, expected);
    }

    #[test]
    fn line_json_rejects_noncanonical_or_malformed_fields() {
        let original = line(
            &[Point::new(0.0, 0.0), Point::new(10.0, 10.0)],
            ColorRgba8::default(),
        );
        let payload = original.to_line_json_payload().unwrap();
        for (field, invalid) in [
            ("typeId", "arrow"),
            ("arrowType", "straight"),
            ("startArrowhead", "arrow"),
            ("endArrowhead", "dot"),
            ("strokeStyle", "unknown"),
            ("fillStyle", "unknown"),
        ] {
            let mut malformed = payload.clone();
            malformed.insert(field.to_owned(), Value::String(invalid.to_owned()));
            assert_eq!(
                ArrowData::from_line_json_payload(&malformed, DrawRect::new(0.0, 0.0, 10.0, 10.0),),
                Err(ErrorCode::InvalidArgument)
            );
        }
        let mut too_few_points = payload;
        too_few_points.insert("points".to_owned(), Value::Array(vec![]));
        assert!(
            ArrowData::from_line_json_payload(
                &too_few_points,
                DrawRect::new(0.0, 0.0, 10.0, 10.0),
            )
            .is_err()
        );
    }

    #[test]
    fn only_closed_visible_line_fill_hits_the_interior() {
        let fill = ColorRgba8 {
            r: 255,
            g: 0,
            b: 0,
            a: 255,
        };
        let open = line(
            &[
                Point::new(0.0, 0.0),
                Point::new(100.0, 0.0),
                Point::new(50.0, 100.0),
            ],
            fill,
        );
        assert!(!arrow_hit_test(&open, Point::new(50.0, 40.0), 0.0));

        let closed = line(
            &[
                Point::new(0.0, 0.0),
                Point::new(100.0, 0.0),
                Point::new(50.0, 100.0),
                Point::new(0.0, 0.0),
            ],
            fill,
        );
        assert!(arrow_hit_test(&closed, Point::new(50.0, 35.0), 0.0));
        let transparent = ArrowData {
            fill: ColorRgba8::default(),
            ..closed
        };
        assert!(!arrow_hit_test(&transparent, Point::new(50.0, 35.0), 0.0));
        let invisible = ArrowData {
            fill,
            opacity: 0.0,
            ..transparent
        };
        assert!(!arrow_hit_test(&invisible, Point::new(50.0, 35.0), 0.0));
    }
}

#[cfg(test)]
mod arrow_json_tests {
    use std::collections::HashSet;

    use super::*;
    use crate::ARROW_SCHEMA;

    #[test]
    fn arrow_json_round_trip_uses_the_canonical_reference_schema() {
        let mut original = ArrowData::from_global_points(
            &[
                Point::new(10.0, 20.0),
                Point::new(30.0, 60.0),
                Point::new(50.0, 20.0),
            ],
            ColorRgba8 {
                r: 1,
                g: 2,
                b: 3,
                a: 255,
            },
            3.0,
            ArrowStrokeStyle::Dotted,
            ArrowType::Curve,
            Some(Arrowhead::Triangle),
            Some(Arrowhead::Bar),
        )
        .unwrap();
        original.end_binding = Some(ArrowEndpointBinding {
            element_id: ElementId {
                index: 4,
                generation: 2,
            },
            fixed_point: [0.25, 0.75],
            mode: BindMode::Inside,
        });
        original.start_is_special = Some(true);

        let payload = original.to_arrow_json_payload().unwrap();
        assert_eq!(
            payload.keys().map(String::as_str).collect::<HashSet<_>>(),
            ARROW_SCHEMA.fields.iter().copied().collect::<HashSet<_>>()
        );
        assert_eq!(payload[field::ARROW_TYPE], "curved");
        assert_eq!(payload[field::START_ARROWHEAD], "triangle");
        assert_eq!(payload[field::END_ARROWHEAD], "verticalLine");

        let decoded =
            ArrowData::from_arrow_json_payload(&payload, DrawRect::new(10.0, 20.0, 50.0, 60.0))
                .unwrap();
        assert_eq!(decoded.global_points(), original.global_points());
        assert_eq!(decoded.stroke, original.stroke);
        assert_eq!(decoded.stroke_width, original.stroke_width);
        assert_eq!(decoded.stroke_style, original.stroke_style);
        assert_eq!(decoded.arrow_type, original.arrow_type);
        assert_eq!(decoded.start_arrowhead, original.start_arrowhead);
        assert_eq!(decoded.end_arrowhead, original.end_arrowhead);
        assert_eq!(decoded.end_binding, original.end_binding);
        assert_eq!(decoded.start_is_special, original.start_is_special);
    }

    #[test]
    fn arrow_json_rejects_legacy_arrowheads_without_reference_equivalents() {
        let arrow = ArrowData::from_global_points(
            &[Point::new(0.0, 0.0), Point::new(10.0, 10.0)],
            ColorRgba8::default(),
            1.0,
            ArrowStrokeStyle::Solid,
            ArrowType::Straight,
            Some(Arrowhead::CrowfootMany),
            None,
        )
        .unwrap();

        assert_eq!(
            arrow.to_arrow_json_payload(),
            Err(ErrorCode::InvalidArgument)
        );
    }
}

#[cfg(test)]
mod pen_highlight_tests {
    use super::*;

    fn pen_highlight() -> ArrowData {
        ArrowData::from_global_points(
            &[Point::new(10.0, 20.0), Point::new(50.0, 60.0)],
            ColorRgba8 {
                r: 245,
                g: 34,
                b: 45,
                a: 255,
            },
            30.0,
            ArrowStrokeStyle::Solid,
            ArrowType::Straight,
            None,
            None,
        )
        .unwrap()
        .into_pen_highlight()
    }

    #[test]
    fn pen_highlight_payload_round_trips_two_points() {
        let payload = pen_highlight().to_pen_highlight_json_payload().unwrap();
        assert_eq!(payload["typeId"], PEN_HIGHLIGHT_TYPE_ID);
        assert_eq!(payload["strokeWidth"], 30.0);
        let decoded = ArrowData::from_pen_highlight_json_payload(
            &payload,
            DrawRect::new(10.0, 20.0, 50.0, 60.0),
        )
        .unwrap();
        assert!(decoded.is_pen_highlight());
        assert_eq!(decoded.global_points(), pen_highlight().global_points());
        assert_eq!(decoded.arrow_type, ArrowType::Straight);
        assert_eq!(decoded.start_arrowhead, None);
        assert_eq!(decoded.end_arrowhead, None);
    }

    #[test]
    fn pen_highlight_payload_rejects_non_two_point_paths() {
        let mut payload = pen_highlight().to_pen_highlight_json_payload().unwrap();
        payload["points"] = Value::Array(vec![payload["points"][0].clone()]);
        assert_eq!(
            ArrowData::from_pen_highlight_json_payload(
                &payload,
                DrawRect::new(10.0, 20.0, 50.0, 60.0),
            ),
            Err(ErrorCode::InvalidArgument)
        );
    }
}
