# Everyday commands. Jot's own values are here; the targets are core's, from
# core/tooling/common.mk. Run `make` to list them.

GUI_TARGET := jot-gui
CLI_TARGET := jot
GALLERY_TARGET := jot-gallery
LOG_CATEGORIES := jot.*
# Fixed options, so the same commit always renders the same pictures. The sample vault
# is copied first, since opening a folder makes it a vault. SHOTS comes from common.mk.
SHOT_PREPARE = cp -r tests/data/sample-vault $(SHOTS)/vault
SHOT_APP = --size 1280x840 --vault $(SHOTS)/vault --note "Ashfall/Characters/Mara Vey.md"

FEDORA_DEPS := gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel \
	qt6-qtquickcontrols2-devel tomlplusplus-devel kf6-syntax-highlighting-devel kf6-sonnet-devel qt6-qtmultimedia-devel qt6-qttools-devel qt6-linguist tinyxml2-devel qtkeychain-qt6-devel git-lfs qt6-qtsvg \
	clang-tools-extra entr nodejs npm
DEBIAN_DEPS := build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
	qt6-declarative-dev-tools qml6-module-qtquick-controls qml6-module-qtquick-templates \
	qml6-module-qtquick-effects qml6-module-qtquick-dialogs libtomlplusplus-dev \
	libkf6syntaxhighlighting-dev libkf6sonnet-dev qt6-multimedia-dev qml6-module-qtmultimedia qt6-tools-dev qt6-l10n-tools libtinyxml2-dev qtkeychain-qt6-dev git-lfs qt6-svg-plugins \
	libqt6sql6-sqlite clang-format entr nodejs npm

include core/tooling/common.mk

translations: ## Read the strings in the code into translations/jot_en.ts again
	@$(GENTLY) cmake --preset dev >/dev/null
	@$(GENTLY) cmake --build --preset dev --target update_translations

site-screenshots: ## Render the website's screenshots into www/screenshots from a release build
	@$(GENTLY) cmake --preset release >/dev/null
	@$(GENTLY) cmake --build --preset release --target $(GUI_TARGET)
	@$(GENTLY) tools/site-screenshots.sh build/release/src/gui/$(GUI_TARGET)
.PHONY: translations site-screenshots
