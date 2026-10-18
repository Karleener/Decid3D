// Pont entre le rendu logiciel du plan (RenduPlan.h, sans Slint) et Slint.
#pragma once
#include <cstring>
#include <slint.h>

#include "RenduPlan.h"

namespace app {

inline slint::Image VersImageSlint(const ImagePlan& plan) {
    static_assert(sizeof(PixelRgba) == sizeof(slint::Rgba8Pixel), "disposition des pixels");
    slint::SharedPixelBuffer<slint::Rgba8Pixel> tampon((std::uint32_t)plan.m_largeur, (std::uint32_t)plan.m_hauteur);
    std::memcpy(tampon.begin(), plan.m_pixels.data(), plan.m_pixels.size() * sizeof(PixelRgba));
    return slint::Image(tampon);
}

} // namespace app
