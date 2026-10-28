//------------------------------------------------------------------------
// Copyright(c) 2026 Airflame.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstsinglecomponenteffect.h"

namespace Airflame {

//------------------------------------------------------------------------
//  Wavegain
//------------------------------------------------------------------------
class Wavegain : public Steinberg::Vst::SingleComponentEffect
{
public:
	Wavegain () = default;
	~Wavegain () SMTG_OVERRIDE = default;

    // Create function
	static FUnknown* createInstance (void* /*context*/)
	{
		return static_cast<IAudioProcessor *>(new Wavegain);
	}

	//--- ---------------------------------------------------------------------
	// SingleComponentEffect overrides:
	//--- ---------------------------------------------------------------------
	/** Called at first after constructor */
	Steinberg::tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;

	/** Called at the end before destructor */
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

	/** Switch the Plug-in on/off */
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;

	/** Will be called before any process call */
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;

	/** Asks if a given sample size is supported see SymbolicSampleSizes. */
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

	/** Here we go...the process call */
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

	/** For persistence */
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	//--- from EditController --------------------------------------------
	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;

 	//---Interface---------
	DEFINE_INTERFACES
		// Here you can add more supported VST3 interfaces
		// DEF_INTERFACE (Vst::IXXX)
	END_DEFINE_INTERFACES (SingleComponentEffect)
    DELEGATE_REFCOUNT (SingleComponentEffect)

//------------------------------------------------------------------------
protected:
	double gainCurr = 0.;
	double gainPrev = 0.;
};

//------------------------------------------------------------------------
} // namespace Airflame
