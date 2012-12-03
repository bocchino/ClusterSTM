TARGET = algorithm

# Targets
# 
default: pdf

pdf: $(TARGET).pdf

ps:  $(TARGET).ps

dvi: $(TARGET).dvi

html: $(TARGET).html

.PRECIOUS: $(TARGET).dvi $(TARGET).ps $(TARGET).pdf

clean:
	rm -f *.aux *.bbl *.dvi *.lof *.log *.toc *.lot *.blg *~

veryclean:
	rm -f *.aux *.bbl *.dvi *.lof *.log *.toc *.lot *.blg *.dvi *.ps *.pdf *.html *~


# Programs and paths
# This makefile assumes latex, bibtex, and pdflatex is in your path
# 
BIBTEX   = bibtex
LATEX    = latex
PDFLATEX = pdflatex
DVIPS    = dvips -tletter
PS2PDF   = ps2pdf
L2H      = latex2html -white -antialias -local_icons -long_titles 1 \
              -show_section_numbers
POPTS     = --shell-escape

# Source files
# 
TEXFILES = ${wildcard *.tex}
BIBFILES = ${wildcard *.bib}
FIGFILES = ${wildcard *.eps}
LSTFILES = ${wildcard *.lst}

# Commands
# 
%.dvi: %.tex $(TEXFILES) $(FIGFILES) $(LSTFILES) $(BIBFILES)
	$(RM) -f *.aux
	$(LATEX) $*.tex
#	$(BIBTEX) $(TARGET)
#
	$(LATEX) $*.tex
	$(LATEX) $*.tex
	@/bin/echo ""
	@/bin/echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	@/bin/echo "               ++++ UNDEFINED REFERENCES ++++"
	-@grep -i undef $(TARGET).log
	@/bin/echo "                 ++++ EMPTY REFERENCES ++++"
	-@grep -i -n 'cite{ *}' $(TEXFILES) $(FIGFILES)
	-@grep -i -n 'ref{ *}' $(TEXFILES) $(FIGFILES)
	@/bin/echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	@/bin/echo ""

%.ps: %.dvi
	$(DVIPS) -o $@ $<

%.pdf: %.ps
	$(PS2PDF) $<

%.html: %.tex %.dvi
	${L2H} $*

