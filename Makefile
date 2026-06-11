# Mixmaster 3.0 — convenience targets (MODERNIZED-2026 — see MODERNIZATION.md)
.PHONY: build clean start setup remailer

build:
	./scripts/build-macos.sh

clean:
	./scripts/build-macos.sh clean

setup:
	./scripts/setup-mixdir.sh

start:
	./scripts/start-mixmaster.sh

remailer:
	./scripts/start-remailer.sh
