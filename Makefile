-include local.mk
VC ?= vc +aos68k
CFLAGS ?= -O
CPPFLAGS = -ISDK -Iinclude
CORE = src/diagnostic_report.c src/rexx_bridge.c src/rexx_commands.c src/sample.c src/browser_data.c src/config.c src/info.c src/log.c src/log_view.c src/network.c src/connectivity.c src/preferences.c src/core.c src/connection.c SDK/amihass_client.c src/backend.c src/ui.c src/panel.c src/wait.c src/quote.c
HEADERS = $(wildcard SDK/*.h include/*.h)
all: AmiHASSExample AmiHASSPrefs AmiHASSService AmiHASSBrowser AmiHASSDashboard StartInstaller SetupStartup
AmiHASSPrefs: src/main.c $(CORE) $(HEADERS)
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ src/main.c $(CORE) -lamiga
AmiHASSService: src/main.c $(CORE) $(HEADERS)
	$(VC) $(CFLAGS) $(CPPFLAGS) -DAMIHASS_SERVICE -o $@ src/main.c $(CORE) -lamiga
AmiHASSBrowser: src/browser.c src/browser_live.c src/browser_data.c src/panel.c src/core.c SDK/amihass_client.c SDK/quote.c $(HEADERS)
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ src/browser.c src/browser_live.c src/browser_data.c src/panel.c src/core.c SDK/amihass_client.c SDK/quote.c -lamiga
AmiHASSDashboard: src/dashboard.c src/dashboard_layout.c src/dashboard_picker.c src/dashboard_art.c src/browser_live.c $(CORE) $(HEADERS)
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ src/dashboard.c src/dashboard_layout.c src/dashboard_picker.c src/dashboard_art.c src/browser_live.c $(CORE) -lamiga
StartInstaller: tools/startinstaller.c include/nd_version.h
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ tools/startinstaller.c -lamiga
SetupStartup: tools/setupstartup.c src/config.c src/core.c $(HEADERS)
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ tools/setupstartup.c src/config.c src/core.c -lamiga
check:
	python3 tests/check.py
stage: all
	python3 tools/stage.py
.PHONY: all check stage

example: AmiHASSExample
AmiHASSExample: SDK/example.c SDK/amihass_client.c SDK/quote.c $(HEADERS)
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ SDK/example.c SDK/amihass_client.c SDK/quote.c -lamiga

qa: build/qa/AmiHASSConnectionCheck
build/qa/AmiHASSConnectionCheck: tests/native_connection_check.c $(CORE) $(HEADERS)
	mkdir -p build/qa
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/native_connection_check.c $(CORE) -lamiga
.PHONY: qa

qa: all
	python3 tools/stage.py build/qa
	python3 tools/prepare_installer_qa.py

RELEASE_OUTPUT ?= outputs/release-0.1
RELEASE_CONFIG ?= examples/release-config.json
release:
	python3 tools/release.py build --config $(RELEASE_CONFIG) --output $(RELEASE_OUTPUT)

release-plan:
	python3 tools/release.py plan --config $(RELEASE_CONFIG) --output $(RELEASE_OUTPUT)

release-verify:
	python3 tools/release.py check --config $(RELEASE_CONFIG) --output $(RELEASE_OUTPUT)

release-prepare:
	python3 tools/release.py prepare --config $(RELEASE_CONFIG) --output $(RELEASE_OUTPUT)

package-project:
	python3 tools/package-project.py

package-zip: all
	python3 tools/package.py --output build/releases
.PHONY: release-plan release-verify release-prepare package-project package-zip
.PHONY: release

check-release: all
	python3 tests/test_release.py
.PHONY: check-release

# Optional cross-project integration QA; production packages do not require NovaDock.
NOVADOCK ?= ../NovaDock
qa-novadock: qa
	$(MAKE) -C $(NOVADOCK) -f Makefile.cross Plugins/HASS VC='$(VC)'
	$(VC) $(CFLAGS) -I$(NOVADOCK)/plugin_src/hass -I$(NOVADOCK)/include -o build/qa/AmiHASSNovaDockCheck tests/native_novadock_check.c $(NOVADOCK)/plugin_src/hass/nd_hass.c $(NOVADOCK)/plugin_src/hass/nd_hass_client.c src/quote.c -lamiga
	cp $(NOVADOCK)/Plugins/HASS build/qa/NovaDockHASS
.PHONY: qa-novadock

release-readiness:
	python3 tools/release.py readiness --config $(RELEASE_CONFIG) --output $(RELEASE_OUTPUT)
.PHONY: release-readiness

qa: build/qa/AmiHASSInstallerWelcome
build/qa/AmiHASSInstallerWelcome: tests/native_installer_welcome.c
	mkdir -p build/qa
	$(VC) $(CFLAGS) $(CPPFLAGS) -o $@ tests/native_installer_welcome.c -lamiga
