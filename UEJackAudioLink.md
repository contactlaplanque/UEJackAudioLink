# UEJackAudioLink – Project Brief

## 1. Mission

Create a reusable, production-quality Unreal Engine **Runtime plugin** that embeds a JACK audio client inside the engine/editor process.
While it will ultimately power *akM-control* (a spatial-audio composition tool), the plugin is designed to be **stand-alone and open-source** so any UE project can benefit from JACK connectivity.

### Key capabilities
1. Start/stop a JACK server (or connect to an existing one) on all supported desktop OSes.  
2. Create a JACK client, expose input/output ports, and auto-connect to DAW ports when configured.  
3. Provide real-time peak / RMS level metering per channel.  
4. Offer a clean, engine-agnostic C++ interface, Blueprint nodes, and multicast delegates/events.  
5. Run inside the Unreal **Editor** (not just during Play-In-Editor) so users can view meters or configure routing without launching the game.  
6. Shut down cleanly when the plugin or editor exits.

---

## 2. High-level Architecture

```
┌───────────────────┐
│ UEJackAudioLink   │  <─ Runtime UE module (this repo)
└───────┬───────────┘
        │  C++ API + Blueprint wrappers
        ▼
┌───────────────────┐     Audio thread  ───▶  Peak detector  ───▶  Ring buffer
│   JACK Client C++ │
└───────────────────┘
```

---

## 3. Source-Tree Layout (planned)

```
Plugins/
  UEJackAudioLink/
    UEJackAudioLink.uplugin
    Source/
      UEJackAudioLink/          # Runtime
        Public/
          IJackInterface.h
        Private/
          JackClient.cpp
          JackClient.h
          LevelsRingBuffer.h
          UEJackAudioLinkModule.cpp
      UEJackAudioLinkEditor/    # Editor-only UI (optional)
        Private/
          LevelMeterTab.cpp
Tests/                           # Unit & automation tests (framework TBD)
Build/                           # TeamCity configurations
Docs/
```

---

## 4. Continuous Integration

CI service: **TeamCity** (running on a local/self-hosted agent).

Pipeline outline (details to be filled in later):

1. Checkout repository.  
2. Ensure a UE installation is available on the agent (`UE_PATH` environment variable or similar).  
3. Invoke Unreal Build Tool via `RunUAT BuildPlugin` to compile UEJackAudioLink for at least `Win64 Development`.  
4. Run automated tests (framework & commands TBD).  
5. Publish artefacts: zipped packaged plugin, test reports, build log.

> **NOTE**  Exact unit-test framework (GoogleTest, Catch2, UE Automation, etc.) and result parsers will be chosen as the project evolves.

---

## 5. Testing Strategy (initial thoughts)

* **Pure C++ tests** – for algorithmic pieces (peak detector, ring buffer).  
* **Unreal Automation tests** – for engine-aware behaviour (delegate firing, lifecycle).  
* **Integration tests** – eventually spin up a dummy JACK server inside CI and verify port enumeration.

Framework selection, naming conventions, and coverage goals will be finalised after the first working build compiles under TeamCity.

---

## 6. Coding & Style Guidelines

* PascalCase for public classes (`UEJackAudioLinkModule`, `IJackInterface`).  
* Keep third-party JACK headers isolated to `Private/` or an external folder; never leak them into public headers.  
* Every public symbol must have a concise Doxygen block.  
* clang-format (LLVM style, 4-space indent) enforced via pre-commit hook.  
* Use UE types/macros where appropriate (`FString`, `TUniquePtr`, `UE_LOG`).

---

## 7. Next Immediate Tasks

1. In Unreal Editor → Plugins → **New Plugin → Third-Party** → name it **UEJackAudioLink**.  
2. Commit generated boilerplate; push to repository.  
3. Spin up a local TeamCity project with a single build step that compiles the plugin (no tests yet).  
4. Add an empty `Tests/` folder and placeholder Test target in `.uplugin` to pave the way for unit tests.  
5. Draft `Docs/GettingStarted.md` explaining how to clone, set `UE_PATH`, and run the build.  
6. Sketch an Editor “Level Meter” tab (either Slate or ImGui) that will later subscribe to `FOnJackLevelsUpdated`.

---

## 8. Reference Material

* JACK C API docs – <https://jackaudio.org/api/>  
* Unreal Build System – third-party lib integration – <https://docs.unrealengine.com/>  
* UE Automation Testing – <https://docs.unrealengine.com/TestingAndOptimization/Automation/>
