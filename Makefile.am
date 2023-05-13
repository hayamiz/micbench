ACLOCAL_AMFLAGS = $$ACLOCAL_ARGS
SUBDIRS = src test

README: README.md
	cp $< $@

version:
	@echo $(PACKAGE_VERSION)

release: dist
	mkdir -p release
	cp -t release/ $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz
	git add -A release/
	git commit -m "Release $(PACKAGE_VERSION)"
	git tag -a v$(PACKAGE_VERSION) -m "Release $(PACKAGE_VERSION)"

.PHONY: version release