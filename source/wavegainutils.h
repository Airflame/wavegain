//------------------------------------------------------------------------
// Copyright(c) 2026 Airflame.
//------------------------------------------------------------------------

#pragma once

#include <cmath>

namespace Airflame::WavegainUtils {

//------------------------------------------------------------------------
/** Converts between decibels and a linear gain factor */
inline double dbToGain (const double db)
{
	return std::pow (10., db / 20.);
}

//------------------------------------------------------------------------
inline double gainToDb (const double gain)
{
	return 20. * std::log10 (gain);
}

//------------------------------------------------------------------------
} // namespace Airflame::WavegainUtils
