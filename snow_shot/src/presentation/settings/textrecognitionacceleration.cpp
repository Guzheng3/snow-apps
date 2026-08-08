#include "snow_shot/presentation/settings/textrecognitionacceleration.h"

#include "snow_ocr_c.h"

namespace snow_shot::presentation::settings {

bool directMlTextRecognitionSupported() {
    static const bool supported = snow_ocr_directml_is_available() != 0;
    return supported;
}

} // namespace snow_shot::presentation::settings
