//------------------------------------------------------------------------
// Copyright(c) 2026 Airflame.
//------------------------------------------------------------------------

#define _USE_MATH_DEFINES // exposes M_PI on MSVC, must come before any include

#include "wavegain.h"
#include "cids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

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

	Vst::Parameter* gainParam = new Vst::RangeParameter(USTRING("Volume"), 0, USTRING("dB"), -60, +12, -3);
	gainCurr = dbToGain (gainParam->toPlain (gainParam->getInfo().defaultNormalizedValue));
	gainPrev = gainCurr;
	parameters.addParameter(gainParam);

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
					case 0: // gainParam
						if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) == kResultTrue)
						{
							gainCurr = dbToGain (parameters.getParameter (0)->toPlain (value));
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
		constexpr double freqHz = 1.;
		const double phaseStep = freqHz / processSetup.sampleRate;

		const int32 minBus = std::min (data.numInputs, data.numOutputs);
		for (int32 i = 0; i < minBus; i++)
		{
			const int32 minChan = std::min (data.inputs[i].numChannels, data.outputs[i].numChannels);
			for (int32 c = 0; c < minChan; c++)
			{
				// apply the gain while copying (also works if the buffers are the same)
				const Vst::Sample32* in = data.inputs[i].channelBuffers32[c];
				Vst::Sample32* out = data.outputs[i].channelBuffers32[c];
				double gain = gainPrev;
				double phase = lfoPhase;
				for (int32 s = 0; s < data.numSamples; s++)
				{
					gain += gainStep;
					phase += phaseStep;
					out[s] = static_cast<Vst::Sample32>(in[s] * applySineModulation(gain, 3, phase));
				}
			}
			data.outputs[i].silenceFlags = data.inputs[i].silenceFlags;

			// clear the remaining output buffers
			for (int32 c = minChan; c < data.outputs[i].numChannels; c++)
			{
				// clear output buffers
				memset (data.outputs[i].channelBuffers32[c], 0,
						data.numSamples * sizeof (Vst::Sample32));

				// inform the host that this channel is silent
				data.outputs[i].silenceFlags |= (static_cast<uint64>(1) << c);
			}
		}
		// clear the remaining output buffers
		for (int32 i = minBus; i < data.numOutputs; i++)
		{
			// clear output buffers
			for (int32 c = 0; c < data.outputs[i].numChannels; c++)
			{
				memset (data.outputs[i].channelBuffers32[c], 0,
						data.numSamples * sizeof (Vst::Sample32));
			}
			// inform the host that this bus is silent
			data.outputs[i].silenceFlags = (static_cast<uint64>(1) << data.outputs[i].numChannels) - 1;
		}
		gainPrev = gainCurr;
		lfoPhase = std::fmod (lfoPhase + phaseStep * data.numSamples, 1.);
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

	// update the parameter so the host shows the restored value
	setParamNormalized (0, gainNormalized);
	gainCurr = dbToGain (parameters.getParameter (0)->toPlain (getParamNormalized (0)));

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::getState (IBStream* state)
{
	// here we need to save the model
	IBStreamer streamer (state, kLittleEndian);

	// save the gain as a normalized value, converted back from the linear factor
	const double gainNormalized = parameters.getParameter (0)->toNormalized (gainToDb (gainCurr));
	if (!streamer.writeDouble (gainNormalized))
		return kResultFalse;

	return kResultOk;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API Wavegain::createView (const FIDString name)
{
	// Here the Host wants to open your editor (if you have one)
	if (FIDStringsEqual (name, Vst::ViewType::kEditor))
	{
		// create your editor here and return a IPlugView ptr of it
        return nullptr;
	}
	return nullptr;
}

//------------------------------------------------------------------------
double Wavegain::dbToGain (const double db)
{
	return std::pow (10., db / 20.);
}

//------------------------------------------------------------------------
double Wavegain::gainToDb (const double gain)
{
	return 20. * std::log10 (gain);
}

//------------------------------------------------------------------------
double Wavegain::applySineModulation(const double gain, const double deltaDb, const double phase) {
	return gain * dbToGain (deltaDb * std::sin (2. * M_PI * phase));
}

//------------------------------------------------------------------------
} // namespace Airflame
