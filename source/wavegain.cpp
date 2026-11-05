//------------------------------------------------------------------------
// Copyright(c) 2026 Airflame.
//------------------------------------------------------------------------

#define _USE_MATH_DEFINES // exposes M_PI on MSVC, must come before any include

#include "wavegain.h"
#include "cids.h"
#include "wavegainutils.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace Airflame {
//------------------------------------------------------------------------
// Wavegain
//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::initialize (FUnknown* context)
{
	//---always initialize the parent-------
	const tresult result = SingleComponentEffect::initialize (context);
	// if everything Ok, continue
	if (result != kResultOk)
	{
		return result;
	}

	addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);

	Vst::Parameter* gainParam = new Vst::RangeParameter(USTRING("Volume"), kGainParam, USTRING("dB"), -60, +12, -3);
	gainCurr = WavegainUtils::dbToGain (gainParam->toPlain (gainParam->getInfo().defaultNormalizedValue));
	gainPrev = gainCurr;

	Vst::Parameter* lfoFrequencyParam = new Vst::RangeParameter(USTRING("LFO Frequency"), kLfoFrequencyParam, USTRING("Hz"), 0.5, 10, 1);
	lfoFrequencyHz = lfoFrequencyParam->toPlain (lfoFrequencyParam->getInfo().defaultNormalizedValue);

	Vst::Parameter* lfoAmplitudeParam = new Vst::RangeParameter(USTRING("LFO Amplitude"), kLfoAmplitudeParam, USTRING("dB"), 0, 10, 3);
	lfoAmplitudeDbCurr = lfoAmplitudeParam->toPlain (lfoAmplitudeParam->getInfo().defaultNormalizedValue);
	lfoAmplitudeDbPrev = lfoAmplitudeDbCurr;

	parameters.addParameter (gainParam);
	parameters.addParameter (lfoFrequencyParam);
	parameters.addParameter (lfoAmplitudeParam);
	parameters.addParameter (STR16 ("Bypass"), nullptr, 1, 0,
	                         Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsBypass,
	                         kBypassParam);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::terminate ()
{
	return SingleComponentEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::setActive (const TBool state)
{
	return SingleComponentEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::process (Vst::ProcessData& data)
{
	if (data.inputParameterChanges)
	{
		const int32 numParamsChanged = data.inputParameterChanges->getParameterCount ();
		for (int32 index = 0; index < numParamsChanged; index++)
		{
			if (auto* paramQueue = data.inputParameterChanges->getParameterData (index))
			{
				Vst::ParamValue value;
				int32 sampleOffset;
				const int32 numPoints = paramQueue->getPointCount ();
				switch (paramQueue->getParameterId ())
				{
					case kGainParam:
						if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) == kResultTrue)
						{
							gainCurr = WavegainUtils::dbToGain (parameters.getParameter (kGainParam)->toPlain (value));
						}
						break;
					case kLfoFrequencyParam:
						if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) == kResultTrue)
						{
							lfoFrequencyHz = parameters.getParameter (kLfoFrequencyParam)->toPlain (value);
						}
						break;
					case kLfoAmplitudeParam:
						if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) == kResultTrue)
						{
							lfoAmplitudeDbCurr = parameters.getParameter (kLfoAmplitudeParam)->toPlain (value);
						}
						break;
					case kBypassParam:
						if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) == kResultTrue)
						{
							bypass = value >= 0.5;
						}
						break;
					default: ;
				}
			}
		}
	}

	if (data.numSamples > 0)
	{
		const double gainStep = (gainCurr - gainPrev) / data.numSamples;
		const double lfoPhaseStep = lfoFrequencyHz / processSetup.sampleRate;
		const double lfoAmplitudeDbStep = (lfoAmplitudeDbCurr - lfoAmplitudeDbPrev) / data.numSamples;
		const float wetCurr = bypass ? 0.f : 1.f;
		const float wetStep = (wetCurr - wetPrev) / static_cast<float>(data.numSamples);

		const int32 minBus = std::min (data.numInputs, data.numOutputs);
		for (int32 bus = 0; bus < minBus; bus++)
		{
			const int32 minChan = std::min (data.inputs[bus].numChannels, data.outputs[bus].numChannels);
			for (int32 channel = 0; channel < minChan; channel++)
			{
				// apply the gain while copying (also works if the buffers are the same)
				const Vst::Sample32* in = data.inputs[bus].channelBuffers32[channel];
				Vst::Sample32* out = data.outputs[bus].channelBuffers32[channel];

				double gain = gainPrev;
				double lfoPhaseSample = lfoPhase;
				double lfoAmplitudeDb = lfoAmplitudeDbPrev;
				float wet = wetPrev;

				for (int32 s = 0; s < data.numSamples; s++)
				{
					gain += gainStep;
					lfoPhaseSample += lfoPhaseStep;
					lfoAmplitudeDb += lfoAmplitudeDbStep;
					wet += wetStep;

					const Vst::Sample32 drySample = in[s];
					const auto processedSample = static_cast<Vst::Sample32>(drySample * applySineModulation(gain, lfoAmplitudeDb, lfoPhaseSample));
					out[s] = processedSample * wet + drySample * (1.f - wet);
				}
			}
			data.outputs[bus].silenceFlags = data.inputs[bus].silenceFlags;

			// clear the remaining output buffers
			for (int32 channel = minChan; channel < data.outputs[bus].numChannels; channel++)
			{
				// clear output buffers
				memset (data.outputs[bus].channelBuffers32[channel], 0,
						data.numSamples * sizeof (Vst::Sample32));

				// inform the host that this channel is silent
				data.outputs[bus].silenceFlags |= (static_cast<uint64>(1) << channel);
			}
		}
		// clear the remaining output buffers
		for (int32 bus = minBus; bus < data.numOutputs; bus++)
		{
			// clear output buffers
			for (int32 channel = 0; channel < data.outputs[bus].numChannels; channel++)
			{
				memset (data.outputs[bus].channelBuffers32[channel], 0,
						data.numSamples * sizeof (Vst::Sample32));
			}
			// inform the host that this bus is silent
			data.outputs[bus].silenceFlags = (static_cast<uint64>(1) << data.outputs[bus].numChannels) - 1;
		}
		gainPrev = gainCurr;
		lfoPhase = std::fmod (lfoPhase + lfoPhaseStep * data.numSamples, 1.);
		lfoAmplitudeDbPrev = lfoAmplitudeDbCurr;
		wetPrev = wetCurr;
	}

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::setupProcessing (Vst::ProcessSetup& newSetup)
{
	return SingleComponentEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::canProcessSampleSize (const int32 symbolicSampleSize)
{
	if (symbolicSampleSize == Vst::kSample32)
		return kResultTrue;

	// disable the following comment if your processing support kSample64
	/* if (symbolicSampleSize == Vst::kSample64)
		return kResultTrue; */

	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::setState (IBStream* state)
{
	// called when we load a preset, the model has to be reloaded
	IBStreamer streamer (state, kLittleEndian);

	double gainNormalized;
	if (!streamer.readDouble (gainNormalized))
		return kResultFalse;

	double lfoFrequencyNormalized;
	if (!streamer.readDouble (lfoFrequencyNormalized))
		return kResultFalse;

	double lfoAmplitudeNormalized;
	if (!streamer.readDouble (lfoAmplitudeNormalized))
		return kResultFalse;

	bool savedBypass;
	if (!streamer.readBool (savedBypass))
		return kResultFalse;

	// update the parameter so the host shows the restored value
	setParamNormalized (kGainParam, gainNormalized);
	gainCurr = WavegainUtils::dbToGain (parameters.getParameter (kGainParam)->toPlain (getParamNormalized (kGainParam)));

	setParamNormalized (kLfoFrequencyParam, lfoFrequencyNormalized);
	lfoFrequencyHz = parameters.getParameter (kLfoFrequencyParam)->toPlain (getParamNormalized (kLfoFrequencyParam));

	setParamNormalized (kLfoAmplitudeParam, lfoAmplitudeNormalized);
	lfoAmplitudeDbCurr = parameters.getParameter (kLfoAmplitudeParam)->toPlain (getParamNormalized (kLfoAmplitudeParam));

	setParamNormalized (kBypassParam, savedBypass ? 1. : 0.);
	bypass = savedBypass;

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::getState (IBStream* state)
{
	// here we need to save the model
	IBStreamer streamer (state, kLittleEndian);

	// save the gain as a normalized value, converted back from the linear factor
	const double gainNormalized = parameters.getParameter (kGainParam)->toNormalized (WavegainUtils::gainToDb (gainCurr));
	if (!streamer.writeDouble (gainNormalized))
		return kResultFalse;

	const double lfoFrequencyNormalized = parameters.getParameter (kLfoFrequencyParam)->toNormalized (lfoFrequencyHz);
	if (!streamer.writeDouble (lfoFrequencyNormalized))
		return kResultFalse;

	const double lfoAmplitudeNormalized = parameters.getParameter (kLfoAmplitudeParam)->toNormalized (lfoAmplitudeDbCurr);
	if (!streamer.writeDouble (lfoAmplitudeNormalized))
		return kResultFalse;

	if (!streamer.writeBool (bypass))
		return kResultFalse;

	return kResultOk;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API Wavegain::createView (const FIDString name)
{
	// Here the Host wants to open your editor (if you have one)
	if (FIDStringsEqual (name, Vst::ViewType::kEditor))
	{
		return new VSTGUI::VST3Editor (this, "view", "editor.uidesc");
	}
	return nullptr;
}

//------------------------------------------------------------------------
double Wavegain::applySineModulation (const double gain, const double lfoAmplitudeDb, const double lfoPhase) {
	return gain * WavegainUtils::dbToGain (lfoAmplitudeDb * std::sin (2. * M_PI * lfoPhase));
}

//------------------------------------------------------------------------
} // namespace Airflame
