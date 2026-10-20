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
	qt6-qtquickcontrols2-devel tomlplusplus-devel kf6-syntax-highlighting-devel \
	clang-tools-extra entr nodejs npm
DEBIAN_DEPS := build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
	qt6-declarative-dev-tools qml6-module-qtquick-controls qml6-module-qtquick-templates \
	qml6-module-qtquick-effects qml6-module-qtquick-dialogs libtomlplusplus-dev \
	libkf6syntaxhighlighting-dev \
	libqt6sql6-sqlite clang-format entr nodejs npm

include core/tooling/common.mk
