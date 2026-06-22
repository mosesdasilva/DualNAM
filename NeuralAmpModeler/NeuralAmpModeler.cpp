#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <iostream>
#include <utility>

#include "Colors.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#include "NeuralAmpModelerControls.h"

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text5
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle titleStyle =
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = false;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;


NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  _InitToneStack();
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 1, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kSlim)->InitDouble("Slim", 0.0, 0.0, 1.0, 0.01);
  GetParam(kModelAInputLevel)->InitGain("Input A", 0.0, -20.0, 20.0, 0.1);
  GetParam(kModelBInputLevel)->InitGain("Input B", 0.0, -20.0, 20.0, 0.1);
  GetParam(kModelAOutputLevel)->InitGain("Output A", 0.0, -40.0, 40.0, 0.1);
  GetParam(kModelBOutputLevel)->InitGain("Output B", 0.0, -40.0, 40.0, 0.1);
  GetParam(kModelAEQActive)->InitBool("EQ A", true);
  GetParam(kModelABass)->InitDouble("Bass A", 5.0, 0.0, 10.0, 0.1);
  GetParam(kModelAMid)->InitDouble("Middle A", 5.0, 0.0, 10.0, 0.1);
  GetParam(kModelATreble)->InitDouble("Treble A", 5.0, 0.0, 10.0, 0.1);
  GetParam(kModelBEQActive)->InitBool("EQ B", true);
  GetParam(kModelBBass)->InitDouble("Bass B", 5.0, 0.0, 10.0, 0.1);
  GetParam(kModelBMid)->InitDouble("Middle B", 5.0, 0.0, 10.0, 0.1);
  GetParam(kModelBTreble)->InitDouble("Treble B", 5.0, 0.0, 10.0, 0.1);

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto slimIconSVG = pGraphics->LoadSVG(SLIMMABLE_ICON_FN);

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);

    const auto b = pGraphics->GetBounds();
    const auto settingsButtonArea = CornerButtonArea(b);

    // Model loader button
    auto makeLoadModelCompletionHandler = [&](const dualnam::ModelSlot slot) {
      return [&, slot](const WDL_String& fileName, const WDL_String& path) {
        if (!fileName.GetLength())
          return;

        const std::string msg = _StageModel(slot, fileName);
        if (!msg.empty())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
      };
    };

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamAFileString = "Select model A directory...";
    const std::string defaultNamBFileString = "Select model B directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamAFileString = "Select model A...";
    const std::string defaultNamBFileString = "Select model B...";
    const std::string defaultIRString = "Select IR...";
#endif
    // Getting started page listing additional resources
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";

    auto hideSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(true);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(true);
      ui->SetAllControlsDirty();
    };
    auto showSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(false);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(false);
      ui->SetAllControlsDirty();
    };

    auto attachGlobalStrip = [&](const IRECT& stripBounds) {
      const auto knobSize = 75.0f;
      const auto knobTop = stripBounds.T + 12.0f;
      const auto knobGap = 22.0f;
      const auto firstKnobLeft = stripBounds.L + 35.0f;
      const auto globalInputArea =
        IRECT(firstKnobLeft, knobTop, firstKnobLeft + knobSize, knobTop + knobSize);
      const auto gateThresholdArea = globalInputArea.GetHShifted(knobSize + knobGap);
      const auto globalOutputArea = gateThresholdArea.GetHShifted(knobSize + knobGap);
      const auto gateToggleArea =
        IRECT(gateThresholdArea.L - 4.0f, stripBounds.B - 28.0f, gateThresholdArea.R + 4.0f, stripBounds.B - 4.0f);

      pGraphics->AttachControl(new IBitmapControl(stripBounds, backgroundBitmap));
      pGraphics->AttachControl(new IBitmapControl(stripBounds, linesBitmap));
      pGraphics->AttachControl(
        new NAMKnobControl(globalInputArea, kInputLevel, "GLOBAL INPUT", style, knobBackgroundBitmap));
      pGraphics->AttachControl(
        new NAMKnobControl(gateThresholdArea, kNoiseGateThreshold, "GATE THRESHOLD", style, knobBackgroundBitmap));
      pGraphics->AttachControl(
        new NAMKnobControl(globalOutputArea, kOutputLevel, "GLOBAL OUTPUT", style, knobBackgroundBitmap));
      pGraphics->AttachControl(
        new NAMSwitchControl(gateToggleArea, kNoiseGateActive, "Noise Gate", style, switchHandleBitmap));
    };

    auto attachChannelPanel = [&](const IRECT& panelBounds, const char* channelTitle, const dualnam::ModelSlot modelSlot,
                                  const int modelInputParam, const int modelOutputParam, const int modelBrowserTag,
                                  const int clearModelMessage, const char* defaultModelString, const int inputMeterTag,
                                  const int outputMeterTag, const int eqActiveParam, const int bassParam,
                                  const int midParam, const int trebleParam, const char* eqGroup,
                                  const bool sharedControlsActive) {
      const auto mainArea = panelBounds.GetPadded(-20);
      const auto contentArea = mainArea.GetPadded(-10);
      const auto titleHeight = 50.0f;
      const auto titleArea = contentArea.GetFromTop(titleHeight);
      const auto knobsArea = contentArea.GetFromTop(NAM_KNOB_HEIGHT)
                               .GetReducedFromLeft(20.0f)
                               .GetReducedFromRight(20.0f)
                               .GetVShifted(titleHeight + 25.0f);
      constexpr int channelKnobCount = 5;
      const auto inputKnobArea = knobsArea.GetGridCell(0, 0, 1, channelKnobCount).GetPadded(2.0f);
      const auto bassKnobArea = knobsArea.GetGridCell(0, 1, 1, channelKnobCount).GetPadded(2.0f);
      const auto midKnobArea = knobsArea.GetGridCell(0, 2, 1, channelKnobCount).GetPadded(2.0f);
      const auto trebleKnobArea = knobsArea.GetGridCell(0, 3, 1, channelKnobCount).GetPadded(2.0f);
      const auto outputKnobArea = knobsArea.GetGridCell(0, 4, 1, channelKnobCount).GetPadded(2.0f);
      const auto eqToggleArea = midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

      const auto fileWidth = 200.0f;
      const auto fileHeight = 30.0f;
      const auto modelRow = contentArea.GetFromBottom(2.0f * fileHeight).GetFromTop(fileHeight).GetVShifted(-1.0f);
      const auto modelArea = modelRow.GetFromLeft(fileWidth).GetHShifted(35.0f);
      const auto modelIconArea = modelArea.GetFromLeft(30.0f).GetTranslated(-40.0f, 10.0f);
      const auto slimIconArea =
        IRECT(modelArea.R + 6.0f, modelArea.MH() - 14.0f, modelArea.R + 62.0f, modelArea.MH() + 14.0f);
      const auto irArea = modelRow.GetMidHPadded(fileWidth).GetVShifted(38.0f);
      const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);
      const auto inputMeterArea =
        contentArea.GetFromLeft(30.0f).GetHShifted(-20.0f).GetMidVPadded(100.0f).GetVShifted(-25.0f);
      const auto outputMeterArea =
        contentArea.GetFromRight(30.0f).GetHShifted(20.0f).GetMidVPadded(100.0f).GetVShifted(-25.0f);

      pGraphics->AttachControl(new IBitmapControl(panelBounds, backgroundBitmap));
      pGraphics->AttachControl(new IBitmapControl(panelBounds, linesBitmap));
      pGraphics->AttachControl(new IVLabelControl(titleArea, channelTitle, titleStyle));
      pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

      pGraphics->AttachControl(
        new NAMFileBrowserControl(modelArea, clearModelMessage, defaultModelString, "nam",
                                  makeLoadModelCompletionHandler(modelSlot), style, fileSVG, crossSVG, leftArrowSVG,
                                  rightArrowSVG, fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl),
        modelBrowserTag);

      auto* slimControl = new NAMSquareButtonControl(slimIconArea, DefaultClickActionFunc, slimIconSVG);
      pGraphics->AttachControl(slimControl, sharedControlsActive ? kCtrlTagSlimmableIcon : -1);
      if (sharedControlsActive)
        slimControl->SetAnimationEndActionFunction(showSlimOverlay)->Hide(true);
      else
        slimControl->SetDisabled(true);

      auto* irSwitch = new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle);
      pGraphics->AttachControl(irSwitch);
      auto* irBrowser =
        new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                  fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                  "Get IRs", getUrl);
      pGraphics->AttachControl(irBrowser, sharedControlsActive ? kCtrlTagIRFileBrowser : -1);

      auto* eqSwitch = new NAMSwitchControl(eqToggleArea, eqActiveParam, "EQ", style, switchHandleBitmap);
      pGraphics->AttachControl(eqSwitch);

      pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, modelInputParam, "", style, knobBackgroundBitmap));
      auto* bassKnob = new NAMKnobControl(bassKnobArea, bassParam, "", style, knobBackgroundBitmap);
      auto* midKnob = new NAMKnobControl(midKnobArea, midParam, "", style, knobBackgroundBitmap);
      auto* trebleKnob = new NAMKnobControl(trebleKnobArea, trebleParam, "", style, knobBackgroundBitmap);
      auto* outputKnob = new NAMKnobControl(outputKnobArea, modelOutputParam, "", style, knobBackgroundBitmap);
      pGraphics->AttachControl(bassKnob, -1, eqGroup);
      pGraphics->AttachControl(midKnob, -1, eqGroup);
      pGraphics->AttachControl(trebleKnob, -1, eqGroup);
      pGraphics->AttachControl(outputKnob);

      auto* inputMeter = new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style);
      auto* outputMeter = new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style);
      pGraphics->AttachControl(inputMeter, inputMeterTag);
      pGraphics->AttachControl(outputMeter, outputMeterTag);

      if (!sharedControlsActive)
      {
        irSwitch->SetDisabled(true);
        irBrowser->SetDisabled(true);
      }
    };

    const auto globalStripHeight = 120.0f;
    const auto globalStrip = b.GetFromTop(globalStripHeight);
    const auto channelPanels = b.GetReducedFromTop(globalStripHeight);
    const auto channelPanelWidth = channelPanels.W() / 2.0f;
    attachGlobalStrip(globalStrip);
    attachChannelPanel(channelPanels.GetFromLeft(channelPanelWidth), "CHANNEL A", dualnam::ModelSlot::A, kModelAInputLevel,
                       kModelAOutputLevel, kCtrlTagModelAFileBrowser, kMsgTagClearModelA,
                       defaultNamAFileString.c_str(), kCtrlTagInputMeterA, kCtrlTagOutputMeterA, kModelAEQActive,
                       kModelABass, kModelAMid, kModelATreble, "EQ_A_KNOBS", true);
    attachChannelPanel(channelPanels.GetFromRight(channelPanelWidth), "CHANNEL B", dualnam::ModelSlot::B, kModelBInputLevel,
                       kModelBOutputLevel, kCtrlTagModelBFileBrowser, kMsgTagClearModelB,
                       defaultNamBFileString.c_str(), kCtrlTagInputMeterB, kCtrlTagOutputMeterB, kModelBEQActive,
                       kModelBBass, kModelBMid, kModelBTreble, "EQ_B_KNOBS", false);

    // Settings/help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    const auto slimKnobArea = b.GetCentredInside(100.f, NAM_KNOB_HEIGHT + 24.f);
    pGraphics->AttachControl(new NAMSlimOverlayBackdropControl(b, hideSlimOverlay), kCtrlTagSlimOverlayBackdrop)
      ->Hide(true);
    pGraphics
      ->AttachControl(new NAMKnobControl(slimKnobArea, kSlim, "Slim", style, knobBackgroundBitmap), kCtrlTagSlimKnob)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  _DeallocateIOPointers();
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  // Preserve left/right input separation for the two independent model slots.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ApplyDSPStaging();
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive[dualnam::kStereoChannels]{GetParam(kModelAEQActive)->Bool(),
                                                       GetParam(kModelBEQActive)->Bool()};

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }

  iplug::sample* modelInputPointers[dualnam::kStereoChannels]{
    mModelInputArray[static_cast<size_t>(dualnam::ModelSlot::A)].data(),
    mModelInputArray[static_cast<size_t>(dualnam::ModelSlot::B)].data()};
  dualnam::ApplyStereoInputGains(triggerOutput, modelInputPointers, nFrames, mModelInputGains);

  ResamplingNAM* models[dualnam::kStereoChannels]{mModelSlots.Live(dualnam::ModelSlot::A),
                                                  mModelSlots.Live(dualnam::ModelSlot::B)};
  dualnam::ProcessStereoModels(modelInputPointers, mOutputPointers, nFrames, models);
  // Apply the noise gate after the NAM
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  sample* toneStackOutPointers[dualnam::kStereoChannels]{};
  dsp::tone_stack::AbstractToneStack* toneStacks[dualnam::kStereoChannels]{
    mToneStacks[static_cast<size_t>(dualnam::ModelSlot::A)].get(),
    mToneStacks[static_cast<size_t>(dualnam::ModelSlot::B)].get()};
  dualnam::RouteStereoEffects(gateGainOutput, toneStackOutPointers, nFrames, toneStacks, toneStackActive);

  sample** irPointers = toneStackOutPointers;
  if (mIR != nullptr && GetParam(kIRToggle)->Value())
    irPointers = mIR->Process(toneStackOutPointers, numChannelsInternal, numFrames);

  // And the HPF for DC offset (Issue 271)
  const double highPassCutoffFreq = kDCBlockerFrequency;
  // const double lowPassCutoffFreq = 20000.0;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  // const recursive_linear_filter::LowPassParams lowPassParams(sampleRate, lowPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  // mLowPass.SetParams(lowPassParams);
  sample** hpfPointers = mHighPass.Process(irPointers, numChannelsInternal, numFrames);
  // sample** lpfPointers = mLowPass.Process(hpfPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(modelInputPointers, outputs, numFrames);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSenderA.Reset(sampleRate);
  mInputSenderB.Reset(sampleRate);
  mOutputSenderA.Reset(sampleRate);
  mOutputSenderB.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  for (auto& toneStack : mToneStacks)
    toneStack->Reset(sampleRate, maxBlockSize);
  _UpdateLatency();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSenderA.TransmitData(*this);
  mInputSenderB.TransmitData(*this);
  mOutputSenderA.TransmitData(*this);
  mOutputSenderB.TransmitData(*this);

  if (mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewModelLoadedInDSP = false;
    }
  }
  if (mModelCleared)
  {
    if (auto* pGraphics = GetUI())
    {
      // FIXME -- need to disable only the "normalized" model
      // pGraphics->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
      static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->ClearModelInfo();
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimKnob))
        p->Hide(true);
      pGraphics->SetAllControlsDirty();
      mModelCleared = false;
    }
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  chunk.PutStr(dualnam::state::kHeader);
  chunk.PutStr(dualnam::state::kSchemaVersion);
  chunk.PutStr(mNAMPathA.Get());
  chunk.PutStr(mNAMPathB.Get());
  chunk.PutStr(mIRPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  // Look for the expected header. If it's there, then we'll know what to do.
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  if (strcmp(header.Get(), dualnam::state::kHeader) == 0)
  {
    return _UnserializeDualNAMState(chunk, pos);
  }
  if (strcmp(header.Get(), dualnam::state::kLegacyNAMHeader) == 0)
  {
    return _UnserializeStateWithKnownVersion(chunk, pos);
  }
  else
  {
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();

  if (mNAMPathA.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelAFileBrowser, kMsgTagLoadedModel, mNAMPathA.GetLength(), mNAMPathA.Get());
    // If it's not loaded yet, then mark as failed.
    // If it's yet to be loaded, then the completion handler will set us straight once it runs.
    if (_ModelA() == nullptr && _StagedModelA() == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelAFileBrowser, kMsgTagLoadFailed);
  }

  if (mNAMPathB.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelBFileBrowser, kMsgTagLoadedModel, mNAMPathB.GetLength(), mNAMPathB.Get());
    if (_ModelB() == nullptr && _StagedModelB() == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelBFileBrowser, kMsgTagLoadFailed);
  }

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  if (_ModelA() != nullptr)
  {
    _UpdateControlsFromModel();
  }
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    // Changes to the input gain
    case kCalibrateInput:
    case kInputCalibrationLevel:
    case kInputLevel: _SetInputGain(); break;
    case kModelAInputLevel:
    case kModelBInputLevel: _SetModelInputGains(); break;
    // Changes to the output gain
    case kModelAOutputLevel:
    case kModelBOutputLevel: _SetModelOutputGains(); break;
    case kOutputLevel: _SetGlobalOutputGain(); break;
    // Independent tone stacks:
    case kModelABass: mToneStacks[0]->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kModelAMid: mToneStacks[0]->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kModelATreble: mToneStacks[0]->SetParam("treble", GetParam(paramIdx)->Value()); break;
    case kModelBBass: mToneStacks[1]->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kModelBMid: mToneStacks[1]->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kModelBTreble: mToneStacks[1]->SetParam("treble", GetParam(paramIdx)->Value()); break;
    case kSlim: _ApplySlimParamToLoadedNAMs(); break;
    default: break;
  }
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kModelAEQActive:
        pGraphics->ForControlInGroup("EQ_A_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        break;
      case kModelBEQActive:
        pGraphics->ForControlInGroup("EQ_B_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        break;
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active); break;
      default: break;
    }
  }
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModelA: mModelSlots.RequestRemove(dualnam::ModelSlot::A); return true;
    case kMsgTagClearModelB: mModelSlots.RequestRemove(dualnam::ModelSlot::B); return true;
    case kMsgTagClearIR: mShouldRemoveIR = true; return true;
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);

      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());

            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }

      return true;
    }
    default: return false;
  }
}

// Private methods ============================================================

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_ApplyDSPStaging()
{
  const auto modelChanges = mModelSlots.ApplyPending();
  const auto modelAChange = modelChanges[static_cast<size_t>(dualnam::ModelSlot::A)];
  if (modelAChange == dualnam::SlotChange::Removed)
  {
    mNAMPathA.Set("");
    mModelCleared = true;
  }
  const auto modelBChange = modelChanges[static_cast<size_t>(dualnam::ModelSlot::B)];
  if (modelBChange == dualnam::SlotChange::Removed)
    mNAMPathB.Set("");
  if (mShouldRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  if (modelAChange == dualnam::SlotChange::Activated)
  {
    mNewModelLoadedInDSP = true;
  }
  if (modelAChange != dualnam::SlotChange::None ||
      modelBChange != dualnam::SlotChange::None)
  {
    _UpdateLatency();
    _SetInputGain();
    _SetModelOutputGains();
  }
  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }
}

void NeuralAmpModeler::_DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  for (const auto slot : {dualnam::ModelSlot::A, dualnam::ModelSlot::B})
  {
    if (auto* staged = mModelSlots.Staged(slot))
      staged->Reset(sampleRate, maxBlockSize);
    else if (auto* live = mModelSlots.Live(slot))
      live->Reset(sampleRate, maxBlockSize);
  }

  // IR
  if (mStagedIR != nullptr)
  {
    const double irSampleRate = mStagedIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
}

void NeuralAmpModeler::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  // Input calibration
  if ((_ModelA() != nullptr) && (_ModelA()->HasInputLevel()) && GetParam(kCalibrateInput)->Bool())
  {
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - _ModelA()->GetInputLevel();
  }
  mInputGain = DBToAmp(inputGainDB);
}

void NeuralAmpModeler::_SetModelInputGains()
{
  mModelInputGains[static_cast<size_t>(dualnam::ModelSlot::A)] =
    dualnam::DecibelsToLinear(GetParam(kModelAInputLevel)->Value());
  mModelInputGains[static_cast<size_t>(dualnam::ModelSlot::B)] =
    dualnam::DecibelsToLinear(GetParam(kModelBInputLevel)->Value());
}

void NeuralAmpModeler::_SetModelOutputGains()
{
  mModelOutputGains[static_cast<size_t>(dualnam::ModelSlot::A)] =
    dualnam::DecibelsToLinear(GetParam(kModelAOutputLevel)->Value());
  mModelOutputGains[static_cast<size_t>(dualnam::ModelSlot::B)] =
    dualnam::DecibelsToLinear(GetParam(kModelBOutputLevel)->Value());
}

void NeuralAmpModeler::_SetGlobalOutputGain()
{
  mGlobalOutputGain = dualnam::DecibelsToLinear(GetParam(kOutputLevel)->Value());
}

void NeuralAmpModeler::_ApplySlimParamToLoadedNAMs()
{
  const double v = GetParam(kSlim)->Value();
  auto apply = [v](ResamplingNAM* p) {
    if (p == nullptr)
      return;
    if (nam::SlimmableModel* s = p->GetSlimmableModel())
      s->SetSlimmableSize(v);
  };
  for (const auto slot : {dualnam::ModelSlot::A, dualnam::ModelSlot::B})
  {
    apply(mModelSlots.Live(slot));
    apply(mModelSlots.Staged(slot));
  }
}

std::string NeuralAmpModeler::_StageModel(const dualnam::ModelSlot slot, const WDL_String& modelPath)
{
  WDL_String& slotPath = slot == dualnam::ModelSlot::A ? mNAMPathA : mNAMPathB;
  const int browserTag = slot == dualnam::ModelSlot::A ? kCtrlTagModelAFileBrowser : kCtrlTagModelBFileBrowser;
  WDL_String previousNAMPath = slotPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath);

    // Check that the model has 1 input and 1 output channel
    if (model->NumInputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 input channel, but has " + std::to_string(model->NumInputChannels()));
    }
    if (model->NumOutputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 output channel, but has "
                               + std::to_string(model->NumOutputChannels()));
    }

    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), GetBlockSize());
    if (nam::SlimmableModel* slimmable = temp->GetSlimmableModel())
    {
      slimmable->SetSlimmableSize(GetParam(kSlim)->Value());
    }
    mModelSlots.Stage(slot, std::move(temp));
    slotPath = modelPath;
    SendControlMsgFromDelegate(browserTag, kMsgTagLoadedModel, slotPath.GetLength(), slotPath.Get());
  }
  catch (std::runtime_error& e)
  {
    SendControlMsgFromDelegate(browserTag, kMsgTagLoadFailed);

    mModelSlots.CancelStaged(slot);
    slotPath = previousNAMPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mStagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mIRPath = irPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
  }
  else
  {
    if (mStagedIR != nullptr)
    {
      mStagedIR = nullptr;
    }
    mIRPath = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::_InitToneStack()
{
  for (auto& toneStack : mToneStacks)
    toneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
}
void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mModelInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mModelInputArray.size(); c++)
    {
      mModelInputArray[c].resize(numFrames);
      std::fill(mModelInputArray[c].begin(), mModelInputArray[c].end(), 0.0);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  if (nChansOut != dualnam::kStereoChannels)
  {
    std::stringstream ss;
    ss << "Expected " << dualnam::kStereoChannels << " internal channels, but " << nChansOut
       << " channels were requested.";
    throw std::runtime_error(ss.str());
  }

  for (size_t channel = 0; channel < nChansOut; ++channel)
  {
    const bool inputConnected = channel < nChansIn;
    for (size_t s = 0; s < nFrames; s++)
      mInputArray[channel][s] = inputConnected ? mInputGain * inputs[channel][s] : 0.0;
  }
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  if (nChansIn != dualnam::kStereoChannels)
    throw std::runtime_error("DualNAM requires two internal output channels.");

  dualnam::ApplyStereoOutputGains(inputs, outputs, static_cast<int>(nFrames), mModelOutputGains, nChansOut);
  dualnam::ApplyGlobalStereoGain(outputs, static_cast<int>(nFrames), mGlobalOutputGain, nChansOut);

#ifdef APP_API // Ensure valid output to interface
  for (size_t channel = 0; channel < nChansOut; ++channel)
    for (size_t s = 0; s < nFrames; ++s)
    {
      const sample value = outputs[channel][s];
      outputs[channel][s] = std::clamp(value, -1.0, 1.0);
    }
#endif
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  if (_ModelA() == nullptr)
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = _ModelA()->GetEncapsulatedSampleRate();
    modelInfo.inputCalibrationLevel.known = _ModelA()->HasInputLevel();
    modelInfo.inputCalibrationLevel.value = _ModelA()->HasInputLevel() ? _ModelA()->GetInputLevel() : 0.0;
    modelInfo.outputCalibrationLevel.known = _ModelA()->HasOutputLevel();
    modelInfo.outputCalibrationLevel.value = _ModelA()->HasOutputLevel() ? _ModelA()->GetOutputLevel() : 0.0;

    static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->SetModelInfo(modelInfo);

    const bool disableInputCalibrationControls = !_ModelA()->HasInputLevel();
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
    {
      auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode));
      c->SetNormalizedDisable(!_ModelA()->HasLoudness());
      c->SetCalibratedDisable(!_ModelA()->HasOutputLevel());
    }

    if (auto* pSlimIcon = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
    {
      const bool show = _ModelA()->GetSlimmableModel() != nullptr;
      pSlimIcon->Hide(!show);
    }
  }
}

void NeuralAmpModeler::_UpdateLatency()
{
  int latency = mModelSlots.MaximumLatency();
  // Other things that add latency here...

  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames)
{
  sample* inputA[1]{dualnam::StereoChannelPointer(inputPointer, static_cast<size_t>(dualnam::ModelSlot::A))};
  sample* inputB[1]{dualnam::StereoChannelPointer(inputPointer, static_cast<size_t>(dualnam::ModelSlot::B))};
  sample* outputA[1]{dualnam::StereoChannelPointer(outputPointer, static_cast<size_t>(dualnam::ModelSlot::A))};
  sample* outputB[1]{dualnam::StereoChannelPointer(outputPointer, static_cast<size_t>(dualnam::ModelSlot::B))};

  mInputSenderA.ProcessBlock(inputA, static_cast<int>(nFrames), kCtrlTagInputMeterA);
  mInputSenderB.ProcessBlock(inputB, static_cast<int>(nFrames), kCtrlTagInputMeterB);
  mOutputSenderA.ProcessBlock(outputA, static_cast<int>(nFrames), kCtrlTagOutputMeterA);
  mOutputSenderB.ProcessBlock(outputB, static_cast<int>(nFrames), kCtrlTagOutputMeterB);
}

// HACK
#include "Unserialization.cpp"
