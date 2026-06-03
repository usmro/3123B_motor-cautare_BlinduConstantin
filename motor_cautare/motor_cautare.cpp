/**
 * Motor de Cautare Documente Text
 * ─────────────────────────────────
 * Compilare: g++ -std=c++17 -Wall -O2 -o motor_cautare motor_cautare.cpp
 * Rulare   : ./motor_cautare <director>
 *
 * Cerinte implementate:
 *   - Design Pattern Observer  : Document (Observable) + Logger (Observer)
 *   - Cautare avansata         : AND / OR intre cuvinte
 *   - Eliminare stop-words     : lista configurabila de cuvinte ignorate
 *   - Indexare + REPL interactiv
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <cctype>

namespace fs = std::filesystem;

// ════════════════════════════════════════════════════════════════
//  SECTIUNEA 1 – UTILITARE
// ════════════════════════════════════════════════════════════════
namespace util {

    char toLower(char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string toLowerCase(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), toLower);
        return r;
    }

    // Retine doar litere/cifre si converteste la lowercase
    std::string normalizeazaCuvant(const std::string& cuvant) {
        std::string r;
        r.reserve(cuvant.size());
        for (char c : cuvant)
            if (std::isalpha(static_cast<unsigned char>(c)) ||
                std::isdigit(static_cast<unsigned char>(c)))
                r += toLower(c);
        return r;
    }

    // Tokenizare fara filtrare stop-words
    std::vector<std::string> tokenizeaza(const std::string& text) {
        std::vector<std::string> tokeni;
        std::istringstream flux(text);
        std::string cuv;
        while (flux >> cuv) {
            std::string norm = normalizeazaCuvant(cuv);
            if (!norm.empty()) tokeni.push_back(norm);
        }
        return tokeni;
    }

    // Timestamp lizibil pentru log
    std::string timestamp() {
        auto now  = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        return buf;
    }

    // Intersectie a doua seturi
    std::set<std::string> intersectie(const std::set<std::string>& a,
                                      const std::set<std::string>& b) {
        std::set<std::string> r;
        for (const auto& x : a)
            if (b.count(x)) r.insert(x);
        return r;
    }

    // Reuniune a doua seturi
    std::set<std::string> reuniune(const std::set<std::string>& a,
                                   const std::set<std::string>& b) {
        std::set<std::string> r = a;
        r.insert(b.begin(), b.end());
        return r;
    }

} // namespace util


// ════════════════════════════════════════════════════════════════
//  SECTIUNEA 2 – STOP-WORDS
// ════════════════════════════════════════════════════════════════

/**
 * Lista de stop-words in romana si engleza.
 * Cuvintele din aceasta lista sunt excluse din index.
 */
class StopWords {
public:
    std::unordered_set<std::string> lista;

    StopWords() {
        // Romana
        for (const char* sw : {
            "si","in","la","pe","cu","de","un","o","a","al","ale",
            "cel","cea","cei","cele","din","spre","prin","pentru",
            "care","ce","sa","se","nu","dar","sau","ori","ca","fi",
            "este","sunt","era","au","am","ai","ar","fi","fost",
            "acest","aceasta","acesta","acestia","acestea",
            "acel","aceea","acei","acele","eu","tu","el","ea",
            "noi","voi","ei","ele","ma","te","il","le","ne","va",
            "le","imi","iti","ii","ne","vi","le","li","isi",
            "mai","tot","toate","toti","toata","foarte","acum",
            "atunci","dupa","inainte","intre","asupra","despre",
            "daca","cat","cand","unde","cum","astfel","deci",
            "iar","insa","totusi","chiar","doar","poate","fie"
        }) lista.insert(sw);

        // Engleza
        for (const char* sw : {
            "the","a","an","and","or","but","in","on","at","to",
            "for","of","with","by","from","is","are","was","were",
            "be","been","being","have","has","had","do","does","did",
            "will","would","shall","should","may","might","must",
            "can","could","not","no","nor","so","yet","both",
            "either","neither","each","every","all","any","few",
            "more","most","other","some","such","than","that","this",
            "these","those","i","you","he","she","it","we","they",
            "what","which","who","whom","whose","when","where","why","how"
        }) lista.insert(sw);
    }

    bool esteStopWord(const std::string& cuv) const {
        return lista.count(cuv) > 0;
    }

    // Filtrare vector de tokeni
    std::vector<std::string> filtreaza(
            const std::vector<std::string>& tokeni) const {
        std::vector<std::string> r;
        r.reserve(tokeni.size());
        for (const auto& t : tokeni)
            if (!esteStopWord(t)) r.push_back(t);
        return r;
    }
};


// ════════════════════════════════════════════════════════════════
//  SECTIUNEA 3 – DESIGN PATTERN OBSERVER
// ════════════════════════════════════════════════════════════════

/**
 * Tipul evenimentului emis de Observable.
 */
struct EventCautare {
    std::string interogare;        // textul cautat
    std::string tipOperatie;       // "AND" | "OR" | "SIMPLU"
    std::vector<std::string> rezultate; // fisierele gasite
    std::string timestamp;
};

/**
 * Interfata Observer – orice clasa care vrea sa fie notificata
 * la o cautare trebuie sa implementeze onCautare().
 */
class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void onCautare(const EventCautare& ev) = 0;
};

/**
 * Interfata Observable – Document (si Index) o implementeaza
 * pentru a gestiona lista de observatori.
 */
class IObservable {
public:
    virtual ~IObservable() = default;
    virtual void adaugaObserver(std::shared_ptr<IObserver> obs) = 0;
    virtual void eliminaObserver(std::shared_ptr<IObserver> obs) = 0;
protected:
    virtual void notificaObserveri(const EventCautare& ev) = 0;
};

/**
 * Logger – Observer concret.
 * Inregistreaza fiecare cautare in fisierul search.log
 * si optional pe stdout.
 */
class Logger : public IObserver {
public:
    std::string caleFisierLog;
    bool afisareConsola;

    explicit Logger(const std::string& caleFisier = "search.log",
                    bool consola = false)
        : caleFisierLog(caleFisier), afisareConsola(consola) {}

    void onCautare(const EventCautare& ev) override {
        std::ostringstream linie;
        linie << "[" << ev.timestamp << "] "
              << "TIP=" << ev.tipOperatie << " "
              << "QUERY=\"" << ev.interogare << "\" "
              << "REZULTATE=" << ev.rezultate.size();
        if (!ev.rezultate.empty()) {
            linie << " (";
            for (size_t i = 0; i < ev.rezultate.size(); ++i) {
                if (i) linie << ", ";
                linie << ev.rezultate[i];
            }
            linie << ")";
        }
        linie << "\n";

        // Scriere in fisier (append)
        std::ofstream log(caleFisierLog, std::ios::app);
        if (log.is_open()) log << linie.str();

        if (afisareConsola)
            std::cout << "[LOG] " << linie.str();
    }

    // Returneaza toate inregistrarile din fisierul de log
    std::vector<std::string> citesteLogs() const {
        std::vector<std::string> logs;
        std::ifstream f(caleFisierLog);
        std::string l;
        while (std::getline(f, l))
            if (!l.empty()) logs.push_back(l);
        return logs;
    }
};


// ════════════════════════════════════════════════════════════════
//  SECTIUNEA 4 – CLASA DOCUMENT  (Observable)
// ════════════════════════════════════════════════════════════════
class Document : public IObservable {
public:
    std::string caleFisier;
    std::string continut;

private:
    std::vector<std::shared_ptr<IObserver>> observeri;

public:
    Document() = default;
    explicit Document(const std::string& cale) : caleFisier(cale) {}

    // ── IObservable ──────────────────────────────────────────────
    void adaugaObserver(std::shared_ptr<IObserver> obs) override {
        observeri.push_back(obs);
    }

    void eliminaObserver(std::shared_ptr<IObserver> obs) override {
        observeri.erase(
            std::remove(observeri.begin(), observeri.end(), obs),
            observeri.end());
    }

    // Emite eveniment catre toti observerii
    void notificaObserveri(const EventCautare& ev) override {
        for (auto& o : observeri) o->onCautare(ev);
    }

    // Emite public (apelat din Index dupa cautare)
    void emiteEvent(const EventCautare& ev) { notificaObserveri(ev); }

    // ── Incarcare fisier ─────────────────────────────────────────
    bool incarca() {
        std::ifstream fisier(caleFisier);
        if (!fisier.is_open()) {
            std::cerr << "[EROARE] Nu se poate deschide: "
                      << caleFisier << "\n";
            return false;
        }
        std::ostringstream buf;
        buf << fisier.rdbuf();
        continut = buf.str();
        return true;
    }

    std::string numeFisier() const {
        return fs::path(caleFisier).filename().string();
    }
};


// ════════════════════════════════════════════════════════════════
//  SECTIUNEA 5 – CLASA INDEX
// ════════════════════════════════════════════════════════════════

/**
 * Tip de operatie pentru cautare avansata.
 */
enum class OperatieQuery { SIMPLU, AND, OR };

class Index : public IObservable {
public:
    // cuvant -> set de nume de fisiere
    std::map<std::string, std::set<std::string>> indexInvers;

    // Documentele incarcate
    std::vector<Document> documente;

    // Stop-words
    StopWords stopWords;

private:
    std::vector<std::shared_ptr<IObserver>> observeri;

    // ── IObservable ──────────────────────────────────────────────
public:
    void adaugaObserver(std::shared_ptr<IObserver> obs) override {
        observeri.push_back(obs);
        // Propagam si la fiecare document
        for (auto& doc : documente) doc.adaugaObserver(obs);
    }

    void eliminaObserver(std::shared_ptr<IObserver> obs) override {
        observeri.erase(
            std::remove(observeri.begin(), observeri.end(), obs),
            observeri.end());
    }

protected:
    void notificaObserveri(const EventCautare& ev) override {
        for (auto& o : observeri) o->onCautare(ev);
    }

    // ── Incarcare ────────────────────────────────────────────────
public:
    bool incarcaDocumente(const std::string& caleDirector) {
        if (!fs::exists(caleDirector) || !fs::is_directory(caleDirector)) {
            std::cerr << "[EROARE] Directorul nu exista: "
                      << caleDirector << "\n";
            return false;
        }
        int n = 0;
        for (const auto& entry : fs::directory_iterator(caleDirector)) {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".txt") {
                Document doc(entry.path().string());
                if (doc.incarca()) {
                    // Atasam observerii deja inregistrati
                    for (auto& o : observeri) doc.adaugaObserver(o);
                    documente.push_back(std::move(doc));
                    ++n;
                }
            }
        }
        if (n == 0) {
            std::cerr << "[ATENTIE] Niciun fisier .txt gasit in: "
                      << caleDirector << "\n";
            return false;
        }
        std::cout << "[INFO] " << n << " document(e) incarcate.\n";
        return true;
    }

    // ── Construire index cu filtrare stop-words ──────────────────
    void construiesteIndex() {
        indexInvers.clear();
        for (auto& doc : documente) {
            auto tokeni  = util::tokenizeaza(doc.continut);
            auto filtrati = stopWords.filtreaza(tokeni);
            for (const auto& t : filtrati)
                indexInvers[t].insert(doc.numeFisier());
        }
        std::cout << "[INFO] Index construit cu "
                  << indexInvers.size() << " cuvinte unice"
                  << " (stop-words excluse).\n";
    }

    // ── Cautare simpla (un singur cuvant) ────────────────────────
    std::set<std::string> cautaSimpu(const std::string& cuvant) const {
        std::string cheie = util::normalizeazaCuvant(cuvant);
        auto it = indexInvers.find(cheie);
        return (it != indexInvers.end()) ? it->second : std::set<std::string>{};
    }

    // ── Cautare AND (intersectie) ────────────────────────────────
    std::set<std::string> cautaAND(const std::vector<std::string>& cuvinte) const {
        if (cuvinte.empty()) return {};
        std::set<std::string> r = cautaSimpu(cuvinte[0]);
        for (size_t i = 1; i < cuvinte.size(); ++i)
            r = util::intersectie(r, cautaSimpu(cuvinte[i]));
        return r;
    }

    // ── Cautare OR (reuniune) ────────────────────────────────────
    std::set<std::string> cautaOR(const std::vector<std::string>& cuvinte) const {
        std::set<std::string> r;
        for (const auto& c : cuvinte)
            r = util::reuniune(r, cautaSimpu(c));
        return r;
    }

    // ── Punctul central de cautare – notifica observerii ─────────
    std::set<std::string> cauta(const std::string& interogare,
                                OperatieQuery op = OperatieQuery::SIMPLU) {
        // Tokenizare + filtrare stop-words din interogare
        auto tokeni  = util::tokenizeaza(interogare);
        auto filtrati = stopWords.filtreaza(tokeni);

        std::set<std::string> rezultate;
        std::string tipStr;

        // Cuvintele efective dupa filtrare stop-words (sau brute daca toate erau stop)
        const auto& cuvinte = filtrati.empty() ? tokeni : filtrati;

        if (op == OperatieQuery::AND) {
            // Intersectie: documentele care contin TOATE cuvintele cerute
            rezultate = cautaAND(cuvinte);
            tipStr = "AND";
        } else if (op == OperatieQuery::OR) {
            // Reuniune: documentele care contin CEL PUTIN UN cuvant
            rezultate = cautaOR(cuvinte);
            tipStr = "OR";
        } else {
            // SIMPLU: cauta primul (si singurul) cuvant util
            if (!cuvinte.empty())
                rezultate = cautaSimpu(cuvinte[0]);
            tipStr = "SIMPLU";
        }

        // Construim evenimentul si notificam observerii
        EventCautare ev;
        ev.interogare  = interogare;
        ev.tipOperatie = tipStr;
        ev.rezultate   = std::vector<std::string>(rezultate.begin(),
                                                   rezultate.end());
        ev.timestamp   = util::timestamp();
        notificaObserveri(ev);

        return rezultate;
    }

    // Supraincarcari convenabile
    std::set<std::string> cautaAND(const std::string& interogare) {
        return cauta(interogare, OperatieQuery::AND);
    }

    std::set<std::string> cautaOR(const std::string& interogare) {
        return cauta(interogare, OperatieQuery::OR);
    }

    // ── Statistici ───────────────────────────────────────────────
    void afiseazaStatistici() const {
        std::cout << "\n===== STATISTICI INDEX =====\n";
        std::cout << "Documente indexate : " << documente.size() << "\n";
        std::cout << "Cuvinte unice      : " << indexInvers.size() << "\n";
        std::cout << "Stop-words         : " << stopWords.lista.size() << "\n";

        std::vector<std::pair<size_t, std::string>> frq;
        for (const auto& [c, d] : indexInvers) frq.emplace_back(d.size(), c);
        std::sort(frq.rbegin(), frq.rend());

        std::cout << "\nTop 5 cuvinte (nr. documente):\n";
        for (size_t i = 0; i < std::min<size_t>(5, frq.size()); ++i)
            std::cout << "  " << frq[i].second
                      << " -> " << frq[i].first << " doc(e)\n";
        std::cout << "============================\n\n";
    }
};


// ════════════════════════════════════════════════════════════════
//  SECTIUNEA 6 – REPL INTERACTIV
// ════════════════════════════════════════════════════════════════

// ── Coduri ANSI pentru culori ────────────────────────────────────
namespace culori {
    constexpr const char* RESET    = "\033[0m";
    constexpr const char* ROSU     = "\033[1;31m";
    constexpr const char* GALBEN   = "\033[1;33m";
    constexpr const char* ALBASTRU = "\033[1;34m";
}

// ── Helpers pentru meniu ──────────────────────────────────────────

// Rand interior cu latime fixa (55 caractere vizibile intre ║ ║)
// Exemplu: randul("  text  ")  →  "  ║  text                           ║"
// Afiseaza un rand interior centrat pe W=55 caractere vizibile.
// viz_w = latimea vizuala reala a sirului (nr. de coloane terminal),
// necesara cand sirul contine caractere UTF-8 multi-byte.
static void rand(const std::string& continut, int viz_w = -1) {
    const int W = 55;
    int viz = (viz_w >= 0) ? viz_w : static_cast<int>(continut.size());
    int pad_stanga  = (W - viz) / 2;
    int pad_dreapta =  W - viz  - pad_stanga;
    std::cout << "  ║";
    for (int i = 0; i < pad_stanga;  ++i) std::cout << ' ';
    std::cout << continut;
    for (int i = 0; i < pad_dreapta; ++i) std::cout << ' ';
    std::cout << "║\n";
}

// Rand cu comanda colorata + descriere, aliniate simetric
//   ex: randComanda("[1]", ALBASTRU, "cauta  <cuvant>", "cautare simpla")
static void randComanda(const std::string& nr,
                         const char* culoare,
                         const std::string& cmd,
                         const std::string& desc) {
    // Coloana stanga (nr + cmd): 28 caractere vizibile
    // Coloana dreapta (desc)   : 25 caractere vizibile
    // Total interior           : 2 + 28 + 2 + 25 = 55 + spatii = 55
    const int COL_STANGA = 28;
    const int W          = 55;

    std::string stanga = nr + "  " + culoare + cmd + culori::RESET;
    int viz_stanga = static_cast<int>(nr.size()) + 2 + static_cast<int>(cmd.size());

    std::cout << "  ║  " << stanga;
    for (int i = viz_stanga; i < COL_STANGA; ++i) std::cout << ' ';
    std::cout << "▸ " << desc;
    int viz_total = COL_STANGA + 2 + static_cast<int>(desc.size());
    for (int i = viz_total; i < W; ++i) std::cout << ' ';
    std::cout << "║\n";
}

static void afiseazaMeniu() {

    // Caractere de chenar
    const std::string TOP = "  ╔═══════════════════════════════════════════════════════╗";
    const std::string SEP = "  ╠═══════════════════════════════════════════════════════╣";
    const std::string BOT = "  ╚═══════════════════════════════════════════════════════╝";
    const std::string GOL = "  ║                                                       ║";

    std::cout << "\n";
    std::cout << TOP << "\n";
    std::cout << GOL << "\n";
    rand("░▒▓  MOTOR DE CAUTARE DOCUMENTE  ▓▒░  v2.0", 42);
    std::cout << GOL << "\n";
    std::cout << SEP << "\n";
    std::cout << GOL << "\n";
    randComanda("[1]", culori::ALBASTRU, "cauta  <cuvant>      ", "cautare simpla    ");
    randComanda("[2]", culori::ALBASTRU, "and    <c1> <c2> ... ", "toate cuvintele   ");
    randComanda("[3]", culori::ALBASTRU, "or     <c1> <c2> ... ", "cel putin unul    ");
    std::cout << GOL << "\n";
    randComanda("[4]", culori::GALBEN,   "stat                 ", "statistici index  ");
    randComanda("[5]", culori::GALBEN,   "log                  ", "istoric cautari   ");
    std::cout << GOL << "\n";
    randComanda("[0]", culori::ROSU,     "exit                 ", "iesire            ");
    std::cout << GOL << "\n";
    std::cout << BOT << "\n";
    std::cout << "\n";
}

static void afiseazaRezultate(const std::string& interogare,
                               const std::string& tip,
                               const std::set<std::string>& rezultate) {
    std::cout << "\n  ┌─── Rezultate ─────────────────────────────────────┐\n";
    std::cout << "  │  Interogare : \"" << interogare << "\"";
    // padding la dreapta
    int pad = 36 - static_cast<int>(interogare.size());
    for (int i = 0; i < pad; ++i) std::cout << ' ';
    std::cout << "│\n";
    std::cout << "  │  Operație   : " << tip;
    pad = 39 - static_cast<int>(tip.size());
    for (int i = 0; i < pad; ++i) std::cout << ' ';
    std::cout << "│\n";
    std::cout << "  ├───────────────────────────────────────────────────┤\n";
    if (rezultate.empty()) {
        std::cout << "  │  ✗  Niciun document găsit.                        │\n";
    } else {
        for (const auto& d : rezultate) {
            std::cout << "  │  ✔  " << d;
            pad = 46 - static_cast<int>(d.size());
            for (int i = 0; i < pad; ++i) std::cout << ' ';
            std::cout << "│\n";
        }
        std::cout << "  ├───────────────────────────────────────────────────┤\n";
        std::cout << "  │  Total: " << rezultate.size() << " document(e)";
        pad = 41 - static_cast<int>(std::to_string(rezultate.size()).size());
        for (int i = 0; i < pad; ++i) std::cout << ' ';
        std::cout << "│\n";
    }
    std::cout << "  └───────────────────────────────────────────────────┘\n";
}

void ruleazaREPL(Index& index, std::shared_ptr<Logger> logger) {
    afiseazaMeniu();

    std::string linie;
    while (true) {
        std::cout << "  ▶ ";
        if (!std::getline(std::cin, linie)) break;

        // Trim spatii
        size_t st = linie.find_first_not_of(" \t");
        if (st == std::string::npos) continue;
        linie = linie.substr(st);

        // EXIT
        if (linie == "exit" || linie == "quit" || linie == "0") {
            std::cout << "\n  " << culori::ROSU
                      << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n"
                      << "  \u2551   La revedere! \u2736                \u2551\n"
                      << "  \u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d" << culori::RESET << "\n\n";
            break;
        }

        size_t sp  = linie.find(' ');
        std::string cmd  = (sp != std::string::npos) ? linie.substr(0, sp) : linie;
        std::string args = (sp != std::string::npos) ? linie.substr(sp + 1) : "";

        if (cmd == "stat" || linie == "4") {
            index.afiseazaStatistici();
        } else if (cmd == "log" || linie == "5") {
            auto logs = logger->citesteLogs();
            size_t start = logs.size() > 10 ? logs.size() - 10 : 0;
            std::cout << "\n  ┌─── Istoric căutări (" << (logs.size()-start)
                      << ") ──────────────────────────┐\n";
            for (size_t i = start; i < logs.size(); ++i)
                std::cout << "  │  " << logs[i] << "\n";
            std::cout << "  └───────────────────────────────────────────────────┘\n";
        } else if (cmd == "cauta" || linie == "1") {
            if (args.empty()) {
                std::cout << "  ⚠  Sintaxă: cauta <cuvânt>\n";
            } else {
                auto r = index.cauta(args, OperatieQuery::SIMPLU);
                afiseazaRezultate(args, "SIMPLU", r);
            }
        } else if (cmd == "and" || linie == "2") {
            if (args.empty()) {
                std::cout << "  ⚠  Sintaxă: and <cuv1> <cuv2> ...\n";
            } else {
                auto r = index.cauta(args, OperatieQuery::AND);
                afiseazaRezultate(args, "AND", r);
            }
        } else if (cmd == "or" || linie == "3") {
            if (args.empty()) {
                std::cout << "  ⚠  Sintaxă: or <cuv1> <cuv2> ...\n";
            } else {
                auto r = index.cauta(args, OperatieQuery::OR);
                afiseazaRezultate(args, "OR", r);
            }
        } else {
            std::cout << "  ⚠  Comandă necunoscută: \"" << cmd
                      << "\". Consultați meniul de mai jos.\n";
        }

        // Reafiseaza meniul dupa fiecare comanda
        afiseazaMeniu();
    }
}


// ════════════════════════════════════════════════════════════════
//  SECTIUNEA 7 – MAIN
// ════════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Utilizare: " << argv[0] << " <director>\n";
        return 1;
    }

    // 1. Cream logger-ul (Observer)
    auto logger = std::make_shared<Logger>("search.log", /*consola=*/false);
    std::cout << "[INFO] Inregistrari log -> search.log\n";

    // 2. Cream indexul si inregistram logger-ul ca observer
    Index index;
    index.adaugaObserver(logger);

    // 3. Incarcam documentele
    if (!index.incarcaDocumente(argv[1])) return 1;

    // 4. Construim indexul (cu filtrare stop-words)
    index.construiesteIndex();

    // 5. REPL interactiv
    ruleazaREPL(index, logger);

    return 0;
}
