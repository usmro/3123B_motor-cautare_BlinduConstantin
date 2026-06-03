# ──────────────────────────────────────────────
#  Makefile – Motor de Cautare Documente v2.0
# ──────────────────────────────────────────────
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
MAIN     = motor_cautare
TEST     = teste
SRC_MAIN = motor_cautare.cpp
SRC_TEST = teste_unitare.cpp

.PHONY: all test demo clean

all: $(MAIN)

$(MAIN): $(SRC_MAIN)
	$(CXX) $(CXXFLAGS) -o $@ $<
	@echo "Compilat: ./$(MAIN)"

$(TEST): $(SRC_TEST)
	$(CXX) $(CXXFLAGS) -o $@ $<
	@echo "Compilat: ./$(TEST)"

# Ruleaza testele unitare
test: $(TEST)
	@echo ""
	./$(TEST)

# Creeaza documente exemplu si porneste motorul
demo: $(MAIN)
	@mkdir -p documente_demo
	@printf "Programarea in C++ este un limbaj de programare puternic si eficient.\nC++ suporta paradigme multiple: OOP, generica si procedurala." \
	    > documente_demo/cpp.txt
	@printf "Linux este un sistem de operare open-source bazat pe kernel-ul Linux.\nProgramatorii Linux folosesc adesea C++ si Python pentru aplicatii de sistem." \
	    > documente_demo/linux.txt
	@printf "Motoarele de cautare indexeaza documente pentru regasire rapida.\nUnul din algoritmii de baza este indexul invers care mapeaza cuvinte la documente." \
	    > documente_demo/search.txt
	@printf "Python este popular in domeniul inteligentei artificiale si al datelor.\nBibliotecilepython faciliteaza procesarea textului si analiza datelor." \
	    > documente_demo/python.txt
	./$(MAIN) documente_demo

clean:
	rm -f $(MAIN) $(TEST) search.log test_motor_log.log
	rm -rf documente_demo/
