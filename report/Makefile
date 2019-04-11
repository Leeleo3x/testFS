PAPER = report
TEX = $(wildcard *.tex sections/*.tex)
FIG = $(wildcard figures/*)
BIB = report.bib
STY = usenix2019_v3.sty

.PHONY: all single clean

all: $(PAPER).pdf

# Build the paper using a single pass of pdflatex, for faster compilation.
# This is for convenience when the bibliography and references do not change.
single:
	pdflatex $(PAPER)

$(PAPER).pdf: $(TEX) $(BIB) $(FIG) $(STY)
	pdflatex $(PAPER)
	bibtex $(PAPER)
	pdflatex $(PAPER)
	pdflatex $(PAPER)

clean:
	$(RM) *.log *.aux \
	*.cfg *.glo *.idx *.toc \
	*.ilg *.ind *.out *.lof \
	*.lot *.bbl *.blg *.gls *.cut *.hd \
	*.dvi *.ps *.thm *.tgz *.zip *.rpi *.gz \
	$(PAPER).pdf
