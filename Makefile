# Everyday commands. The CMake presets do the real work; this file gives them
# short names. Run `make` to list them.

# --- App-specific: everything above the next marker differs between apps. ---

APP := jot
GUI_TARGET := jot-gui
CLI_TARGET := jot
GALLERY_TARGET := jot-gallery
LOG_CATEGORIES := jot.*
# Fixed options, so the same commit always renders the same pictures. The sample vault
# is copied first, since opening a folder makes it a vault. SHOTS is set further down.
SHOT_PREPARE = cp -r tests/data/sample-vault $(SHOTS)/vault
SHOT_APP = --size 1280x840 --vault $(SHOTS)/vault --note "Ashfall/Characters/Mara Vey.md"

FEDORA_DEPS := gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel \
	qt6-qtquickcontrols2-devel tomlplusplus-devel kf6-syntax-highlighting-devel \
	clang-tools-extra entr nodejs npm
DEBIAN_DEPS := build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
	qt6-declarative-dev-tools qml6-module-qtquick-controls qml6-module-qtquick-templates \
	qml6-module-qtquick-effects qml6-module-qtquick-dialogs libtomlplusplus-dev \
	libkf6syntaxhighlighting-dev \
	libqt6sql6-sqlite clang-format entr nodejs npm

# --- Shared by every app in the suite below this line. ---

PRESET ?= dev
BUILD := build/$(PRESET)
GUI := $(BUILD)/src/gui/$(GUI_TARGET)
CLI := $(BUILD)/src/cli/$(CLI_TARGET)
GALLERY := $(BUILD)/src/gallery/$(GALLERY_TARGET)

# Fedora's Qt sends logs to the journal when stderr is not a terminal, and its
# qtlogging.ini turns debug output off. This puts everything back on stderr,
# timestamped, with the app's own categories fully enabled.
DEV_ENV := QT_FORCE_STDERR_LOGGING=1 \
	QT_LOGGING_RULES="qml.debug=true;$(LOG_CATEGORIES)=true" \
	QT_MESSAGE_PATTERN="%{time hh:mm:ss.zzz} %{if-category}%{category} %{endif}%{type}: %{message}"

# Files whose change should rebuild and restart the app under `make watch`. QML is left
# out because --live-qml reloads it in place.
# The themes sit in the core submodule, whose files git ls-files does not list.
WATCHED := { git ls-files --cached --others --exclude-standard -- src CMakeLists.txt ':!:*.qml'; \
	printf '%s\n' core/themes/*.toml; }

# Rebuilds target $(1) and restarts $(2) on every save. Quitting the app or Ctrl-C
# ends the watch; a failed build or a crash waits for the next save instead. entr -d
# exits with status 2 when a file is added, so the loop re-lists and picks it up.
define watch
	@command -v entr >/dev/null || { echo "make $@ needs entr: run make deps" >&2; exit 1; }
	@status=2; while [ $$status -eq 2 ]; do \
		$(WATCHED) | entr -d -r -z -s 'cmake --build --preset $(PRESET) --target $(1) || exec sleep infinity; \
			env $(DEV_ENV) $(2) || { echo "$(1) exited with status $$?, waiting for a change" >&2; exec sleep infinity; }'; \
		status=$$?; \
	done
endef

.DEFAULT_GOAL := help
.PHONY: help deps setup configure build run cli gallery watch watch-gallery screenshots test lint format check clean release release-dry major minor patch

help: ## List commands
	@awk 'BEGIN {FS = ":.*## "} /^[a-z-]+:.*## / {printf "  make %-14s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

deps: ## Install build and development dependencies (uses sudo)
	@if command -v dnf >/dev/null; then sudo dnf install -y $(FEDORA_DEPS); \
	elif command -v apt-get >/dev/null; then sudo apt-get update && sudo apt-get install -y $(DEBIAN_DEPS); \
	else echo "make deps: unsupported distro, see README.md for packages" >&2; exit 1; fi

setup: ## Fetch core, install dev tooling, enable git hooks, and configure
	@git submodule update --init
	@npm ci
	@git config core.hooksPath .githooks
	@cmake --preset $(PRESET)
	@touch $(PRESET_STAMP)

# CMake re-runs itself with the cached settings, so a changed preset only reaches
# an existing build directory through this stamp.
PRESET_STAMP := $(BUILD)/.preset-applied

$(PRESET_STAMP): CMakePresets.json
	@cmake --preset $(PRESET)
	@touch $@

configure: ## Re-run CMake
	@cmake --preset $(PRESET)
	@touch $(PRESET_STAMP)

build: $(PRESET_STAMP) ## Build everything
	@cmake --build --preset $(PRESET)

run: build ## Run the app with logging on; ARGS="..." passes options
	@$(DEV_ENV) $(GUI) $(ARGS)

cli: build ## Run the CLI, e.g. make cli ARGS="--help"
	@$(DEV_ENV) $(CLI) $(ARGS)

gallery: build ## Open the theme gallery; THEME=path shows a theme file live
	@$(DEV_ENV) $(GALLERY) $(if $(THEME),--theme $(THEME))

watch: $(PRESET_STAMP) ## Reload QML on save; rebuild and restart on C++ changes
	$(call watch,$(GUI_TARGET),$(GUI) --live-qml $(ARGS))

watch-gallery: $(PRESET_STAMP) ## Like watch, for the gallery; THEME=path optional
	$(call watch,$(GALLERY_TARGET),$(GALLERY) --live-qml $(if $(THEME),--theme $(THEME)))

SHOTS := $(BUILD)/screenshots

screenshots: build ## Render the app and gallery for every built-in theme
	@rm -rf $(SHOTS) && mkdir -p $(SHOTS)
	@$(if $(SHOT_PREPARE),$(SHOT_PREPARE))
	@for theme in $(basename $(notdir $(wildcard core/themes/*.toml))); do \
		QT_QPA_PLATFORM=offscreen $(GUI) $(SHOT_APP) --theme $$theme \
			--screenshot $(SHOTS)/app-$$theme.png && \
		QT_QPA_PLATFORM=offscreen $(GALLERY) --theme $$theme \
			--screenshot $(SHOTS)/gallery-$$theme.png || exit 1; \
	done
	@echo "Screenshots are in $(SHOTS)"

test: build ## Run the tests
	@ctest --preset $(PRESET)

lint: build ## Lint QML and Markdown
	@cmake --build --preset $(PRESET) --target all_qmllint
	@npx markdownlint-cli2

format: ## Format C++, QML, Markdown, JSON and YAML in place
	@core/tooling/format.sh fix
	@npm run --silent format:md

check: ## Run everything CI checks: format, lint, a -Werror build, and tests
	@core/tooling/format.sh check
	@core/tooling/check-version.sh
	@npx markdownlint-cli2
	@npm run --silent format:md:check
	@cmake --workflow --preset ci

# The level comes as a second goal, as in `make release patch`; VERSION=1.2.3 names one.
LEVEL := $(or $(VERSION),$(firstword $(filter major minor patch,$(MAKECMDGOALS))))

release: ## Release from main: make release patch|minor|major, or VERSION=1.2.3
	@core/tooling/release.sh $(LEVEL)

release-dry: ## Show what make release would do, changing nothing
	@core/tooling/release.sh $(LEVEL) --dry-run

major minor patch:
	@:

clean: ## Remove build output
	@rm -rf build
