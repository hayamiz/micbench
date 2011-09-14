
SPARSEFILES = 1MB.sparse 100MB.sparse

CLEANFILES = $(SPARSEFILES)

%MB.sparse:
	dd if=/dev/null of=./$@ bs=1M seek=$$(echo $@|sed -e 's/MB\.sparse$$//g')

all: $(SPARSEFILES)
