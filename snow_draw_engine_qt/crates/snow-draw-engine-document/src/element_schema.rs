//! Canonical canvas-element payload schemas.
//!
//! Geometry, rotation, opacity, and paint order belong to the element state,
//! not these type-specific payloads. The field sets mirror the definitions in
//! `snow_draw_core/lib/draw/elements/types` from the reference implementation.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ElementPayloadSchema {
    pub type_id: &'static str,
    pub fields: &'static [&'static str],
}

pub mod field {
    pub const ID: &str = "id";
    pub const RECT: &str = "rect";
    pub const ROTATION: &str = "rotation";
    pub const OPACITY: &str = "opacity";
    pub const Z_INDEX: &str = "zIndex";
    pub const DATA: &str = "data";
    pub const TYPE_ID: &str = "typeId";
    pub const POINTS: &str = "points";
    pub const COLOR: &str = "color";
    pub const FILL_COLOR: &str = "fillColor";
    pub const FILL_STYLE: &str = "fillStyle";
    pub const STROKE_COLOR: &str = "strokeColor";
    pub const STROKE_WIDTH: &str = "strokeWidth";
    pub const STROKE_STYLE: &str = "strokeStyle";
    pub const CORNER_RADIUS: &str = "cornerRadius";
    pub const SHAPE: &str = "shape";
    pub const ARROW_TYPE: &str = "arrowType";
    pub const START_ARROWHEAD: &str = "startArrowhead";
    pub const END_ARROWHEAD: &str = "endArrowhead";
    pub const START_BINDING: &str = "startBinding";
    pub const END_BINDING: &str = "endBinding";
    pub const FIXED_SEGMENTS: &str = "fixedSegments";
    pub const GEOMETRY_VERSION: &str = "geometryVersion";
    pub const STRAIGHT_SEGMENTS: &str = "straightSegments";
    pub const CLOSED: &str = "closed";
    pub const START_IS_SPECIAL: &str = "startIsSpecial";
    pub const END_IS_SPECIAL: &str = "endIsSpecial";
    pub const TEXT: &str = "text";
    pub const FONT_SIZE: &str = "fontSize";
    pub const FONT_FAMILY: &str = "fontFamily";
    pub const HORIZONTAL_ALIGN: &str = "horizontalAlign";
    pub const VERTICAL_ALIGN: &str = "verticalAlign";
    pub const AUTO_RESIZE: &str = "autoResize";
    pub const NUMBER: &str = "number";
    pub const TEXT_ELEMENT_ID: &str = "textElementId";
    pub const FILTER_TYPE: &str = "type";
    pub const STRENGTH: &str = "strength";
}

/// Common fields owned by reference `ElementState`, separate from the
/// type-specific payload schemas below.
pub const ELEMENT_STATE_FIELDS: &[&str] = &[
    field::ID,
    field::RECT,
    field::ROTATION,
    field::OPACITY,
    field::Z_INDEX,
    field::DATA,
];

pub const RECTANGLE_TYPE_ID: &str = "rectangle";
pub const ARROW_TYPE_ID: &str = "arrow";
pub const LINE_TYPE_ID: &str = "line";
pub const FREE_DRAW_TYPE_ID: &str = "free_draw";
pub const TEXT_TYPE_ID: &str = "text";
pub const SERIAL_NUMBER_TYPE_ID: &str = "serial_number";
pub const RECTANGLE_HIGHLIGHT_TYPE_ID: &str = "rectangle_highlight";
pub const PEN_HIGHLIGHT_TYPE_ID: &str = "pen_highlight";
pub const FILTER_TYPE_ID: &str = "filter";
pub const PEN_FILTER_TYPE_ID: &str = "pen_filter";
pub const SPOTLIGHT_TYPE_ID: &str = "spotlight";

pub const RECTANGLE_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: RECTANGLE_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::CORNER_RADIUS,
        field::SHAPE,
        field::FILL_COLOR,
        field::COLOR,
        field::STROKE_WIDTH,
        field::STROKE_STYLE,
        field::FILL_STYLE,
    ],
};

pub const ARROW_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: ARROW_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::POINTS,
        field::COLOR,
        field::STROKE_WIDTH,
        field::STROKE_STYLE,
        field::ARROW_TYPE,
        field::START_ARROWHEAD,
        field::END_ARROWHEAD,
        field::START_BINDING,
        field::END_BINDING,
        field::FIXED_SEGMENTS,
        field::START_IS_SPECIAL,
        field::END_IS_SPECIAL,
    ],
};

pub const LINE_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: LINE_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::POINTS,
        field::COLOR,
        field::FILL_COLOR,
        field::STROKE_WIDTH,
        field::STROKE_STYLE,
        field::FILL_STYLE,
        field::ARROW_TYPE,
        field::START_ARROWHEAD,
        field::END_ARROWHEAD,
        field::START_BINDING,
        field::END_BINDING,
        field::FIXED_SEGMENTS,
        field::START_IS_SPECIAL,
        field::END_IS_SPECIAL,
    ],
};

pub const FREE_DRAW_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: FREE_DRAW_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::GEOMETRY_VERSION,
        field::POINTS,
        field::STRAIGHT_SEGMENTS,
        field::CLOSED,
        field::COLOR,
        field::FILL_COLOR,
        field::STROKE_WIDTH,
        field::STROKE_STYLE,
        field::FILL_STYLE,
    ],
};

pub const TEXT_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: TEXT_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::TEXT,
        field::COLOR,
        field::FONT_SIZE,
        field::FONT_FAMILY,
        field::HORIZONTAL_ALIGN,
        field::VERTICAL_ALIGN,
        field::FILL_COLOR,
        field::FILL_STYLE,
        field::STROKE_COLOR,
        field::STROKE_WIDTH,
        field::CORNER_RADIUS,
        field::AUTO_RESIZE,
    ],
};

pub const SERIAL_NUMBER_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: SERIAL_NUMBER_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::NUMBER,
        field::COLOR,
        field::FILL_COLOR,
        field::FILL_STYLE,
        field::FONT_SIZE,
        field::FONT_FAMILY,
        field::STROKE_WIDTH,
        field::STROKE_STYLE,
        field::TEXT_ELEMENT_ID,
    ],
};

pub const RECTANGLE_HIGHLIGHT_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: RECTANGLE_HIGHLIGHT_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::COLOR,
        field::STROKE_COLOR,
        field::STROKE_WIDTH,
    ],
};

pub const PEN_HIGHLIGHT_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: PEN_HIGHLIGHT_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::POINTS,
        field::COLOR,
        field::STROKE_WIDTH,
    ],
};

pub const FILTER_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: FILTER_TYPE_ID,
    fields: &[field::TYPE_ID, field::FILTER_TYPE, field::STRENGTH],
};

pub const PEN_FILTER_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: PEN_FILTER_TYPE_ID,
    fields: &[
        field::TYPE_ID,
        field::POINTS,
        field::FILTER_TYPE,
        field::STRENGTH,
        field::STROKE_WIDTH,
    ],
};

pub const SPOTLIGHT_SCHEMA: ElementPayloadSchema = ElementPayloadSchema {
    type_id: SPOTLIGHT_TYPE_ID,
    fields: &[field::TYPE_ID],
};

pub const ELEMENT_PAYLOAD_SCHEMAS: &[ElementPayloadSchema] = &[
    RECTANGLE_SCHEMA,
    ARROW_SCHEMA,
    LINE_SCHEMA,
    FREE_DRAW_SCHEMA,
    TEXT_SCHEMA,
    SERIAL_NUMBER_SCHEMA,
    RECTANGLE_HIGHLIGHT_SCHEMA,
    PEN_HIGHLIGHT_SCHEMA,
    FILTER_SCHEMA,
    PEN_FILTER_SCHEMA,
    SPOTLIGHT_SCHEMA,
];

pub fn payload_schema(type_id: &str) -> Option<&'static ElementPayloadSchema> {
    ELEMENT_PAYLOAD_SCHEMAS
        .iter()
        .find(|schema| schema.type_id == type_id)
}

#[cfg(test)]
mod tests {
    use std::collections::HashSet;

    use super::*;

    #[test]
    fn every_canvas_element_has_one_unambiguous_payload_schema() {
        let type_ids = ELEMENT_PAYLOAD_SCHEMAS
            .iter()
            .map(|schema| schema.type_id)
            .collect::<HashSet<_>>();

        assert_eq!(type_ids.len(), ELEMENT_PAYLOAD_SCHEMAS.len());
        for schema in ELEMENT_PAYLOAD_SCHEMAS {
            assert_eq!(schema.fields.first(), Some(&field::TYPE_ID));
            assert_eq!(
                schema.fields.iter().collect::<HashSet<_>>().len(),
                schema.fields.len()
            );
            assert_eq!(payload_schema(schema.type_id), Some(schema));
        }
    }

    #[test]
    fn common_element_state_fields_match_the_reference_boundary() {
        assert_eq!(
            ELEMENT_STATE_FIELDS,
            &["id", "rect", "rotation", "opacity", "zIndex", "data"]
        );
    }
}
