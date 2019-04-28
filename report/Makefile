PAPER = report
TEX = $(wildcard *.tex sections/*.tex)
FIG = $(wildcard figures/*)
CHARTS = $(wildcard charts/*)
BIB = report.bib
STY = usenix2019_v3.sty

FLAGS = -shell-escape

.PHONY: all single clean

all: $(PAPER).pdf

# Build the paper using a single pass of pdflatex, for faster compilation.
# This is for convenience when the bibliography and references do not change.
single:
	pdflatex $(PAPER)

$(PAPER).pdf: $(TEX) $(BIB) $(FIG) $(STY) $(CHARTS)
	pdflatex $(FLAGS) $(PAPER)
	bibtex $(PAPER)
	pdflatex $(FLAGS) $(PAPER)
	pdflatex $(FLAGS) $(PAPER)

clean:
	$(RM) *.log *.aux \
	*.cfg *.glo *.idx *.toc \
	*.ilg *.ind *.out *.lof \
	*.lot *.bbl *.blg *.gls *.cut *.hd \
	*.dvi *.ps *.thm *.tgz *.zip *.rpi *.gz \
	$(PAPER).pdf
