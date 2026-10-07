# Builds the libc370 manuals.  Needs typst and the bookmaster submodule
# (git submodule update --init docs/books/bookmaster).

TYPST ?= typst
BM    := bookmaster
# The PDF carries the edition: ml01-0004-0.pdf is the first edition.
BOOKS := ml01-0003-0.pdf ml01-0004-0.pdf

.PHONY: all check clean

all: check $(BOOKS)

ml01-0003-%.pdf: ml01-0003.typ $(wildcard guide/*.typ ex/*/*) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

ml01-0004-%.pdf: ml01-0004.typ $(wildcard ref/*.typ ex/*/* syntax/*.txt) $(BM)/bookmaster.typ
	$(TYPST) compile --root . --font-path $(BM)/fonts --ignore-system-fonts $< $@

check:
	@if ls syntax/*.txt >/dev/null 2>&1; then python3 $(BM)/tools/check-syntax.py syntax/*.txt; fi

clean:
	rm -f *.pdf
