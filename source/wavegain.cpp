//------------------------------------------------------------------------
// Copyright(c) 2026 Airflame.
//------------------------------------------------------------------------

#include "wavegain.h"
#include "cids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"

using namespace Steinberg;

namespace Airflame {
//------------------------------------------------------------------------
// Wavegain
//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::initialize (FUnknown* context)
{
	// Here the Plug-in will be instantiated

	//---always initialize the parent-------
	tresult result = SingleComponentEffect::initialize (context);
	// if everything Ok, continue
	if (result != kResultOk)
	{
		return result;
	}

	//--- create Audio IO ------
	addAudioInput (STR16 ("Stereo In"), Vst::SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), Vst::SpeakerArr::kStereo);

	/* If you don't need an event bus, you can remove the next line */
	addEventInput (STR16 ("Event In"), 1);

	// Here you could register some parameters
	Vst::Parameter* gainParam = new Vst::RangeParameter(USTRING("Volume"), 0, USTRING("dB"), -60, +12, -3);
	parameters.addParameter(gainParam);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::terminate ()
{
	// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

	//---do not forget to call parent ------
	return SingleComponentEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::setActive (TBool state)
{
	//--- called when the Plug-in is enable/disable (On/Off) -----
	return SingleComponentEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::process (Vst::ProcessData& data)
{
	//--- First : Read inputs parameter changes-----------

	/*if (data.inputParameterChanges)
	{
		int32 numParamsChanged = data.inputParameterChanges->getParameterCount ();
		for (int32 index = 0; index < numParamsChanged; index++)
		{
			if (auto* paramQueue = data.inputParameterChanges->getParameterData (index))
			{
				Vst::ParamValue value;
				int32 sampleOffset;
				int32 numPoints = paramQueue->getPointCount ();
				switch (paramQueue->getParameterId ())
				{
				}
			}
		}
	}*/

	//--- Here you have to implement your processing

	if (data.numSamples > 0)
	{
		//--- ------------------------------------------
		// here as example a default implementation where we try to copy the inputs to the outputs:
		// if less input than outputs then clear outputs
		//--- ------------------------------------------

		int32 minBus = std::min (data.numInputs, data.numOutputs);
		for (int32 i = 0; i < minBus; i++)
		{
			int32 minChan = std::min (data.inputs[i].numChannels, data.outputs[i].numChannels);
			for (int32 c = 0; c < minChan; c++)
			{
				// do not need to be copied if the buffers are the same
				if (data.outputs[i].channelBuffers32[c] != data.inputs[i].channelBuffers32[c])
				{
					memcpy (data.outputs[i].channelBuffers32[c], data.inputs[i].channelBuffers32[c],
							data.numSamples * sizeof (Vst::Sample32));
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
				data.outputs[i].silenceFlags |= ((uint64)1 << c);
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
			data.outputs[i].silenceFlags = ((uint64)1 << data.outputs[i].numChannels) - 1;
		}
	}

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::setupProcessing (Vst::ProcessSetup& newSetup)
{
	//--- called before any processing ----
	return SingleComponentEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::canProcessSampleSize (int32 symbolicSampleSize)
{
	// by default kSample32 is supported
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

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Wavegain::getState (IBStream* state)
{
	// here we need to save the model
	IBStreamer streamer (state, kLittleEndian);

	return kResultOk;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API Wavegain::createView (FIDString name)
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
} // namespace Airflame
