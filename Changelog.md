# Crusher Changelog

## v1.45.2 — PNG logo click area (2026-11-24)

- Fit the zoom-menu click area to the actual proportionally rendered PNG rectangle rather than the fixed 280 × 43 header area. Anchor the popup to its bottom-left corner at every zoom level. Transparent padding within the PNG remains part of its image bounds. Logo rendering, DSP and all other controls are unchanged.

## v1.45.1 — PNG logo support (2026-11-24)

- Replaced the required SVG logo with `Resources/logo.png` in the Windows build check, CMake embedded binary resources and GUI image loader. The image fits proportionally in the existing 280 × 43 design-pixel logo area, with transparent pixels retained. The logo zoom-menu hit area and anchor remain unchanged. Rebuild after swapping artwork. Resources are excluded; DSP, parameter IDs and all 24 distortion types remain unchanged.
- Retained the red README release badge and adopted the requested shortened introduction. Windows VST3 build and visual testing remain pending.

## v1.45.0 — Sampler distortion (2026-11-24)

- Added Sampler as type 24 (stored index 23), with original vintage converter aperture, analogue loading, DAC code mismatch and reconstruction modelling. Five new controls MODEL, RATE, BITS, COLOUR and PUNCH, with shared DRIVE/OUTPUT; new targets 145–149. Existing IDs and DSP retained.
- MODEL shows nearest morph family, RATE shows converter clock, BITS shows effective interpolated resolution. No synthetic noise. Red README release badge. Resources excluded. Windows VST3 and listening tests pending.

## v1.44.1 — Amp CAB alignment (2026-11-24)

- Moved the Amp CAB label and dropdown 11 design pixels right, centring them beneath DRIVE and SOFT CLIP (within half a pixel). Sizes, vertical positions, other controls, DSP, parameter IDs and sequencer are unchanged.

## v1.44.0 — Cascade selection/index correction (2026-11-24)

- Fixed the distortion selection attachment's obsolete hard-coded 22-item limit. Selecting **Cascade** (menu ID 23, stored index 22) now correctly remains on Cascade instead of reverting to Vocal.
- The selection boundary is now obtained from the existing `distortionType` host parameter, with debug-time checks against both the menu and its description table. This avoids repeating the same last-model indexing mistake when new types are appended.
- Updated the displayed, CMake, build-script and README version to **1.44.0**, keeping the established plugin identity, state schema, and all parameter/type IDs intact. All 23 DSP engines, processing, presets, sequencer targets, and Resources are unchanged. Windows VST3 compilation and PHI verification remain pending.

## v1.43.5 — Cascade multi-stage distortion (2026-11-24)

- Added **Cascade** as distortion type 23 (internal index 22), appending its choice after the existing 22 models without renumbering them. Its independently implemented `CrusherCascade.h` combines 1–16 smoothly interpolated saturation stages, deterministic mixed transfer curves, pre-stage frequency emphasis, asymmetric harmonic variation, and dry-transient restoration aimed at drums, vocals and mixes. All control paths are finite and generated noise is absent. No host memory allocation occurs in audio processing, and only the selected/outgoing model is processed; higher stage counts use more CPU.
- Added five saved, automatable and sequencer/ENV-assignable controls **STAGES**, **VARIATION**, **EMPH**, **BIAS**, **PUNCH**, plus existing shared DRIVE and OUTPUT. Appended sequencer choices 140–144, preserving existing IDs 1–139, the 32 sequencer steps and 16 target slots, and eight ENV slots. STAGES shows its physical 1–16 count in parameter readouts. The alphabetical distortion dropdown, context-specific 81px knob grid, descriptions and tooltip wiring have been extended; previous 22 model DSP files are unchanged.
- Standalone C++17 DSP tests, sanitizer tests, control-impact tests and structural/source-integrity validation are performed locally. The new model does not include oversampling; very high drive and sharp clipping can still alias. Full Windows JUCE VST3 compilation, host GUI tests, listening and measured CPU benchmarking are pending. Source ZIP excludes Resources.


## v1.43.4 — Vocal control rebalancing (2026-11-23)

- Improved the perceptible range of Vocal BODY, PRESENCE, BITE and SMOOTH on vocals, snares, drums and mixed SV-Drummer-style buses. CHARACTER and DRIVE retain their transfer curves, meaning and range. BODY now spans ±15 dB and dynamically covers more low-mid content away from centre; PRESENCE adds post-saturation adjustment; BITE varies the high-band crossover and shaped-signal blend; SMOOTH adds stronger level-responsive taming and progressive post filtering. No new parameters, noise, look-ahead, latency or audio-thread allocations. Control coefficient lookup tables are computed during prepare().
- At the 50% defaults for all four rebalanced controls, standalone comparisons with v1.43.3 are sample-exact across 22.05–192 kHz, all tested DRIVE and CHARACTER positions. Non-neutral v1.43.3 Vocal presets intentionally sound different. All other 21 distortion DSP files, parameter and type IDs, 32-step/16-slot sequencer structure, EQ, filter, comp and mix remain unchanged. Resources excluded from source ZIP. Windows VST3 compilation, PHI listening and CPU benchmarks remain unverified.


## v1.43.3 — Vocal distortion (2026-11-23)

- Added **Vocal** as distortion type 22 (DSP index 21, dropdown ID 22), retaining all 21 existing model IDs, parameters and sound engines. Original `CrusherVocal.h` combines low-band weight, characteristic midrange saturation, adjustable presence and high-band bite with a signal-responsive upper-band smoother, aimed at vocals and drums. It generates no synthetic noise.
- Controls: shared DRIVE and OUTPUT, plus five dedicated saved, host-automatable and drag-to-sequencer/ENV controls: CHARACTER, BODY, PRESENCE, BITE, SMOOTH (appended target choices **135–139**, leaving 1–134 untouched). The six-knob main distortion panel, existing controls and sequencing UI stay in place. Vocal DSP and smoothers advance only while selected or fading out; audio processing allocates no memory.
- Source ZIP excludes Resources. Standalone DSP signal/impact/extreme tests and structural integrity checks completed; full Windows VST3 compilation, DAW listening and CPU measurement remain to be verified.


## v1.43.2 — Measured parameter readouts (2026-11-23)

- Display-only unit readouts replace percentages for 15 parameters: shared filter cutoff (Hz/kHz) and resonance (Q); Codec algorithm name, capture rate (Hz/kHz) and whole-bit resolution; Lo-Fi capture rate and fractional bit resolution; Diode Clip IN HP, ENV, OUT HP and OUT LP (Hz/ms); Fresh FREQ, Razor FREQ and Fuzz TONE (Hz/kHz); Crackle DENSITY (relative multiplier). Numeric text entry translates back to the original percentage-based host parameters.
- Codec and Lo-Fi rate readouts follow the host sample rate. All 0–100 parameter ranges, IDs, defaults, stored states, automation, sequencer/ENV target choices, audio DSP, GUI geometry and the original 21 distortion types are unchanged; parameters with DRIVE-dependent or purely subjective meanings remain percentages.
- Source ZIP excludes Resources. Standalone conversion/round-trip, source-integrity and package checks are performed locally; a full Windows JUCE VST3 build and PHI readout test remain required.

## v1.43.1 — Codec idle-noise suppression (2026-11-23)

- Codec's DAMAGE grain and spectral-mask noise fill now respond to a per-channel input activity detector, preserving their full strength on normal signals while fading generated noise to silence after input ends. Peak-envelope release is 8 ms and sample-rate independent; the full fade to silence is approximately 80 ms after a strong signal, depending on level.
- No new parameters, no audio-thread allocations or transcendentals. Codec frame faults, mask behaviour, quantisation and other control mappings remain intact. All other 20 model DSPs, HumpDSP, original 21 type IDs, 134 sequencer/ENV assignments, EQ, filter, comp, mix/output and Resources are unchanged. Full Windows VST3 build and host listening remain unverified.

## v1.43.0 — Original Codec distortion (2026-11-23)

- Added **Codec** as distortion type 21 (DSP index 20, sorted dropdown ID 21), preserving the existing 20 distortion type parameter indices and the original DSP files. The standalone `source/CrusherCodec.h` implements PCM quantisation, mu-law, A-law, differential predictive quantisation and four-band spectral-loss/quantisation, with controllable capture rate, resolution, frame noise/faults and tonal shaping. These are original codec-inspired sound-design approximations, not actual MP3, AAC, GSM or ToneBoosters implementations.
- CODEC, RATE, BITS, DAMAGE and TONE appear in the existing 3 × 2 grid with shared DRIVE. All five are saved and host-automatable and are appended as sequencer/ENV target choices **130–134**; 1–129 remain untouched. Idle Codec smoothers do not run, and its DSP is used only while selected or fading out. Lookup tables are prepared outside the audio thread, and no runtime allocations or added latency are required.
- Updated CMake, build script, on-screen version and README to 1.43.0. ZIP excludes Resources. Standalone DSP/control/finite-state tests, static ID/wiring checks and unchanged-source comparisons are required before release; Windows VST3 build and real-host listening/CPU tests still require user verification.

## v1.42.1 — Lo-Fi selection crash fix (2026-11-23)

- Fixed a missing Diode Clip entry in the 20-element distortion-details table. Previously only 19 text entries were initialized, so Lo-Fi's message occupied Diode Clip's slot and the 20th pointer was null. Selecting Lo-Fi attempted to append this null string to the GUI status line, potentially crashing the host (including PHI).
- Added compile-time checks that every summary and detail entry is non-null. Distortion indices, parameter IDs, Lo-Fi DSP, all other model DSP, saved state, EQ, filter, compressor, sequencer, and GUI layouts are unchanged. No resource files are included.
- Updated version strings and build metadata to 1.42.1. Static mapping and standalone C++17 description-table checks passed; Windows VST3 build and PHI retest remain required.

## v1.42.0 — Original Lo-Fi combined distortion (2026-11-23)

- Added **Lo-Fi** as the 20th distortion type (dropdown ID 20, DSP index 19) without renumbering the existing 19 models. Its independent `source/CrusherLoFi.h` uses a composite CLIP → RECTIFY → NOISE → optional PRE anti-alias → SAMPLE RATE HOLD + 1–16 BIT quantisation → optional POST low-pass reconstruction chain. Unlike Pixelated, it intentionally permits true one-bit crushing and stacks multiple distortion mechanisms. DRIVE controls the wet intensity; all other model algorithms and the upstream HumpDSP files remain untouched.
- Lo-Fi's six main controls fit the existing three-by-two distortion grid: top **DRIVE / RATE / BITS**, bottom **CLIP / RECTIFY / NOISE**. RATE covers native sample rate through approximately 180 Hz; BITS spans 1 to 16 bits. The two optional anti-alias stages are adjusted via the **RATE right-click menu**, with independent OFF/25/50/75/100% choices and ENV target assignment/removal; their internal host parameters remain continuous 0–100%. Anti-alias filtering is intentionally optional, default OFF for a raw digital-degradation character.
- The five new main parameters and two AA parameters are saved, host-automatable and appended to the existing sequencer/envelope parameter target catalogue as IDs **123–129**. Existing IDs **1–122**, the 32 sequencer steps, 16 sequencer target slots and eight envelope slots are unchanged. The Lo-Fi processor runs only as the selected type or an outgoing crossfade; its seven control smoothers advance only while in use, without audio-thread allocation. Filter/rate lookup tables are prepared before audio processing.
- Updated CMake, Windows build script, GUI version label and instructional README to **1.42.0**. The source ZIP excludes the user's `Resources` folder. Standalone four-sample-rate control-impact and extreme-parameter checks passed; the Windows JUCE VST3 build, in-DAW listening, layout and measured CPU use remain unverified.

## v1.41.3 — EQ/ENV theme-red selection styling (2026-11-23)

- EQ and ENV view selectors use the existing theme-bar muted red (#8e414b) for their selected outline, label and translucent background. Their amber power LEDs, independent power states, view switching, button sizes and rounded corners remain unchanged.
- PHAT retains **exactly** its v1.41.2 styling and behaviour, including its yellow selected text, border and translucent background. LP/BP/HP filter selectors and RESET are unchanged.
- GUI-only colour update; CMake metadata, build script, on-screen version and instructional README increased to v1.41.3. No DSP, parameter, sequencer or Resources changes. Windows VST3/DAW verification pending.

## v1.41.2 — Combined EQ/ENV view-power selectors and unified yellow buttons (2026-11-23)

- Moved the existing independently powered EQ and ENV LEDs inside the left sides of their respective view-selector buttons. Clicking the LED still toggles its APVTS-attached effect enable parameter; clicking the rest of that button changes the visible EQ/ENV view only. Both combined selectors remain visible regardless of the selected view.
- Matched EQ, ENV and PHAT button fill, outline, label colour and 2.6 px corner radius to the LP/BP/HP filter-shape buttons: selected = translucent yellow background, yellow border and yellow text. Unselected buttons use the same dark background and muted outline/text as inactive filter selectors. RESET shares the inactive artwork and remains a momentary EQ reset.
- Re-centred the EQ header controls and restored the taller EQ response graph now that the second LED no longer consumes a separate row. No main panel geometry changes.
- Updated the build script, CMake/JUCE metadata, on-screen label and README to **v1.41.2**. DSP, saved parameters, target assignments, sequencer and Resources unchanged. Windows VST3/DAW verification pending.

## v1.41.1 — Persistent EQ/ENV power LEDs and uniform header buttons (2026-11-23)

- Both EQ and ENV power LEDs now remain visible and clickable when either EQ or ENV is selected, stacked vertically with independent EQ/ENV labels and on/off states. They are hidden only by the unrelated preset-browser overlay.
- EQ, ENV and RESET view/header buttons all use the same 50 x 23 px standard text-button artwork with 3 px corner radii and matching font; active EQ/ENV selection remains highlighted in muted red.
- Renamed FLAT to RESET, including tooltip/help text, without changing the EQ reset action. Lowered the EQ graph by 8 px and reduced its height by 8 px to keep it clear of the lower LED.
- Updated versions in GUI, CMake, build script and instructional README to **1.41.1**. No processor DSP, parameter IDs, sequencer behaviour or resource files changed. Windows VST3/DAW verification remains pending.

## v1.41.0 — Global envelope follower and selectable shared filter shapes (2026-11-23)

- Added three compact clickable response-curve icons between FILTER CUT and RES: LP (the untouched original Hump resonant low-pass/fat-limiter processing), new resonant BP and new resonant HP. Only the selected shared shape runs; FILTER remains independent of the distortion model. Mode is preset-saved and host-automatable; default LP.
- Added EQ/ENV selector to the existing 332-pixel EQ panel. This changes only the displayed controls. Each processor retains its own power LED and remains active when the alternate view is shown; no new main panel and no overall size change.
- Implemented a global pre-distortion stereo-linked peak envelope follower, with host-automatable SENS, ATTACK, RELEASE and bipolar DEPTH, activity meter, and independent ENABLE. Its detector only runs when enabled and an ENV target is assigned; the modulation is resolved at audio-block control rate and applied through the existing smoothed control paths.
- Added eight independent host-saved envelope assignment slots reusing the existing 122 sequencer target parameter choices, without changing the sequencer's 32 steps or 16 target slots. Right-click any target-enabled knob/label to assign or remove a target. EQ point menus gain separate ENV Gain and ENV Freq assignment/removal. The ENV modulation combines with sequencer modulation without modifying the underlying saved values. Existing EQ display follows the combined modulated values.
- Kept all 19 distortion DSP engines, the original HumpDSP source, global seven-band EQ implementation, compressor, master and sequencer architecture untouched. No legacy-parameter migration or backwards-compatibility layer is included, as Crusher has no existing release presets requiring one.
- Updated CMake, VST3 version, build script, GUI version and README to **1.41.0**. Development ZIP excludes Resources; standalone DSP and static checks performed, Windows VST3 build/DAW verification and actual CPU measurements pending.


## v1.40.0 — Sequencer modulation of all seven EQ graph points (2026-11-23)

- Added right-click menu actions **Set Seq Target Gain** (EQ point Gain/dB) and **Set Seq Target Freq** (EQ point Frequency/Hz) to each of the seven existing EQ points. Original filter-shape menu remains available.
- Appended 14 independent sequencer target choices (109–122) without renumbering the previous 108 or changing the 32-step / 16-slot sequencer. Existing assigned targets are re-enabled; otherwise the first empty slot is used. When all slots are full, assignment choices are disabled.
- Applied target depth/direction to the EQ frequency and gain DSP values, preserving stored EQ parameter values and automation. The graph animates modulated points and the response curve, while retaining manual dragging, mouse-wheel Q and reset behaviour.
- EQ coefficients remain recalculated only when band values change, using the existing smoothed biquads. No additional per-sample coefficient calculations, real-time allocations or processing of inactive distortion models. Fast modulation resolution remains limited by audio-block size.
- Updated GUI, CMake and build versions to **1.40.0**; README remains instructional, with development changes only in this changelog. All 19 distortion DSP algorithms, original HumpDSP, and Resources are unchanged. Windows VST3/DAW and actual CPU results require local verification.

## v1.39.0 — Diode Clip control alignment (2026-11-23)

- Rearranged Diode Clip's controls into DRIVE / DIODE / ENV on the upper row and IN HP / OUT HP / OUT LP on the lower row, using the existing three-column knob grid.
- Kept ENV's existing envelope-follower DSP intact. Its speed primarily changes transient clipping response and is intentionally subtle on sustained signals.
- GUI-only correction. All DSP, parameters, host automation, sequencer mappings, effect panels and Resources remain unchanged.

## v1.39.0 — Diode Clip menu and description mapping correction (2026-11-23)

- Corrected the actual selection defect: the distortion menu change handler still clamped its internal model index to an 18-entry description array, silently rewriting Diode Clip (index 18) as Hump (index 17).
- Added Diode Clip as the 19th entry in both description arrays; use the selected model index directly and guard invalid IDs instead of silently clamping them to another type.
- Included all 19 models in the existing randomise function, and added a compile-time check that the two model-description arrays remain the same length.
- No DSP, parameter IDs, sequencer mapping, graphics, Resources, or version changes. Windows VST3 build and DAW behaviour require verification.

## v1.39.0 — Diode Clip dropdown selection fix (2026-11-23)

- Corrected the distortion type menu parameter callback to accept all 19 item IDs rather than clamping to the previous 18. Choosing Diode Clip no longer resets the visible selection to Hump.
- No change to distortion algorithms, audio routing, parameters, MIDI/host automation, sequencer, menu geometry, or the v1.39.0 version number.

## v1.39.0 — Build-only correction (2026-11-23)

- Fixed a Windows C2065 compile error in `PluginEditor.cpp::resized()` by defining `diodeSelected` using the Diode Clip dropdown ID (19) before the shared DRIVE layout condition.
- No changes to DSP, parameters, zoom menu behavior, GUI positions, sequencer, or the 1.39.0 version number.

## v1.39.0 — Diode Clip model and scaled logo menu (2026-11-23)

- Added an original **Diode Clip** distortion type, appended as internal choice 19 without renumbering existing types or changing the Hump model.
- Diode Clip has DRIVE plus IN HP, ENV, DIODE, OUT HP and OUT LP on Crusher's existing three-column by two-row knob grid. Five new controls are saved, host-automatable and drag-to-sequencer targets 104–108; sequencer remains 32 steps / 16 slots.
- Independent DSP uses asymmetric smooth diode-inspired limiting, signal-responsive envelope, pre-emphasis high-pass and post high-/low-pass filters. The algorithm is independently designed rather than sourced from AIR Music Technology and is not guaranteed to match AIR Diode Clip. No third-party AIR code or assets are included.
- Diode Clip DSP and smoothers run only when selected or temporarily fading out during a model crossfade. Coefficients are prepared outside the audio thread; no realtime allocations or per-sample exponentials are added.
- Logo zoom menu now scales its font, popup width and item height to the editor zoom (75%–300%) while staying anchored to the logo's bottom-left. Actual positioning may be constrained by desktop work area at very large zoom levels.
- No processing changes to the existing 18 distortion types, HumpDSP, shared seven-band EQ, FILTER, COMP, MIX/OUT or the sequencer architecture. Existing parameters and target choice indices remain undisturbed; no migration logic added. Resources excluded from source ZIP.
- Static source, standalone DSP and archive checks performed; Windows JUCE/VST3 compile, in-DAW visual/audio testing and real CPU measurement remain outstanding.


## v1.38.0 — First public release version (2026-11-23)

- Promoted the version from v0.1.38 to v1.38.0 in JUCE/CMake plugin metadata, on-screen version label, build script and README.
- No changes to DSP, effect controls, GUI geometry, parameter IDs, presets, sequencer, binary resources or processing behaviour.
- This development source ZIP continues to exclude the user-maintained `Resources` folder; any public source release must provide the assets required to build its matching VST3 binary, with appropriate redistribution rights.

## v0.1.38 — Central seven-band EQ and fixed distortion-red strip (2026-11-23)

- Reordered the main panels to **DISTORTION → OUTPUT/MODE → EQ → FILTER → COMP → MIX/OUT**, retaining precisely 6-pixel gaps and the original FILTER, COMP and MIX/OUT panel locations.
- Reduced the contextual DISTORTION panel to **264 px**, fitting the existing three-column 81-pixel knob grid and Amp CAB control; relocated all contextual knobs, labels, the distortion LED, shared OUTPUT knob and STEREO/MONO radio controls without resizing them.
- Expanded the centrally positioned EQ panel to **332 px**, with a **312 × 126 px** response graph. Increased the shared graphic EQ from five to **seven** independently editable, host-automatable frequency/gain/Q/shape bands; PRE, EMPH, POST, FLAT and amber bypass remain available. The EQ stays bypassed by default, and neutral gains remain 0 dB.
- The top coloured strip is now always the same muted burgundy-red **#8e414b** as THEME's 100% endpoint, independent of the selected distortion model.
- The DSP changes are limited to two added EQ biquad stages per active EQ path (seven instead of five); each distortion model, HumpDSP, compressor, filter, shared OUTPUT/MONO routing, MIX/OUT, and the 32-step/16-target sequencer remain unchanged. Additional EQ processing only occurs when the EQ is enabled; no audio-thread allocations were introduced.
- Previous development EQ parameter values and host states need not be retained; no migration or compatibility code has been added. Resources remain excluded from the source ZIP. Windows JUCE/VST3 build and DAW appearance remain unverified.

## v0.1.37 — Uniform panel spacing, Hump tooltips and bipolar background (2026-11-23)

- Matched the gap between **all six main effect panels** to the sequencer's 6-pixel internal panel separation. Preserved the original rightmost MIX/OUT position; shifted controls in the small panels only as necessary to remain centred with their LEDs.
- Removed HumpCrusher/original-plugin references from the Hump on-screen descriptions and related effect tooltips; original upstream DSP, licence files and README attribution remain unchanged.
- THEME now works like Vortex's bipolar panel surround: **0% = full user-supplied image**, **50% = plain charcoal**, **100% = solid muted distortion red**, with continuous blending. Both main and sequencer surrounds use the same image mapping and independent clipping, keeping the inter-panel gap clear. Dark knob/step/target panels stay opaque.
- Optional `Resources/crusher_background.jpg` is detected and embedded on build. The binary still builds and runs without it; swapping the resource requires rebuilding. No Resources assets are included in the source ZIP.
- Theme is a GUI-only setting (not audio or sequencer state). Neutral default and double-click reset are now 50%. No audio algorithm, parameter ID, preset format, sequencer target, modulation or CPU-DSP path was modified.
- Source/geometry checks and ZIP integrity performed; full Windows VST3 build and DAW visual testing remain to be verified.

## v0.1.36 — Contextual knob-grid layout (2026-11-23)

- Reorganised all six-knob distortion views into a consistent **three columns × two rows** at 81-pixel column spacing. Every DRIVE-based model now places TONE directly beneath DRIVE. Hump has no DRIVE; its TONE sits below its first knob, MECH. No knob sizes or per-model parameters were changed.
- Asym retains five knobs: swapped the lower-row SHAPE and TONE positions to put TONE beneath DRIVE while preventing overlap.
- Amp is now DRIVE / SOFT CLIP / TOP BOOST on the upper row and CAB / BIAS on the lower row. Reduced CAB dropdown from **150 × 25** to **120 × 20** pixels, retaining its original aspect ratio and matching the sequencer dropdown height; its label follows the new position.
- GUI-only change. No changes to DSP, gain, parameters, sequencer targets, filter/compressor, global EQ, shared OUTPUT/MONO, MIX/OUT or Resources. Development ZIP excludes Resources.
- Source/geometry comparisons and archive-integrity checks performed; full Windows VST3 compilation and DAW visual verification are still outstanding.

## v0.1.35 — Shared distortion OUTPUT and pre-distortion STEREO/MONO (2026-11-23)

- Added a new 66-pixel-wide OUTPUT/MODE panel immediately to the left of FILTER. The contextual distortion panel was narrowed, retaining the same 81-pixel knob positions, CAB dropdown, and the original FILTER/COMP/MIX/OUT panel dimensions and positions.
- Removed all 18 model-specific OUTPUT parameters, dials, host automation entries and sequencer targets. Added one shared ±12 dB distortion-wet OUTPUT with 0 dB default and one drag-to-sequencer target; the final MIX/OUT master OUT is unchanged.
- Replaced Amp-only STEREO/MONO with a global host-automatable selector that operates **before** every distortion model. STEREO leaves the source channels independent; MONO sums L/R before the waveshaper. Amp retains its original pre-distortion Mono processing and channel-copy behaviour. A mono input can still emerge as stereo from models with internal state/spread.
- Shared distortion OUTPUT is applied after the selected model and Hump Filter/Tube chain, ahead of EQ POST/EMPH compensation and COMP; 0 dB takes a no-op path. The upstream HumpDSP and the other model header DSP arithmetic remain unchanged. No preset migration or legacy parameter aliases were added.
- Rebuilt the compact sequencer-target list to exclude obsolete OUTPUT targets, retaining 32 steps and 16 target slots. State schema updated to 3; old development presets/automations/target indexes are intentionally incompatible. Existing `.crusherseq` files save steps/gates only and continue to load as patterns, without migration code.
- README now describes only the current controls and build workflow; historical changes remain here in Changelog. Resources remain excluded from the source package.
- Static model/parameter/knob geometry and source ZIP checks performed. Full Windows JUCE/VST3 build, DAW visuals, listening and actual CPU measurements remain unverified.


## v0.1.34 — Distortion LED and EQ dropdown alignment (2026-11-23)

- Removed the painted DISTORTION panel heading and moved its existing 14 × 14 amber power LED from the upper-right to the upper-left, retaining its 4 px top and 3 px side inset.
- Resized the global EQ mode dropdown to exactly the sequencer dropdown dimensions (90 × 20 px), vertically centred in the EQ header row. FLAT button and EQ graph remain unchanged.
- GUI-only update. No changes to parameters, DSP, per-model OUTPUT controls, Amp STEREO/MONO routing, sequencer controls, effects panels, MIX/OUT or Resources. A shared OUTPUT/STEREO panel is under design consideration, not implemented in this version.
- Static geometry, source-diff and ZIP checks passed; Windows JUCE/VST3 compilation and DAW GUI verification remain unverified.

## v0.1.33 — Tube contextual controls (2026-11-22)

- Added six saved, automatable and sequencer-assignable Tube-only parameters: BIAS, HEAT, SAG, BODY, TONE and OUTPUT plus shared DRIVE, laid out DRIVE/BIAS/HEAT/SAG above BODY/TONE/OUTPUT on the standard 81px grid. Appended targets 115–120; 32 sequencer steps and 16 target slots remain unchanged.
- Added `source/CrusherTube.h` with the original standalone Tube tanh/bias/normaliser/compensation arithmetic retained at neutral defaults, plus optional asymmetric, hotter/softer, envelope sag, pre-body tilt, post-tone tilt and ±12 dB output controls. Replaced old fixed Tube-only `applyInputDrive` code without duplicate legacy processing. The *separate HumpCrusher Tube algorithm* is entirely unchanged.
- Tube DSP and smoothers run only when selected or outgoing during the short model crossfade; no audio-thread allocations. Expensive coefficients and output gain are prepared/cached. No changes to other distortion engines, HumpDSP, global EQ, Filter, COMP, MIX/OUT, or sequencer GUI. Source ZIP excludes Resources.
- Standalone DSP and source checks performed; Windows VST3 build, DAW listening, GUI and real-world CPU tests remain unverified.

## v0.1.32 — Stressed contextual controls (2026-11-22)

- Added six saved, automatable and sequencer-assignable Stressed parameters: PRESS, FOLD, TENSION, BIAS, TONE and OUTPUT. DRIVE remains shared. Standard 81-pixel grid: DRIVE/PRESS/FOLD/TENSION above BIAS/TONE/OUTPUT. New sequencer choices 109–114; 32 steps and 16 slots unchanged.
- Introduced a standalone `source/CrusherStressed.h` processor retaining the original Stressed tanh, sine-wave folding, DC removal and compensation exactly at neutral defaults. Removed the old fixed Stressed DSP/state without adding a legacy path.
- Stressed runs only while active or briefly fading out; inactive smoothers snap to their current controls. No audio-thread allocations, prepared filter coefficients and cached output gain.
- Other distortion engines, HumpDSP, global EQ/filter/compressor/MIX/OUT and sequencer GUI unchanged. Resources excluded; README remains instructional. Windows VST3, DAW listening and actual CPU still require testing.

## v0.1.31 — Soft Clip controls (2026-11-22)

- Added Soft Clip-only THRESH, KNEE, BIAS, BODY, TONE and OUTPUT controls beside shared DRIVE, aligned to the established 81px grid.
- Replaced the fixed Soft Clip dispatch with one independent configurable DSP engine. The original tanh curve and output compensation are unchanged at neutral defaults.
- Added saved/host-automatable parameters and sequencer drag targets 103–108; the sequencer remains at 32 steps and 16 slots.
- Soft Clip runs only while selected or fading out; no realtime allocations. New tonal coefficients and output gains are cached.
- No modifications to the other 17 distortion algorithms, shared EQ/filter/comp/master, or Resources.

## v0.1.30 — Saturation contextual controls (2026-11-22)

- Added Saturation-only CURVE, BIAS, WARM, BODY, TONE and OUTPUT beside shared DRIVE in the standard 81px four-over-three grid: DRIVE/CURVE/BIAS/WARM above BODY/TONE/OUTPUT.
- Replaced only the Saturation model's fixed arctangent dispatch with `source/CrusherSaturation.h`. The original arithmetic is retained at 50% for the five percentage controls and 0 dB OUTPUT, while the new controls vary the waveshaping curve, asymmetry, harmonic density, pre-distortion bass, post tonal tilt and ±12 dB level.
- Six new saved, host-automatable controls are drag-to-sequencer targets 97–102. Saturation DSP and smoothers run only when selected or briefly fading out; no real-time allocations; coefficients and output gain cached.
- Other 17 distortion models, HumpDSP, global EQ, Filter, Comp, MIX/OUT and sequencer layout remain unchanged. README remains instructional; Resources excluded from source ZIP. Windows JUCE VST3 build, DAW sound and CPU measurement not yet verified.

## v0.1.29 — Samurai contextual controls (2026-11-22)

- Added Samurai-only EDGE (pre-drive emphasis), GATE (dead-zone), BLADE (saturation hardness), RING (metallic sine harmonics), TONE (post tonal tilt) and OUTPUT (±12 dB) beside shared DRIVE on the 81px four-top/three-bottom knob grid. All six are saved, host-automatable and assignable to sequencer targets 91–96.
- New standalone `source/CrusherSamurai.h` replaces the fixed Samurai implementation. At EDGE/GATE/BLADE/RING/TONE 50% and OUTPUT 0 dB, the original high-pass emphasis, dead zone, tanh+sine blade, DC block and gain compensation are unchanged. Previous fixed Samurai state and implementation removed, without duplicate DSP or compatibility layers.
- Samurai's DSP and smoothers run only while its model is active or briefly fading out, with no realtime allocations. Coefficients and output gain lookup are prepared/cached. The other 17 distortion engines, HumpDSP, global EQ, Filter, Comp, MIX/OUT and sequencer layout are unchanged. Source ZIP excludes Resources; README remains instructional.
- Standalone sound-preservation, control effect, extreme-setting and source/ZIP validation performed; Windows JUCE VST3 build, DAW listening/GUI and actual CPU measurements remain unverified.

## v0.1.28 — Rectify contextual controls (2026-11-22)

- Added six Rectify-only controls: WAVE (positive/full/negative rectification), BIAS (asymmetric input offset), CURVE (firmer/softer waveshaping), FOLD (post-rectifier waveform folding), TONE (post tonal tilt), and OUTPUT (±12 dB), alongside shared DRIVE. Standard 81px two-row knob grid and corrected upper-left DRIVE placement.
- New `source/CrusherRectify.h` replaces the previous fixed Rectify path, reusing the original full-wave `abs(tanh(...))`, 18 Hz DC blocker and gain compensation exactly at neutral settings. WAVE, BIAS, CURVE and TONE are neutral at 50%; FOLD is neutral at 0%; OUTPUT is neutral at 0 dB. The old state and processor have been removed; no legacy duplicate DSP.
- Six new saved/host-automatable drag-to-sequencer targets 85–90 are appended, preserving the original 32 steps and 16 target slots. Rectify processing and smoothing run only while selected or during a brief outgoing crossfade. No audio-thread memory allocation; expensive output gains are cached and filter coefficients are prepared per sample rate.
- All other distortion DSP, including Hump, Amp, Razor and Pixelated, global EQ, Filter, Comp, MIX/OUT, sequencer GUI, and Resources unchanged. Source ZIP excludes Resources; README contains instructions and Changelog records development history separately.
- Standalone neutral/reference, control-effect, finite-extreme and source/archive checks were performed. Actual Windows JUCE VST3 compilation, DAW GUI/listening and CPU benchmarking remain unverified.

## v0.1.27 — Razor DRIVE knob placement (2026-11-22)

- Corrected the shared DRIVE knob layout condition so Razor places DRIVE in the upper-left grid column, followed by EDGE, FREQ and RING. The lower row BITE, TONE and OUTPUT is unchanged.
- GUI-only change. No DSP, parameter, sequencer, other distortion controls, or Resources changed.
- Source and geometry checks passed; Windows VST3 compilation and DAW visual verification remain outstanding.

## v0.1.26 — Razor contextual controls (2026-11-22)

- Added Razor-only EDGE, FREQ, RING, BITE, TONE and OUTPUT controls alongside shared DRIVE on the 81px four-top/three-bottom knob grid. All six are saved, host-automatable and drag-to-sequencer targets 79–84.
- New `source/CrusherRazor.h` preserves Razor's original pre-emphasis, tanh plus sine-harmonic shaping, DC blocker and output compensation sample-for-sample at neutral settings (50% and OUTPUT 0 dB). The new parameters expand edge emphasis, emphasis cutoff, metallic harmonic strength, clipping bite, post tone and ±12 dB level. The old fixed Razor state and processor code were removed.
- Razor runs only while selected or during an outgoing type-change crossfade. Its smoothers are inactive for other models. Sample-rate coefficient lookup and cached output gain avoid expensive calculations at audio rate; no real-time allocations. The shared EQ, other distortion types, Filter, Comp, MIX/OUT and sequencer layout are untouched.
- Standalone Razor reference, parameter-effect, finite-extreme and sanitizer tests and source/ZIP checks passed. Windows JUCE/VST3 compilation, DAW visuals/listening and measured CPU remain unverified. Resources excluded from the package.

## v0.1.25 — Pixelated JITTER range correction (2026-11-22)

- Reworked only Pixelated JITTER timing so its full range is clearly audible. Rather than weak per-sample rate fluctuations, one independent random hold-duration multiplier is chosen at each capture. Below 50% creates longer stuttering holds; above 50% varies both faster captures and longer holds for more obvious flutter. The midpoint (50%) is a strict no-op, preserving the v0.1.24 Pixelated output sample-for-sample.
- Kept DRIVE and BITS response from v0.1.24, along with all Pixelated parameters, IDs, GUI geometry and sequencer assignments. Random draws happen on capture only; no heap allocations, added processors, or redundant legacy implementation. Non-Pixelated DSP, global EQ, filter, compressor and MIX/OUT unchanged. Resources excluded; Windows VST3/DAW listening and actual CPU measurements remain unverified.

## v0.1.24 — Pixelated DRIVE response (2026-11-22)

- Revoiced Pixelated DRIVE so bit-depth reduction and sample-and-hold degradation become audible much earlier in the knob travel rather than mostly above 70%. The new fourth-root drive curve is precomputed during `prepare()`, not calculated on the audio thread. The wet mix rises earlier while still reaching the original fully dry (0%) and fully processed (100%) endpoints.
- This intentionally changes the sound at intermediate Pixelated DRIVE positions; no legacy mapping or parameter migration code was retained. The BITS v0.1.23 correction and all six Pixelated-only control definitions, IDs, GUI positions and sequencer targets are unchanged.
- No changes to other distortion DSP, shared EQ, filter, compressor, MIX/OUT or sequencer, and no real-time allocations. README remains instructional; Resources excluded. Standalone drive-response/finite tests are performed; Windows VST3 compilation, DAW listening and CPU measurements remain unverified.

## v0.1.23 — Pixelated BITS range correction (2026-11-22)

- Corrected the Pixelated BITS control: below 50%, interpolate smoothly between a coarse three-bit quantiser and the original DRIVE-dependent resolution, retaining a small held-signal contribution at the low end to prevent quiet signals being silenced. Above 50%, progressively reduce quantisation error toward a clean held sample, rather than adding inaudible extra bit depth.
- At exactly 50%, retain the original Pixelated quantisation arithmetic and deterministic dither sequence sample-for-sample; the other five Pixelated controls and all remaining DSP are unchanged.
- Keep existing BITS parameter, GUI, host automation, sequencer drag target, knob position, and active-model-only DSP routing. No compatibility code or audio-thread allocations.
- Source-level comparisons and standalone DSP sweeps performed; Windows VST3 build, listening and in-DAW verification remain outstanding.

## v0.1.22 — Pixelated contextual controls (2026-11-22)

- Added Pixelated-specific RATE (hold rate), BITS (quantisation depth), DITHER (pre-quantisation noise), JITTER (optional hold timing instability), TONE (post-crush tilt) and OUTPUT (±12 dB) alongside shared DRIVE. Controls use the standard 81px four-over-three two-row grid, are saved/host-automatable and drag-to-sequencer assignable as targets 73–78. The 32-step/16-target-slot sequencer framework is unchanged.
- New `source/CrusherPixelated.h` replaces the previous fixed Pixelated processing path and owns its sample-and-hold, deterministic noise, quantisation and state. All five percentage controls at 50% and OUTPUT at 0 dB reproduce the previous Pixelated sound, including original dither RNG state, at neutral settings. No unused legacy engine retained.
- Only selected Pixelated or its outgoing crossfade processes audio; inactive smoothers do not advance per sample; OUTPUT remains effective at DRIVE zero. Precomputed output-gain table and per-rate tone coefficient avoid expensive per-sample calculations; no audio-thread allocations.
- Other distortion models, original HumpDSP, shared EQ, filter, compressor, MIX/OUT, Resources and sequencer layout unchanged. README remains instructional; ZIP excludes Resources. Standalone tests/source checks performed; Windows JUCE VST3 compilation, GUI, listening and CPU benchmarking still need verification.

## v0.1.21 — Lightning contextual controls (2026-11-22)

- Added six Lightning-only controls — SPARK, REACT, CLIP, EDGE, TONE and OUTPUT — beside shared DRIVE on the established 81px, four-upper/three-lower knob grid. All six are host-automatable, preset-saved and assignable to appended sequencer targets 67–72; the 32-step and 16-slot sequencer is untouched.
- Replaced the fixed Lightning implementation with `source/CrusherLightning.h`. Neutral settings (SPARK/REACT/CLIP/EDGE/TONE 50%, OUTPUT 0 dB) reproduce the original hard clipping, deterministic signal-reactive noise, DC blocking and compensation without additional processing. Adjustable spark intensity, input reactivity, clip threshold, edge harmonics, post-tone and independent ±12 dB trim provide variations. The old Lightning state and implementation are removed, not retained alongside the new processor.
- Lightning processing and smoothing run only while selected or briefly fading out during a model change. No realtime allocations; post-tone coefficient is calculated per sample rate and output gain is cached. At DRIVE zero only OUTPUT trim remains.
- No DSP or UI changes to other distortion models, global EQ, Hump filter/compressor or MIX/OUT. README remains instructional, changes recorded only in Changelog.md. Resources excluded from ZIP.
- Standalone neutral-reference, control-effect and finite-output tests, source wiring and ZIP validation performed. Windows JUCE/VST3 compilation, DAW listening/GUI and measured CPU remain unverified.

## v0.1.20 — Hump original-sound-preserving extensions (2026-11-22)

- Retained both original HumpCrusher Mech and Tube waveshapers and their Mech -> original Filter -> Tube signal chain. All six upstream `HumpDSP` files remain byte-for-byte unchanged. Original Hump sound is preserved at neutral extension settings for any MECH/TUBE position (within this Crusher signal path).
- Added five optional Hump controls: BODY (pre-Mech low-frequency emphasis), BIAS (pre-Mech asymmetry), FOLD (optional post-Tube folding), TONE (post-Tube brightness), and OUTPUT (±12 dB). BODY/BIAS/TONE default to 50%, FOLD to 0%, OUTPUT to 0 dB. Neutral extensions are direct arithmetic no-ops.
- MECH/TUBE and the new knobs share the same 81px two-row grid as the other expanded distortion types. All five new Hump parameters are host-automatable, saved and assignable by dragging to new sequencer targets 62–66; existing 32-step/16-slot structure remains unchanged.
- Only active Hump or its outgoing crossfade runs the Hump-specific processing; inactive smoothers are idle. Tone coefficients/gain table are prepared once per sample rate and no audio-thread memory is allocated. Global EQ, Filter/COMP, MIX/OUT, original distortion DSP, sequencer visuals and Resources remain untouched. The source ZIP excludes Resources.
- Standalone Hump neutral/reference and control/finite tests plus static source and ZIP integrity checks performed. Windows JUCE/VST3 compilation, DAW listening/GUI and real CPU measurements remain unverified.

## v0.1.19 — Hard Clip contextual controls (2026-11-22)

- Added six Hard Clip-specific controls — THRESH, BIAS, KNEE, FOLD, TONE and OUTPUT — alongside shared DRIVE on the 81px two-row grid. All are host-automatable, saved and draggable to sequencer targets 56–61. FOLD defaults to 0%; THRESH/BIAS/KNEE/TONE default to 50%, OUTPUT to 0 dB, preserving the original hard-clip sound.
- Replaced the fixed Hard Clip DSP path with `source/CrusherHardClip.h`. Adjustable clip rails, clipping asymmetry, soft/square knee, post-clip waveform folding, tone and ±12 dB trim. No duplicate legacy DSP.
- Processor and smoother work run only while Hard Clip is selected or briefly participating in a model transition. Cached gain lookup avoids per-sample dB-to-linear calculations. OUTPUT is effective even at zero DRIVE. No audio-thread allocations.
- Other distortion algorithms, EQ, Filter, Compressor, MIX/OUT and sequencer visuals/step count/slot count unchanged. README documents usage; Resources excluded from source ZIP.
- Standalone Hard Clip numerical/wiring/package checks performed. Full Windows VST3 compilation, GUI, listening and real DAW CPU benchmarking remain unverified.

## v0.1.18 — Contextual Fuzz controls (2026-11-22)

- Added six Fuzz-specific saved/automatable controls: GATE, BIAS, CLIP, BODY, TONE and OUTPUT, alongside the shared DRIVE on the established 81px two-row grid. All six controls can be dragged to sequencer targets 50–55; the 32-step / 16-slot sequencer architecture is unchanged.
- Replaced the fixed Fuzz path with a single independent `source/CrusherFuzz.h` processor. GATE applies optional low-level gating, BIAS alters the asymmetric offset, CLIP controls thresholds/harshness, BODY shapes low frequencies before distortion, TONE controls post-clip low-pass, and OUTPUT trims ±12 dB. Default settings reproduce the original Fuzz algorithm (including its DC blocker), without legacy duplicate DSP.
- Only selected Fuzz or its outgoing model-switch crossfade processes audio. Inactive Fuzz smoothers do not advance at sample rate; tone coefficients are cached and interpolated and output gain recalculates only when changed. No audio-thread allocations; OUTPUT remains functional with DRIVE at zero.
- All other distortion DSP, global EQ, Filter, Compressor, MIX/OUT, sequencer visuals and Resources remain unchanged. README is for operating/build guidance; changes recorded here. ZIP excludes Resources.
- Standalone model, wiring/grid and archive checks performed; Windows JUCE/VST3 build, DAW visuals, listening and actual CPU benchmark remain unverified.

## v0.1.17 — Contextual Fresh controls (2026-11-22)

- Added six Fresh-specific saved and host-automatable controls: FREQ, EXCITE, AIR, SAT, TONE and OUTPUT, alongside shared DRIVE. All controls use the existing 81px contextual knob grid and the six new parameters are drag-to-sequencer assignable.
- Replaced fixed Fresh exciter with independent `CrusherFresh.h`: variable high-frequency crossover, adjustable high-band saturation and blend, final soft saturation, post-tone and ±12 dB trim. Neutral defaults retain Fresh's original signal character without duplicate legacy DSP.
- Only the selected Fresh mode (or its outgoing crossfade) processes audio; inactive Fresh control smoothers do not advance per sample. No audio-thread allocations. Frequency coefficients use an initialised lookup table rather than transcendental calls at audio rate; output gain recalculates only when changed.
- Other distortion models, global EQ, Filter, Compressor, MIX/OUT, sequencer structure and Resources untouched. README documents operation; detailed changes remain in Changelog.md. Source ZIP excludes Resources.
- Standalone Fresh model checks performed; Windows JUCE/VST3 compilation, listening, GUI and CPU measurements remain unverified.

## v0.1.16 — Contextual Crackle controls (2026-11-20)

- Added Crackle-only DENSITY, BURST, DECAY, GRIT, TONE and OUTPUT, together with the shared DRIVE knob, on the same 81px two-row knob grid as Comb and Bumpy.
- New `CrusherCrackle.h` replaces the previous fixed Crackle processor code, without retaining duplicate or legacy DSP. Neutral default settings reproduce the original random burst, saturation and DC-filtering behaviour.
- Six new saved, host-automatable, drag-to-sequencer targets appended without altering sequencer step or slot counts.
- Crackle DSP and its smoothers are active only while selected or briefly outgoing during crossfade. No audio-thread allocations. At DRIVE=0, OUTPUT trim still works.
- Shared EQ, Filter, Compressor, MIX/OUT, other distortion algorithms and Resources are unchanged. README remains instructional; change history is here.
- Standalone and static checks completed; Windows VST3 build, DAW listening and CPU benchmarks remain unverified.


## v0.1.15 — Comb controls and consistent knob grid (2026-11-20)

- Aligned all Bumpy knobs to the same 81-pixel column grid as Amp and Asym (277, 358, 439, 520), with the existing two vertical row positions. Amp and Asym geometry remains unchanged.
- Added Comb-only TIME, FEEDBACK, DAMP, SPREAD, TONE and OUTPUT controls alongside the shared DRIVE, arranged on that same grid. All six new controls are host-automatable, saved and drag-to-sequencer assignable; existing sequencer steps and slots remain unchanged.
- Added standalone `CrusherComb.h` DSP with variable resonant delay time, loop feedback/damping, stereo spread, post tone and output trim. Fixed-size buffers and cached coefficients avoid audio-thread allocation; fractional tap interpolation reduces modulation stepping.
- At 50% neutral values (OUTPUT 0 dB), Comb reproduces the original comb signal to float rounding in standalone comparison tests. Only selected/outgoing-fade Comb is processed, without legacy compatibility paths.
- All non-Comb DSP, shared EQ, Filter, Comp, MIX/OUT, and sequencer GUI and behaviour are retained. Source ZIP excludes Resources. README documents operation/build; history stays here.
- Full Windows JUCE/VST3 build, DAW listening and real CPU measurement remain unverified.


## v0.1.14 — Bumpy controls and section dimming (2026-11-20)

- Added Bumpy RIPPLE, RATE, CONTOUR, BIAS, TONE and OUTPUT knobs alongside shared DRIVE. Each additional Bumpy parameter is host-automatable, saved and assignable by dragging its label to a sequencer slot.
- Default 50% Bumpy shaping and 0 dB output preserve the original Bumpy sound. Only the selected engine runs, with a short crossfade when changing types. Disabled engine parameters do not trigger per-sample processing.
- EQ, DISTORTION, FILTER and COMP controls, labels and EQ graph dim independently when their amber bypass LED is off. LEDs remain bright and usable, and controls remain editable while off. MIX/OUT and the sequencer do not dim.
- README retains usage and build guidance; source releases still omit Resources. No extra legacy compatibility paths.
- Full Windows VST3 build, DAW visuals, listening and real-world CPU measurements remain to be verified.


## v0.1.13 — Asym controls and logo-anchored zoom menu (2026-11-20)

- Added five Asym-only controls: BIAS, SKEW, SHAPE, TONE and OUTPUT, alongside the original DRIVE. Arranged in two aligned rows, each with three equally spaced positions. All five new parameters are DAW-automatable, saved in presets and draggable to the existing 16-slot sequencer.
- BIAS changes the positive/negative offset; SKEW varies the two half-cycle gains independently; SHAPE blends softer atan saturation, the original tanh shape and a harder clip; TONE adds variable low-pass warmth or high-frequency presence; OUTPUT trims the Asym model from -12 to +12 dB.
- Neutral BIAS=0, SKEW=0, SHAPE=50, TONE=50 and OUTPUT=0 dB retain the original Asym core. Control smoothing mitigates modulation zipper noise. DSP lives in the independently testable `CrusherAsym.h`.
- Anchored the logo zoom context menu to the logo's **bottom-left corner** in screen coordinates, correctly following GUI zoom; it no longer follows the mouse click.
- CPU efficiency pass: only the selected distortion engine is processed, with the outgoing engine running briefly during the existing type crossfade. Inactive Asym no longer updates its per-sample smoothers and control DSP, and Hump waveshapers and Tube stage are skipped unless Hump participates in a transition. Distortion-bypassed samples skip the waveshaper, including inactive Amp/Asym control processing after bypass smoothing completes. Amp cabinet filter coefficients are recalculated only when cabinet or sample rate changes (not on every block or unrelated control movement), and output gain only when its value changes. Shared EQ, FILTER and COMP DSP retain their existing bypass gates; no allocation added to the audio thread.
- No changes to Amp, other distortion DSP, global EQ, Hump filter/compressor, MIX/OUT, sequencer timing/slots, or Resources. README remains instructional; detailed history stays in Changelog.md.

**Validation:** Standalone Asym DSP numerical tests, static parameter/editor/target checks and archive integrity passed. The full Windows JUCE VST3 build, DAW visuals and listening tests remain unverified.


## v0.1.12 — Zoom popup and EQ/Amp header alignment (2026-11-20)

- Anchored the logo zoom popup to the actual screen-space mouse click at the top of the editor, with a downward preference. Previously it was anchored to the full-height content component and appeared near the bottom.
- Moved the STEREO and MONO buttons together 5 px left (x=532 → 527) without altering their y positions, individual sizes or spacing.
- Moved the EQ power LED from (233,66,14,14) to the former left-hand EQ label location (29,70,14,14); positioned the drawn EQ label immediately to its right at (47,69,20,18).
- Moved the EQ mode dropdown 7 px right to (75,67,101,23). Moved FLAT 7 px right to (182,67), preserving its 51 px width while increasing its height from 20 to 23 px to match the dropdown.
- Saved `- Build.bat` as UTF-8 without BOM so the initial `@echo off` works correctly in Windows, preventing the stray text previously observed in `Results.log`.
- GUI-only changes otherwise: no DSP, parameter, sequencer, graph implementation, model, cabinet, other panel or Resources changes.

**Validation:** Focused source, geometry, archive and binary-diff checks passed. Full Windows VST3 build and in-DAW visual verification remain outstanding.

## v0.1.11 — CAB restored to upper row and moved left (2026-11-20)

- GUI-only correction: returned CAB label to y=67 and CAB dropdown to y=87 (their original v0.1.9 heights), undoing the v0.1.10 downward movement.
- Shifted both CAB controls 21 pixels left from x=453 to x=432. The dropdown retains its 150×25 dimensions; the CAB label retains 150×13. They visually span AMP OUTPUT and STEREO/MONO beneath them.
- No other coordinates, controls, presets, parameters, audio processing, sequencer, build dependencies or Resources changed.

**Validation:** Source comparison, CAB geometry and ZIP integrity passed; full Windows VST3 build and in-DAW GUI verification outstanding.

## v0.1.10 — CAB controls directly above OUTPUT and radio group (2026-11-20)

- Amp GUI only: moved CAB label from (453,67) to (453,106) and dropdown from (453,87) to (453,123). Both retain their existing 150-pixel width and respective 13/25-pixel heights, centred over the combined OUTPUT and STEREO/MONO controls.
- The CAB dropdown now sits immediately above the bottom-row controls, rather than alongside the SOFT CLIP control; CAB choices, parameter mapping, control functionality and tooltips are unchanged.
- AMP knobs, the stereo/mono radio group, all three effect panels, EQ, sequencer, DSP and Resources are unchanged. No compatibility scaffolding added.

**Validation:** Source diff, exact control geometry, panel-bounds and ZIP integrity checks passed. Windows JUCE/VST3 compilation and DAW visual checks remain unverified.

## v0.1.9 — Amp CAB/radio alignment and EQ FLAT styling (2026-11-20)

- Amp GUI only: raised the STEREO/MONO group together by 10 px, preserving each radio's 84×20 bounds, x position, 22-pixel spacing, settings and wiring. The group visually centres on OUTPUT's rotary face rather than its value area.
- Aligned the CAB label with SOFT CLIP's label (y=67) and centred its original 150×25 dropdown vertically on the SOFT CLIP rotary knob (y=99.5). CAB choices and parameter remain unchanged.
- Changed the EQ FLAT widget from `TextButton` to a non-toggling `ToggleButton` using the exact existing PHAT drawing path (`standardTextButton`), accent colour and 9pt bold font. FLAT retains its 51×20 bounds, tooltip and reset action; PHAT is unchanged.
- Unchanged: all DSP, parameter IDs, global EQ processing, sequencer, other effect panels and Resources. No compatibility scaffolding.

**Validation:** Focused structural checks, geometry and archive integrity passed. Windows JUCE/VST3 compilation and visual DAW confirmation are unverified.

## v0.1.8 — Amp CAB and STEREO/MONO layout (2026-11-20)

- Removed the redundant MODE heading and its unused JUCE label member. STEREO/MONO remain two mutually-exclusive amber radio LEDs with their existing labels, settings, automation and behaviour.
- Moved STEREO/MONO together to the right of AMP OUTPUT (x=532); kept their original 84×20 size, 22-pixel vertical step and tooltip. Their combined vertical centre (196) aligns with the AMP OUTPUT knob centre (195.5).
- Moved the CAB label and dropdown directly above the lower-right OUTPUT and STEREO/MONO group, horizontally centred over the combined arrangement. CAB still uses the same 16 choices and parameter.
- DRIVE, SOFT CLIP, TOP BOOST, BIAS and OUTPUT knob coordinates, the DISTORTION heading/LED, global EQ, FILTER, COMP, MIX/OUT, all DSP, all parameters, sequencer and state schema are unchanged. Resources excluded from source ZIP.
- README retains operation and build instructions; development history remains exclusively in Changelog.md.

**Validation:** Static source, geometry and archive checks passed. Actual Windows VST3 compilation and DAW visual testing remain unverified.

## v0.1.7 — Amp layout refinement (2026-11-20)

- Left DRIVE and SOFT CLIP in their exact v0.1.6 positions. Moved TOP BOOST and BIAS directly below them, with AMP OUTPUT directly to the right of BIAS; all knob columns use equal 81-pixel spacing, and the lower controls retain the original OUT vertical alignment.
- Moved the stacked STEREO/MONO amber radio indicators and MODE label beside SOFT CLIP; retained the same host parameter and functionality.
- Moved the CAB dropdown and label to the centre of the available right-hand distortion-panel area.
- Widened only the TOP BOOST label to fit the caption and sequencer drag marker without truncation; the knob and drag-to-assign behaviour are unchanged.
- Raised the painted DISTORTION heading to centre vertically on its 14×14 amber LED. All power LED positions and sizes are unchanged.
- No DSP, filter, compressor, EQ, parameters, state schema, sequencer functionality, or Resources changes. README retains operating and build instructions; history remains in this file.

**Validation:** Source and geometry checks; ZIP integrity. Windows VST3 compilation and DAW GUI checks remain unverified.

## v0.1.6 — Global draggable EQ and Amp sequencer controls (2026-11-20)

- Build-only correction (v0.1.6): EQGraph now implements JUCE SettableTooltipClient so its existing `setTooltip()` call compiles. No DSP, parameter, GUI geometry or preset changes.

- Added a single global five-band EQ panel to the left of contextual DISTORTION. Draggable frequency/gain points, scroll-wheel Q, right-click band shapes (Bell, Low/High Shelf, Notch, Low/High Cut), double-click band reset and FLAT-all reset, EQ amber bypass and PRE/EMPH/POST modes. Independent DSP; no Voxengo code or assets.
- PRE emphasises frequencies before distortion; EMPH adds corrective EQ after the selected distortion/filter; POST shapes the output after distortion/filter but before compression. New EQ is initially off/neutral. All band settings support DAW automation and preset storage.
- Reorganized Amp into a compact upper row (DRIVE, SOFT CLIP, TOP BOOST, BIAS) with CAB plus vertically stacked STEREO/MONO LEDs and amp OUTPUT below. MIX/OUT, FILTER, COMP, original 32-step sequencer and Resources are unchanged.
- Added four new assignable sequencer targets: Amp SOFT CLIP, TOP BOOST, BIAS and OUTPUT. DRIVE retains its existing modulation target. Sixteen slots and original sequence machinery remain unchanged.
- Removed Amp BASS, MID, MID FREQ and TREBLE parameters, hidden GUI controls and the obsolete three-band tone-stack DSP. The shared EQ replaces them; no redundant legacy controls are retained. Removed all old-state backfilling/migration code. State schema is **2**, and older Crusher preset and host states are intentionally unsupported.
- Updated version/build/README instructions only. Change history is recorded here, not in README. Source ZIP excludes Resources.

**Validation:** Standalone global EQ DSP tests passed at 44.1, 48, 96 and 192 kHz, including flat/EMPH/shape checks; additional source checks performed. Windows JUCE/VST3 compilation, GUI and listening tests remain unverified.

## v0.1.5 — Amp controls, alignment, tooltips and PHAT (2026-11-20)

- Increased Amp TOP BOOST strength with additional presence after distortion, and increased Amp BIAS asymmetry with post-drive shaping plus DC blocking. Zero values remain exactly neutral; only Amp is affected.
- Aligned every distortion view's top-row controls with MIX and Amp's lower-row controls with OUT, using identical x positions for controls in matching Amp columns; retained the original Filter and MIX/OUT geometry.
- Replaced Amp MODE's visible dropdown with two compact, mutually exclusive amber STEREO/MONO radio LEDs directly below CAB. The original `ampMode` host parameter, existing saved values and automation ID remain unchanged via a hidden attachment.
- Moved PHAT up 9 pixels so its centre is on the horizontal centreline of the OUT knob's rotary circle, retaining its original narrow width and centred COMP alignment.
- Standardised parameter hover help to `NAME | Description` on one line where possible, otherwise at most two lines; labels and controls share help, including the context-dependent DRIVE/DIST label.
- README updated for user operation and build; development changes are recorded only here. All other distortion types, original HumpDSP, filter and compressor algorithms, original sequencer functions, MIX/OUT, and Resources are unchanged.

**Validation:** numerical Amp DSP checks across 44.1–192 kHz, source/geometry and ZIP checks only. Windows JUCE/VST3 build and actual DAW GUI/listening checks remain unverified.

## v0.1.4 — Amp-specific controls and original cabinet models (2026-11-20)

- Build-only correction: closed two missing `std::array` initializer braces in `PluginProcessor.cpp` for Amp parameters. No DSP, parameter IDs, GUI, sequencer or resource changes.
- Expanded Amp (existing distortion ID 6) with CAB, MODE, SOFT, TOP, BIAS, BASS, MID, MID FREQ, TREBLE and LEVEL. Existing DIST parameter now displays DRIVE on Amp, retaining its parameter ID, values and sequencer target.
- Added 11 cabinet names from AIR Amp Sim as feature references and five original Crusher choices (16 total). Cabinet responses are independent filter/resonance approximations, not captured/loaded impulse responses or AIR DSP.
- Added pre-amp bias/treble shaping, post-soft-clipping, three-band EQ, cabinet filtering, Amp level and optional mono sum. Amp-only parameters are saved/host-automatable; they are not sequencer targets yet.
- D.I. cabinet and neutral defaults retain original v0.1.3 Amp behaviour. Missing Amp-only parameters in older saved presets and host states are reset to these neutral defaults. None of the other 17 distortion types, HumpDSP, filter/compressor, sequencer, Mix/Out panel geometry or Resources were modified.
- Arranged Amp-only controls in two rows inside the existing distortion panel and retained all other contextual layouts.
- Reference: AIR/AKAI published Amp Sim control table; no proprietary source or assets incorporated.

**Validation:** Standalone C++ Amp DSP tests and structural/parameter mapping checks. Windows JUCE VST3 compilation, DAW GUI, reference listening and CPU tests unverified.


## v0.1.3 — alphabetical distortion menu and compressor layout (2026-11-20)

- Alphabetised the **on-screen** DIST TYPE menu and made **Amp** its default in new instances. Kept every original parameter index and DSP algorithm mapping unchanged; existing preset/host-state numeric selections remain compatible. The host's parameter choice list retains its previous ordering for compatibility.
- Replaced the default ComboBoxAttachment with a parameter attachment mapping sorted menu item IDs back to the unchanged 0–17 values. Host automation and preset loading continue to select the correct model, including Hump.
- Changed the filter cutoff control caption to **CUT** without changing its parameter ID or filter algorithm. Filter LED position retained.
- Renamed compressor knob caption **COMP** and removed the painted COMP panel heading. Moved the compressor knob to the same vertical position as MIX; moved and centred the reduced PHAT button at the OUT knob's vertical centre. Aligned the Comp LED to exactly the same panel-relative coordinates as the Filter LED.
- Matched the distortion power LED to the Filter and Comp LEDs: all are 14×14 pixels at the same panel-relative top-right inset (3 pixels from the right, 4 pixels below the top).
- Kept the original MIX/OUT controls and panel dimensions intact. No compressor or distortion DSP changes; no modifications to the sequencer or Resources.
- Kept operational and build documentation in README; change history only in Changelog.md.

**Validation:** source diffs, saved-ID mapping assertions, GUI coordinate assertions and ZIP integrity. Actual Windows JUCE/VST3 compilation, DAW UI and listening test still needed.


## v0.1.2 — original MIX/OUT geometry restored (2026-11-20)

- **Restored MIX/OUT exactly to its v0.1.0 panel geometry:** panel `(842, 62, 66, 170)`, MIX dial `(845, 67)`, OUT dial `(845, 154)` and Mix Lock `(891, 66, 14, 14)`. Mix/Out parameters and processing are unchanged.
- Re-sized only FILTER and COMP to **match the original 66-pixel MIX/OUT width**, rather than enlarging MIX/OUT. FILTER has CUTOFF at the original MIX height and RES at the original OUT height; its amber LED remains independently switchable.
- Kept COMP immediately left of MIX/OUT with its heading and amber LED on top, reduced PHAT button to 52 pixels wide, and aligned COMP AMOUNT with OUT.
- Expanded the contextual Distortion panel into the available space, moving only its LED to the new panel's right edge.
- No changes to audio processing, filter DSP, parameter schema, sequencer, preset handling or Resources. All other v0.1.1 changes are retained.

**Validation:** geometry assertions, v0.1.0 MIX/OUT coordinate comparison, v0.1.1 source diff and ZIP integrity checks. Windows VST3 compile and actual visual DAW inspection remain unverified.

## v0.1.1 — contextual distortion and compact panels (2026-11-20)

- Fixed Visual Studio C4458 name-hiding warning by renaming the two local `constrainer` pointers to `editorConstrainer`.
- Added **Hump** as Dist Type 18, retaining the original 17 type indexes and independent model algorithms. Only the selected model now runs in the distortion section; Hump shows MECH/TUBE and the original types show DIST.
- Moved Hump's Mech/Tube operation into the contextual distortion selection, retaining the Mech -> Filter -> Tube order when Hump is selected; the other models run before the independently switchable filter.
- Added an independent amber on/off LED to Distortion. Replaced the existing rectangular Filter and Compressor on/off controls with the same amber sequencer-status LED style.
- Replaced the main effect area with a large contextual Distortion panel followed by three *equally sized* narrow panels: Filter, COMP and MIX/OUT. Filter contains only LED, Cutoff and Res; COMP has LED, PHAT and Amount, with Amount aligned vertically with OUT.
- **Did not modify the HumpCrusher filter algorithm:** `source/HumpDSP/Filter.h` and all other `HumpDSP` headers remain byte-identical to v0.1.0.
- Kept the original sequencer and Resources untouched. Documented the new workflow in README; changelog entries remain only here.

**Validation:** source structural/diff checks and original Hump DSP smoke checks only. Windows JUCE build, plugin GUI, listening tests and preset/automation compatibility with v0.1.0 remain to be verified.

## v0.1.0 — first source integration candidate (2026-11-20)

- Forked the user-supplied Dusk v0.1.1 template to a new independent Crusher project, manufacturer `sl23`, code `cru1`.
- Preserved the 17 template distortion algorithms, dark GUI, 32-step sequencer, gates, 16 target slots, browser, presets and master controls.
- Integrated the supplied AGPL-3.0 HumpCrusher Mech and Tube waveshapers, two-pole resonant low-pass with fat limiter, and stereo-linked compressor with PHAT mode.
- Added on/off switches, Mech/Tube, Filter Cutoff/Resonance, Comp Amount, and five new sequencer modulation targets.
- Retained template Mix/Output/Gate/Power controls with the Hump ±0.9999 processed-output ceiling for safe compressor peaks; new signal order is Mech > Filter > original Dist > Tube > Comp > Mix/Output.
- Reorganised only the main effect-panel areas while retaining the main plugin's header, master and sequencer sections.
- Added logo-click zoom menu (75–300%) and retained GUI scale persistence.
- Changed the portable data directory to `Crusher_Data`, preset extension to `.crusherpreset`, sequence extension to `.crusherseq`, and state root to `CrusherParameters`.
- Included unmodified HumpDSP component headers and the original HumpCrusher AGPL-3.0 license text.
- Removed unused dual-filter placeholder parameters from the fresh Crusher parameter schema.
- Source ZIP does **not** include `Resources`.
- Documentation/package update: consolidated credits, AGPLv3 information and a public-release checklist at the end of `README.md`; added root `LICENSE`; clarified build setup using a temporary `logo.svg` supplied separately. No DSP, GUI, parameter or build-script changes.

**Validation:** source wiring and standalone Hump DSP smoke test only. Actual Visual Studio/JUCE VST3 build, DAW loading, GUI inspection, sonic comparison and CPU benchmarks remain unverified.
