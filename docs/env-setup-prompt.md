# Environment Setup Prompt

> Use this prompt with an AI CLI agent to configure or restore the SuiteSpot development environment.

---

Configure this development environment to maximize the capability surface available to AI CLI agents for iterative, autonomous feature development on SuiteSpot — a BakkesMod plugin (Windows x64 DLL, C++20, Visual Studio 2022/MSBuild) with source editing on WSL2 Ubuntu 24.04 and builds on Windows. The goal is not just to understand what exists, but to make it trivially easy to bring in new libraries, integrate external services, port features from other ecosystems, and extend the plugin's capabilities without friction at each step.

**ENVIRONMENT SETUP PRIORITIES:**
1. Dependency pipeline: A fully operational, agent-friendly package management and library integration pathway so that adding a new C++ library, porting an algorithm, or pulling in an external API client is a single step — not a multi-session configuration effort.
2. Capability expansion scaffolding: Build system configuration, include paths, link targets, and ABI compatibility groundwork laid for the categories of features most likely to be added: networking, persistent storage, audio, UI extensions, interprocess communication, and external API integrations.
3. AI agent autonomy: The environment should allow an AI agent to identify a missing capability, locate or evaluate a suitable library, integrate it, and use it — all within a single session and without human intervention for toolchain issues.

**SPECIFIC REQUIREMENTS:**
- vcpkg fully integrated with the MSBuild project in manifest mode (`vcpkg.json`) so agents can add any package via a single file edit and rebuild — no manual CMake or linker configuration per library. Pre-populate with libraries covering the likely expansion surface: `curl` or `cpp-httplib` (HTTP/REST), `sqlite3` (persistent storage beyond JSON), `spdlog` (structured logging), `fmt` (string formatting), `nlohmann-json` (already vendored — migrate to vcpkg-managed), `openssl` (HTTPS), and `websocketpp` or `ixwebsocket` (real-time data feeds)
- `compile_commands.json` generated and kept current on the Linux side so clangd can resolve symbols for any newly added vcpkg dependency immediately, without a Windows build cycle to confirm include paths are correct
- BakkesMod SDK headers fully indexed on WSL2 with the hook system, CVar API, GameWrapper, notifier events, and plugin lifecycle documented in an agent-readable format — so agents know exactly which SDK surface to target when implementing new hooks or extending existing ones
- A vetted, project-appropriate `.clang-format` and `.clang-tidy` configuration committed — not to police existing code, but to ensure newly generated or ported code integrates cleanly without style drift that would confuse future agents
- Doxygen or equivalent configured to run on demand, producing browsable output that covers both the plugin's own Hub-and-spoke architecture and any newly integrated third-party headers — so agents can navigate unfamiliar library APIs without leaving the repo context
- A test scaffold targeting the components fully decoupled from the game runtime (`TrainingPackManager`, `SettingsSync`, `PackUsageTracker`, `MapManager`) — not as a quality gate, but so agents can prototype, validate, and iterate on new feature logic before it touches live BakkesMod hooks
- Pre-commit hooks lightweight enough not to block rapid iteration: format-on-commit via clang-format, nothing that requires a full build or game runtime
- A `CHANGELOG.md` and structured branch/commit convention so agents can reconstruct intent, understand what was attempted in prior sessions, and avoid re-implementing or conflicting with in-progress work

