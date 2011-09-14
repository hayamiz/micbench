
SPARSEFILES = 1MB.sparse 100MB.sparse

CLEANFILES = $(SPARSEFILES)

%MB.sparse: Makefile
	dd if=/dev/null of=./$@ bs=1M seek=$$(echo $@|sed -e 's/MB\.sparse$$//g') count=0

all: $(SPARSEFILES)
