use serde::{Deserialize, Serialize};
use serde_json::{Map, Number, Value};
use snow_draw_engine_core::{
    ColorRgba8, DrawRect, ErrorCode, PathCommand, PathGeometry, PathSegmentMode, Point,
    catmull_rom_path_commands,
};

use crate::{
    ElementKind, FillStyle, StrokeStyle,
    element_schema::{FREE_DRAW_TYPE_ID, field},
};

pub const FREE_DRAW_GEOMETRY_VERSION: u64 = 1;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FreeDrawStyle {
    pub stroke: ColorRgba8,
    pub stroke_width: f64,
    pub stroke_style: StrokeStyle,
    pub fill: ColorRgba8,
    pub fill_style: FillStyle,
    pub opacity: f64,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct FreeDrawData {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
    pub rotation: f64,
    pub vertices: Vec<[f64; 2]>,
    pub segment_modes: Vec<PathSegmentMode>,
    pub closed: bool,
    pub stroke: ColorRgba8,
    pub stroke_width: f64,
    pub stroke_style: StrokeStyle,
    pub fill: ColorRgba8,
    pub fill_style: FillStyle,
    pub opacity: f64,
}

impl FreeDrawData {
    pub fn from_global_vertices(
        vertices: &[Point<f64>],
        segment_modes: Vec<PathSegmentMode>,
        closed: bool,
        style: FreeDrawStyle,
    ) -> Option<Self> {
        if vertices.len() < 2
            || vertices
                .iter()
                .any(|point| !point.x.is_finite() || !point.y.is_finite())
        {
            return None;
        }
        let min_x = vertices
            .iter()
            .map(|point| point.x)
            .fold(f64::INFINITY, f64::min);
        let min_y = vertices
            .iter()
            .map(|point| point.y)
            .fold(f64::INFINITY, f64::min);
        let max_x = vertices
            .iter()
            .map(|point| point.x)
            .fold(f64::NEG_INFINITY, f64::max);
        let max_y = vertices
            .iter()
            .map(|point| point.y)
            .fold(f64::NEG_INFINITY, f64::max);
        let normalize = |value: f64, min: f64, span: f64| {
            if span.abs() <= f64::EPSILON {
                0.0
            } else {
                (value - min) / span
            }
        };
        let mut data = Self {
            x: min_x,
            y: min_y,
            width: max_x - min_x,
            height: max_y - min_y,
            rotation: 0.0,
            vertices: vertices
                .iter()
                .map(|point| {
                    [
                        normalize(point.x, min_x, max_x - min_x),
                        normalize(point.y, min_y, max_y - min_y),
                    ]
                })
                .collect(),
            segment_modes,
            closed,
            stroke: style.stroke,
            stroke_width: style.stroke_width,
            stroke_style: style.stroke_style,
            fill: style.fill,
            fill_style: style.fill_style,
            opacity: style.opacity,
        };
        data.normalize_modes();
        validate_free_draw(&data).ok()?;
        Some(data)
    }

    pub fn element_kind(&self) -> ElementKind {
        ElementKind::FreeDraw
    }

    pub fn global_vertices(&self) -> Vec<Point<f64>> {
        let center = Point::new(self.x + self.width / 2.0, self.y + self.height / 2.0);
        let cosine = self.rotation.cos();
        let sine = self.rotation.sin();
        self.vertices
            .iter()
            .map(|point| {
                let x = self.x + point[0] * self.width;
                let y = self.y + point[1] * self.height;
                let dx = x - center.x;
                let dy = y - center.y;
                Point::new(
                    center.x + dx * cosine - dy * sine,
                    center.y + dx * sine + dy * cosine,
                )
            })
            .collect()
    }

    pub fn path_commands(&self) -> Vec<PathCommand> {
        catmull_rom_path_commands(&self.global_vertices(), &self.segment_modes, self.closed)
    }

    pub fn path_geometry(&self, revision: u64) -> PathGeometry {
        PathGeometry::from_commands(revision, self.path_commands(), self.closed)
    }

    pub fn to_json_payload(&self) -> Result<Map<String, Value>, ErrorCode> {
        validate_free_draw(self)?;
        let mut payload = Map::new();
        payload.insert(
            field::TYPE_ID.to_owned(),
            Value::String(FREE_DRAW_TYPE_ID.to_owned()),
        );
        payload.insert(
            field::GEOMETRY_VERSION.to_owned(),
            Value::Number(FREE_DRAW_GEOMETRY_VERSION.into()),
        );
        payload.insert(
            field::POINTS.to_owned(),
            Value::Array(
                self.vertices
                    .iter()
                    .map(|point| Ok(Value::Array(vec![number(point[0])?, number(point[1])?])))
                    .collect::<Result<Vec<_>, ErrorCode>>()?,
            ),
        );
        payload.insert(
            field::STRAIGHT_SEGMENTS.to_owned(),
            Value::Array(
                self.segment_modes
                    .iter()
                    .enumerate()
                    .filter_map(|(index, mode)| {
                        (*mode == PathSegmentMode::Straight)
                            .then_some(Value::Number((index as u64).into()))
                    })
                    .collect(),
            ),
        );
        payload.insert(field::CLOSED.to_owned(), Value::Bool(self.closed));
        payload.insert(field::COLOR.to_owned(), argb(self.stroke));
        payload.insert(field::FILL_COLOR.to_owned(), argb(self.fill));
        payload.insert(field::STROKE_WIDTH.to_owned(), number(self.stroke_width)?);
        payload.insert(
            field::STROKE_STYLE.to_owned(),
            Value::String(stroke_style_name(self.stroke_style).to_owned()),
        );
        payload.insert(
            field::FILL_STYLE.to_owned(),
            Value::String(fill_style_name(self.fill_style).to_owned()),
        );
        Ok(payload)
    }

    pub fn from_json_payload(
        payload: &Map<String, Value>,
        rect: DrawRect,
    ) -> Result<Self, ErrorCode> {
        const FIELD_COUNT: usize = 10;
        if payload.len() != FIELD_COUNT
            || payload.get(field::TYPE_ID).and_then(Value::as_str) != Some(FREE_DRAW_TYPE_ID)
            || payload.get(field::GEOMETRY_VERSION).and_then(Value::as_u64)
                != Some(FREE_DRAW_GEOMETRY_VERSION)
        {
            return Err(ErrorCode::InvalidArgument);
        }
        let points = payload
            .get(field::POINTS)
            .and_then(Value::as_array)
            .ok_or(ErrorCode::InvalidArgument)?;
        if points.len() < 2 {
            return Err(ErrorCode::InvalidArgument);
        }
        let width = rect.max_x - rect.min_x;
        let height = rect.max_y - rect.min_y;
        let mut vertices = Vec::with_capacity(points.len());
        for value in points {
            let pair = value
                .as_array()
                .filter(|pair| pair.len() == 2)
                .ok_or(ErrorCode::InvalidArgument)?;
            let x = finite_number(&pair[0])?;
            let y = finite_number(&pair[1])?;
            if !(0.0..=1.0).contains(&x) || !(0.0..=1.0).contains(&y) {
                return Err(ErrorCode::InvalidArgument);
            }
            vertices.push([x, y]);
        }
        if vertices.first() == vertices.last() {
            return Err(ErrorCode::InvalidArgument);
        }
        let closed = payload
            .get(field::CLOSED)
            .and_then(Value::as_bool)
            .ok_or(ErrorCode::InvalidArgument)?;
        let edge_count = if closed {
            vertices.len()
        } else {
            vertices.len() - 1
        };
        let mut segment_modes = vec![PathSegmentMode::Curve; edge_count];
        let straight = payload
            .get(field::STRAIGHT_SEGMENTS)
            .and_then(Value::as_array)
            .ok_or(ErrorCode::InvalidArgument)?;
        let mut previous = None;
        for value in straight {
            let index = value
                .as_u64()
                .and_then(|value| usize::try_from(value).ok())
                .filter(|index| *index < edge_count)
                .ok_or(ErrorCode::InvalidArgument)?;
            if previous.is_some_and(|previous| index <= previous) {
                return Err(ErrorCode::InvalidArgument);
            }
            segment_modes[index] = PathSegmentMode::Straight;
            previous = Some(index);
        }
        let data = Self {
            x: rect.min_x,
            y: rect.min_y,
            width,
            height,
            rotation: 0.0,
            vertices,
            segment_modes,
            closed,
            stroke: parse_argb(
                payload
                    .get(field::COLOR)
                    .ok_or(ErrorCode::InvalidArgument)?,
            )?,
            stroke_width: finite_number(
                payload
                    .get(field::STROKE_WIDTH)
                    .ok_or(ErrorCode::InvalidArgument)?,
            )?,
            stroke_style: parse_stroke_style(
                payload
                    .get(field::STROKE_STYLE)
                    .and_then(Value::as_str)
                    .ok_or(ErrorCode::InvalidArgument)?,
            )?,
            fill: parse_argb(
                payload
                    .get(field::FILL_COLOR)
                    .ok_or(ErrorCode::InvalidArgument)?,
            )?,
            fill_style: parse_fill_style(
                payload
                    .get(field::FILL_STYLE)
                    .and_then(Value::as_str)
                    .ok_or(ErrorCode::InvalidArgument)?,
            )?,
            opacity: 1.0,
        };
        validate_free_draw(&data)?;
        Ok(data)
    }

    fn normalize_modes(&mut self) {
        let edge_count = if self.closed {
            self.vertices.len()
        } else {
            self.vertices.len().saturating_sub(1)
        };
        self.segment_modes
            .resize(edge_count, PathSegmentMode::Curve);
        self.segment_modes.truncate(edge_count);
    }
}

pub fn validate_free_draw(data: &FreeDrawData) -> Result<(), ErrorCode> {
    let scalars = [
        data.x,
        data.y,
        data.width,
        data.height,
        data.rotation,
        data.stroke_width,
        data.opacity,
    ];
    let edge_count = if data.closed {
        data.vertices.len()
    } else {
        data.vertices.len().saturating_sub(1)
    };
    if scalars.iter().any(|value| !value.is_finite())
        || data.width < 0.0
        || data.height < 0.0
        || data.stroke_width < 0.0
        || !(0.0..=1.0).contains(&data.opacity)
        || data.vertices.len() < 2
        || data.segment_modes.len() != edge_count
        || data.vertices.first() == data.vertices.last()
        || data
            .vertices
            .iter()
            .flatten()
            .any(|value| !value.is_finite() || !(0.0..=1.0).contains(value))
    {
        return Err(ErrorCode::InvalidArgument);
    }
    Ok(())
}

pub fn free_draw_bounds(data: &FreeDrawData) -> DrawRect {
    let geometry = data.path_geometry(0);
    let outset = data.stroke_width * 0.5;
    DrawRect::new(
        geometry.canvas_bounds[0] - outset,
        geometry.canvas_bounds[1] - outset,
        geometry.canvas_bounds[2] + outset,
        geometry.canvas_bounds[3] + outset,
    )
}

pub fn free_draw_hit_test(data: &FreeDrawData, point: Point<f64>, tolerance: f64) -> bool {
    let commands = data.path_commands();
    let mut current = None;
    for command in commands {
        match command {
            PathCommand::MoveTo { point } => current = Some(Point::new(point[0], point[1])),
            PathCommand::LineTo { point: end } => {
                let end = Point::new(end[0], end[1]);
                if current.is_some_and(|start| {
                    distance_to_segment(point, start, end) <= tolerance + data.stroke_width * 0.5
                }) {
                    return true;
                }
                current = Some(end);
            }
            PathCommand::QuadTo { control, end } => {
                let Some(start) = current else {
                    continue;
                };
                let control = Point::new(control[0], control[1]);
                let end = Point::new(end[0], end[1]);
                let mut previous = start;
                for step in 1..=12 {
                    let t = step as f64 / 12.0;
                    let mt = 1.0 - t;
                    let sample = Point::new(
                        mt * mt * start.x + 2.0 * mt * t * control.x + t * t * end.x,
                        mt * mt * start.y + 2.0 * mt * t * control.y + t * t * end.y,
                    );
                    if distance_to_segment(point, previous, sample)
                        <= tolerance + data.stroke_width * 0.5
                    {
                        return true;
                    }
                    previous = sample;
                }
                current = Some(end);
            }
            PathCommand::CubicTo {
                control_1,
                control_2,
                end,
            } => {
                let Some(start) = current else {
                    continue;
                };
                let c1 = Point::new(control_1[0], control_1[1]);
                let c2 = Point::new(control_2[0], control_2[1]);
                let end = Point::new(end[0], end[1]);
                let mut previous = start;
                for step in 1..=16 {
                    let t = step as f64 / 16.0;
                    let mt = 1.0 - t;
                    let sample = Point::new(
                        mt.powi(3) * start.x
                            + 3.0 * mt * mt * t * c1.x
                            + 3.0 * mt * t * t * c2.x
                            + t.powi(3) * end.x,
                        mt.powi(3) * start.y
                            + 3.0 * mt * mt * t * c1.y
                            + 3.0 * mt * t * t * c2.y
                            + t.powi(3) * end.y,
                    );
                    if distance_to_segment(point, previous, sample)
                        <= tolerance + data.stroke_width * 0.5
                    {
                        return true;
                    }
                    previous = sample;
                }
                current = Some(end);
            }
        }
    }
    false
}

fn distance_to_segment(point: Point<f64>, start: Point<f64>, end: Point<f64>) -> f64 {
    let delta = Point::new(end.x - start.x, end.y - start.y);
    let length_sq = delta.x * delta.x + delta.y * delta.y;
    if length_sq <= 1e-12 {
        return (point.x - start.x).hypot(point.y - start.y);
    }
    let t = (((point.x - start.x) * delta.x + (point.y - start.y) * delta.y) / length_sq)
        .clamp(0.0, 1.0);
    (point.x - (start.x + delta.x * t)).hypot(point.y - (start.y + delta.y * t))
}

fn number(value: f64) -> Result<Value, ErrorCode> {
    Number::from_f64(value)
        .map(Value::Number)
        .ok_or(ErrorCode::InvalidArgument)
}
fn finite_number(value: &Value) -> Result<f64, ErrorCode> {
    value
        .as_f64()
        .filter(|value| value.is_finite())
        .ok_or(ErrorCode::InvalidArgument)
}
fn argb(color: ColorRgba8) -> Value {
    Value::String(format!(
        "#{:02x}{:02x}{:02x}{:02x}",
        color.a, color.r, color.g, color.b
    ))
}
fn parse_argb(value: &Value) -> Result<ColorRgba8, ErrorCode> {
    let value = value
        .as_str()
        .filter(|value| value.len() == 9 && value.starts_with('#'))
        .ok_or(ErrorCode::InvalidArgument)?;
    let byte = |offset| {
        u8::from_str_radix(&value[offset..offset + 2], 16).map_err(|_| ErrorCode::InvalidArgument)
    };
    Ok(ColorRgba8 {
        a: byte(1)?,
        r: byte(3)?,
        g: byte(5)?,
        b: byte(7)?,
    })
}
fn stroke_style_name(style: StrokeStyle) -> &'static str {
    match style {
        StrokeStyle::Solid => "solid",
        StrokeStyle::Dashed => "dashed",
        StrokeStyle::Dotted => "dotted",
    }
}
fn parse_stroke_style(value: &str) -> Result<StrokeStyle, ErrorCode> {
    match value {
        "solid" => Ok(StrokeStyle::Solid),
        "dashed" => Ok(StrokeStyle::Dashed),
        "dotted" => Ok(StrokeStyle::Dotted),
        _ => Err(ErrorCode::InvalidArgument),
    }
}
fn fill_style_name(style: FillStyle) -> &'static str {
    match style {
        FillStyle::Line => "line",
        FillStyle::CrossLine => "crossLine",
        FillStyle::Solid => "solid",
    }
}
fn parse_fill_style(value: &str) -> Result<FillStyle, ErrorCode> {
    match value {
        "line" => Ok(FillStyle::Line),
        "crossLine" => Ok(FillStyle::CrossLine),
        "solid" => Ok(FillStyle::Solid),
        _ => Err(ErrorCode::InvalidArgument),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> FreeDrawData {
        FreeDrawData::from_global_vertices(
            &[
                Point::new(10.0, 20.0),
                Point::new(30.0, 50.0),
                Point::new(50.0, 20.0),
            ],
            vec![PathSegmentMode::Curve, PathSegmentMode::Straight],
            false,
            FreeDrawStyle {
                stroke: ColorRgba8::default(),
                stroke_width: 2.0,
                stroke_style: StrokeStyle::Dashed,
                fill: ColorRgba8::default(),
                fill_style: FillStyle::Solid,
                opacity: 1.0,
            },
        )
        .unwrap()
    }

    #[test]
    fn json_is_versioned_normalized_and_strict() {
        let payload = sample().to_json_payload().unwrap();
        assert_eq!(payload[field::GEOMETRY_VERSION], 1);
        assert_eq!(payload[field::STRAIGHT_SEGMENTS], serde_json::json!([1]));
        assert_eq!(
            FreeDrawData::from_json_payload(&payload, DrawRect::new(10.0, 20.0, 50.0, 50.0))
                .unwrap(),
            sample()
        );
        let mut legacy = payload;
        legacy[field::POINTS] =
            serde_json::json!([{"x": 0.0, "y": 0.0, "p": 0.5}, {"x": 1.0, "y": 1.0}]);
        assert!(
            FreeDrawData::from_json_payload(&legacy, DrawRect::new(0.0, 0.0, 10.0, 10.0)).is_err()
        );
    }

    #[test]
    fn bounds_and_rotation_transform_normalized_vertices() {
        let mut data = sample();
        data.x = 100.0;
        data.y = 200.0;
        data.width = 80.0;
        data.height = 40.0;
        data.rotation = std::f64::consts::FRAC_PI_2;

        let vertices = data.global_vertices();
        assert!((vertices[0].x - 160.0).abs() < 1e-9);
        assert!((vertices[0].y - 180.0).abs() < 1e-9);
        assert!((vertices[1].x - 120.0).abs() < 1e-9);
        assert!((vertices[1].y - 220.0).abs() < 1e-9);
        assert!((vertices[2].x - 160.0).abs() < 1e-9);
        assert!((vertices[2].y - 260.0).abs() < 1e-9);
    }
}
