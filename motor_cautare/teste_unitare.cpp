/**
 * Teste Unitare – Motor de Cautare Documente
 * ────────────────────────────────────────────
 * Compilare: g++ -std=c++17 -Wall -O2 -o teste teste_unitare.cpp
 * Rulare   : ./teste
 *
 * Framework de testare: macro-uri proprii (fara dependinte externe).
 */

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <memory>
#include <functional>
#include <cctype>

namespace fs = std::filesystem;

// ════════════════════════════════════════════════════════════════
//  MICRO-FRAMEWORK DE TESTARE
// ════════════════════════════════════════════════════════════════

struct RezultatTest {
    std::string numeTest;
    bool trecut;
    std::string mesaj;
};

static std::vector<RezultatTest> REZULTATE;
static std::string SUITA_CURENTA = "";

#define BEGIN_SUITE(name)  SUITA_CURENTA = (name);
#define END_SUITE()        SUITA_CURENTA = "";

#define TEST(name, expr) do { \
    bool _ok = (expr); \
    REZULTATE.push_back({SUITA_CURENTA + " :: " + (name), _ok, \
        _ok ? "" : ("FAILED: " #expr)}); \
    std::cout << (_ok ? "  [PASS] " : "  [FAIL] ") \
              << SUITA_CURENTA << " :: " << (name) << "\n"; \
    if (!_ok) std::cout << "         " << "FAILED: " #expr << "\n"; \
} while(0)

#define TEST_EQ(name, a, b) do { \
    auto _a = (a); auto _b = (b); \
    bool _ok = (_a == _b); \
    REZULTATE.push_back({SUITA_CURENTA + " :: " + (name), _ok, \
        _ok ? "" : "Valorile nu sunt egale"}); \
    std::cout << (_ok ? "  [PASS] " : "  [FAIL] ") \
              << SUITA_CURENTA << " :: " << (name) << "\n"; \
} while(0)

void afiseazaSumar() {
    int total = static_cast<int>(REZULTATE.size());
    int trecute = 0;
    for (const auto& r : REZULTATE) if (r.trecut) ++trecute;
    std::cout << "\n══════════════════════════════════\n";
    std::cout << " SUMAR: " << trecute << "/" << total << " teste trecute\n";
    if (trecute == total)
        std::cout << " Toate testele au trecut! ✓\n";
    else {
        std::cout << " " << (total - trecute) << " teste ESUATE:\n";
        for (const auto& r : REZULTATE)
            if (!r.trecut)
                std::cout << "   - " << r.numeTest << " : " << r.mesaj << "\n";
    }
    std::cout << "══════════════════════════════════\n";
}


// ════════════════════════════════════════════════════════════════
//  CODUL SURSA INCLUS (copii locale pentru testare izolata)
// ════════════════════════════════════════════════════════════════
// (Reproducem clasele direct pentru a nu depinde de linkare externa)

namespace util {
    char toLower(char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    std::string toLowerCase(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), toLower);
        return r;
    }
    std::string normalizeazaCuvant(const std::string& cuvant) {
        std::string r;
        for (char c : cuvant)
            if (std::isalpha(static_cast<unsigned char>(c)) ||
                std::isdigit(static_cast<unsigned char>(c)))
                r += toLower(c);
        return r;
    }
    std::vector<std::string> tokenizeaza(const std::string& text) {
        std::vector<std::string> tokeni;
        std::istringstream flux(text);
        std::string cuv;
        while (flux >> cuv) {
            std::string n = normalizeazaCuvant(cuv);
            if (!n.empty()) tokeni.push_back(n);
        }
        return tokeni;
    }
    std::set<std::string> intersectie(const std::set<std::string>& a,
                                      const std::set<std::string>& b) {
        std::set<std::string> r;
        for (const auto& x : a) if (b.count(x)) r.insert(x);
        return r;
    }
    std::set<std::string> reuniune(const std::set<std::string>& a,
                                   const std::set<std::string>& b) {
        std::set<std::string> r = a; r.insert(b.begin(), b.end()); return r;
    }
    std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        return buf;
    }
}

class StopWords {
public:
    std::unordered_set<std::string> lista;
    StopWords() {
        for (const char* sw : {"si","in","la","pe","cu","de","un","o","a",
             "al","ale","cel","cea","din","sau","the","a","an","and","or",
             "but","in","on","at","to","for","of","with","by","is","are"})
            lista.insert(sw);
    }
    bool esteStopWord(const std::string& c) const { return lista.count(c)>0; }
    std::vector<std::string> filtreaza(
            const std::vector<std::string>& t) const {
        std::vector<std::string> r;
        for (const auto& x : t) if (!esteStopWord(x)) r.push_back(x);
        return r;
    }
};

struct EventCautare {
    std::string interogare, tipOperatie, timestamp;
    std::vector<std::string> rezultate;
};

class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void onCautare(const EventCautare& ev) = 0;
};

// Observer de test – retine evenimentele primite in memorie
class TestObserver : public IObserver {
public:
    std::vector<EventCautare> evenimente;
    void onCautare(const EventCautare& ev) override {
        evenimente.push_back(ev);
    }
    void reseteaza() { evenimente.clear(); }
};

class Logger : public IObserver {
public:
    std::string caleFisierLog;
    Logger(const std::string& f = "test_search.log") : caleFisierLog(f) {}
    void onCautare(const EventCautare& ev) override {
        std::ofstream log(caleFisierLog, std::ios::app);
        if (log.is_open())
            log << "[" << ev.timestamp << "] "
                << "TIP=" << ev.tipOperatie
                << " QUERY=\"" << ev.interogare << "\""
                << " REZULTATE=" << ev.rezultate.size() << "\n";
    }
    std::vector<std::string> citesteLogs() const {
        std::vector<std::string> r;
        std::ifstream f(caleFisierLog);
        std::string l;
        while (std::getline(f, l)) if (!l.empty()) r.push_back(l);
        return r;
    }
    void stergeFisier() {
        std::ofstream f(caleFisierLog, std::ios::trunc);
    }
};

enum class OperatieQuery { SIMPLU, AND, OR };

class Document {
public:
    std::string caleFisier, continut;
    std::vector<std::shared_ptr<IObserver>> observeri;

    Document() = default;
    explicit Document(const std::string& c) : caleFisier(c) {}
    void adaugaObserver(std::shared_ptr<IObserver> o) { observeri.push_back(o); }
    void notifica(const EventCautare& ev) { for (auto& o : observeri) o->onCautare(ev); }
    bool incarca() {
        std::ifstream f(caleFisier);
        if (!f.is_open()) return false;
        std::ostringstream b; b << f.rdbuf(); continut = b.str();
        return true;
    }
    std::string numeFisier() const {
        return fs::path(caleFisier).filename().string();
    }
};

class Index {
public:
    std::map<std::string, std::set<std::string>> indexInvers;
    std::vector<Document> documente;
    StopWords stopWords;
    std::vector<std::shared_ptr<IObserver>> observeri;

    void adaugaObserver(std::shared_ptr<IObserver> o) {
        observeri.push_back(o);
        for (auto& d : documente) d.adaugaObserver(o);
    }

    // Adauga un document cu continut dat direct (fara fisier) – util in teste
    void adaugaDocumentDirect(const std::string& nume,
                               const std::string& text) {
        Document d;
        d.caleFisier = "/tmp/" + nume;
        d.continut   = text;
        for (auto& o : observeri) d.adaugaObserver(o);
        documente.push_back(std::move(d));
    }

    void construiesteIndex() {
        indexInvers.clear();
        for (auto& d : documente) {
            auto t = stopWords.filtreaza(util::tokenizeaza(d.continut));
            for (const auto& x : t) indexInvers[x].insert(d.numeFisier());
        }
    }

    std::set<std::string> cautaSimpu(const std::string& c) const {
        auto it = indexInvers.find(util::normalizeazaCuvant(c));
        return (it != indexInvers.end()) ? it->second : std::set<std::string>{};
    }

    std::set<std::string> cautaANDVec(const std::vector<std::string>& cv) const {
        if (cv.empty()) return {};
        std::set<std::string> r = cautaSimpu(cv[0]);
        for (size_t i = 1; i < cv.size(); ++i)
            r = util::intersectie(r, cautaSimpu(cv[i]));
        return r;
    }

    std::set<std::string> cautaORVec(const std::vector<std::string>& cv) const {
        std::set<std::string> r;
        for (const auto& c : cv) r = util::reuniune(r, cautaSimpu(c));
        return r;
    }

    std::set<std::string> cauta(const std::string& interogare,
                                OperatieQuery op = OperatieQuery::SIMPLU) {
        auto tokeni = stopWords.filtreaza(util::tokenizeaza(interogare));
        std::set<std::string> rez;
        std::string tip;
        if (op == OperatieQuery::AND) { rez = cautaANDVec(tokeni); tip = "AND"; }
        else if (op == OperatieQuery::OR) { rez = cautaORVec(tokeni); tip = "OR"; }
        else {
            rez = tokeni.empty() ? std::set<std::string>{}
                                 : cautaSimpu(tokeni[0]);
            tip = "SIMPLU";
        }
        EventCautare ev;
        ev.interogare  = interogare;
        ev.tipOperatie = tip;
        ev.rezultate   = std::vector<std::string>(rez.begin(), rez.end());
        ev.timestamp   = util::timestamp();
        for (auto& o : observeri) o->onCautare(ev);
        return rez;
    }
};


// ════════════════════════════════════════════════════════════════
//  SUITE DE TESTE
// ════════════════════════════════════════════════════════════════

// ── 1. Utilitare ─────────────────────────────────────────────────
void testeUtil() {
    BEGIN_SUITE("Util")

    TEST("toLower 'A' -> 'a'",
         util::toLower('A') == 'a');

    TEST("toLower caracter deja mic",
         util::toLower('z') == 'z');

    TEST_EQ("normalizeaza elimina punctuatie",
            util::normalizeazaCuvant("C++,"),
            std::string("c"));

    TEST_EQ("normalizeaza pastreaza cifre",
            util::normalizeazaCuvant("test123"),
            std::string("test123"));

    TEST_EQ("normalizeaza string gol",
            util::normalizeazaCuvant("...!"),
            std::string(""));

    auto tokeni = util::tokenizeaza("  Hello, World! ");
    TEST_EQ("tokenizeaza numar tokeni", tokeni.size(), (size_t)2);
    TEST_EQ("tokenizeaza primul token", tokeni[0], std::string("hello"));
    TEST_EQ("tokenizeaza al doilea token", tokeni[1], std::string("world"));

    auto t2 = util::tokenizeaza("");
    TEST("tokenizeaza string gol -> vid", t2.empty());

    std::set<std::string> A = {"a","b","c"}, B = {"b","c","d"};
    auto inters = util::intersectie(A, B);
    TEST_EQ("intersectie marime", inters.size(), (size_t)2);
    TEST("intersectie contine b", inters.count("b") == 1);
    TEST("intersectie contine c", inters.count("c") == 1);

    auto reu = util::reuniune(A, B);
    TEST_EQ("reuniune marime", reu.size(), (size_t)4);

    END_SUITE()
}

// ── 2. StopWords ─────────────────────────────────────────────────
void testeStopWords() {
    BEGIN_SUITE("StopWords")

    StopWords sw;

    TEST("'si' este stop-word", sw.esteStopWord("si"));
    TEST("'in' este stop-word",  sw.esteStopWord("in"));
    TEST("'the' este stop-word", sw.esteStopWord("the"));
    TEST("'and' este stop-word", sw.esteStopWord("and"));
    TEST("'programare' nu e stop-word", !sw.esteStopWord("programare"));
    TEST("'linux' nu e stop-word", !sw.esteStopWord("linux"));
    TEST("'cpp' nu e stop-word", !sw.esteStopWord("cpp"));

    std::vector<std::string> intrare = {"si","linux","in","programare"};
    auto filtrate = sw.filtreaza(intrare);
    TEST_EQ("filtrare: 2 cuvinte ramase", filtrate.size(), (size_t)2);
    TEST("filtrare: linux ramane",
         std::find(filtrate.begin(), filtrate.end(), "linux") != filtrate.end());
    TEST("filtrare: programare ramane",
         std::find(filtrate.begin(), filtrate.end(), "programare") != filtrate.end());

    std::vector<std::string> toateStop = {"si","in","de","cu"};
    auto res = sw.filtreaza(toateStop);
    TEST("filtrare all stop-words -> vid", res.empty());

    END_SUITE()
}

// ── 3. Document ───────────────────────────────────────────────────
void testeDocument() {
    BEGIN_SUITE("Document")

    // Creem un fisier temporar
    const std::string tmpPath = "/tmp/test_doc_motor.txt";
    {
        std::ofstream f(tmpPath);
        f << "Programarea in C++ este puternica.\n";
        f << "Linux este popular printre programatori.\n";
    }

    Document d(tmpPath);
    TEST("incarca fisier existent", d.incarca());
    TEST("continut ne-gol", !d.continut.empty());
    TEST("continut contine 'Programarea'",
         d.continut.find("Programarea") != std::string::npos);
    TEST_EQ("numeFisier corect",
            d.numeFisier(), std::string("test_doc_motor.txt"));

    Document d2("/tmp/fisier_inexistent_xyz.txt");
    TEST("incarca fisier inexistent -> false", !d2.incarca());

    // Observer pe Document
    auto obs = std::make_shared<TestObserver>();
    d.adaugaObserver(obs);
    EventCautare ev{"test","SIMPLU",util::timestamp(),{"test_doc_motor.txt"}};
    d.notifica(ev);
    TEST_EQ("observer primeste eveniment", obs->evenimente.size(), (size_t)1);
    TEST_EQ("observer: interogare corecta",
            obs->evenimente[0].interogare, std::string("test"));

    fs::remove(tmpPath);
    END_SUITE()
}

// ── 4. Index – Constructie ────────────────────────────────────────
void testeIndexConstructie() {
    BEGIN_SUITE("Index::Constructie")

    Index idx;
    idx.adaugaDocumentDirect("doc1.txt",
        "Programarea orientata pe obiecte este un paradigm popular.");
    idx.adaugaDocumentDirect("doc2.txt",
        "C++ suporta programarea generica si mostenirea multipla.");
    idx.construiesteIndex();

    TEST("index ne-gol", !idx.indexInvers.empty());
    TEST("'programarea' indexat",
         idx.indexInvers.count("programarea") > 0);
    TEST("'orientata' indexat",
         idx.indexInvers.count("orientata") > 0);

    // Stop-words nu trebuie sa apara in index
    TEST("'pe' NU e indexat (stop-word)",
         idx.indexInvers.count("pe") == 0);
    TEST("'si' NU e indexat (stop-word)",
         idx.indexInvers.count("si") == 0);

    // 'programarea' trebuie sa apara in ambele documente
    auto docs = idx.indexInvers["programarea"];
    TEST_EQ("'programarea' in 2 documente", docs.size(), (size_t)2);

    END_SUITE()
}

// ── 5. Index – Cautare Simpla ─────────────────────────────────────
void testeIndexCautareSimple() {
    BEGIN_SUITE("Index::CautareSimple")

    Index idx;
    idx.adaugaDocumentDirect("a.txt", "Linux kernel este scris in C.");
    idx.adaugaDocumentDirect("b.txt", "Python este un limbaj interpretat.");
    idx.adaugaDocumentDirect("c.txt", "Linux si Python sunt populare.");
    idx.construiesteIndex();

    auto r = idx.cautaSimpu("linux");
    TEST_EQ("'linux' in 2 documente", r.size(), (size_t)2);
    TEST("'linux' gasit in a.txt", r.count("a.txt") > 0);
    TEST("'linux' gasit in c.txt", r.count("c.txt") > 0);

    auto r2 = idx.cautaSimpu("python");
    TEST_EQ("'python' in 2 documente", r2.size(), (size_t)2);

    auto r3 = idx.cautaSimpu("inexistent_xyz");
    TEST("cuvant inexistent -> vid", r3.empty());

    // Normalizare in cautare
    auto r4 = idx.cautaSimpu("LINUX");
    TEST_EQ("cautare case-insensitive", r4.size(), (size_t)2);

    END_SUITE()
}

// ── 6. Index – Cautare AND ────────────────────────────────────────
void testeIndexCautareAND() {
    BEGIN_SUITE("Index::CautareAND")

    Index idx;
    idx.adaugaDocumentDirect("x.txt", "Programarea C++ pe Linux este rapida.");
    idx.adaugaDocumentDirect("y.txt", "Programarea Python este simpla si eleganta.");
    idx.adaugaDocumentDirect("z.txt", "Linux Python coexista bine.");
    idx.construiesteIndex();

    // AND: programarea + linux -> doar x.txt
    auto r = idx.cauta("programarea linux", OperatieQuery::AND);
    TEST_EQ("AND 'programarea linux' -> 1 doc", r.size(), (size_t)1);
    TEST("AND gasit in x.txt", r.count("x.txt") > 0);

    // AND: programarea + python -> y.txt
    auto r2 = idx.cauta("programarea python", OperatieQuery::AND);
    TEST_EQ("AND 'programarea python' -> 1 doc", r2.size(), (size_t)1);
    TEST("AND gasit in y.txt", r2.count("y.txt") > 0);

    // AND: linux + python -> z.txt
    auto r3 = idx.cauta("linux python", OperatieQuery::AND);
    TEST_EQ("AND 'linux python' -> 1 doc", r3.size(), (size_t)1);
    TEST("AND gasit in z.txt", r3.count("z.txt") > 0);

    // AND fara rezultate
    auto r4 = idx.cauta("programarea xyz_inexistent", OperatieQuery::AND);
    TEST("AND fara rezultate -> vid", r4.empty());

    END_SUITE()
}

// ── 7. Index – Cautare OR ─────────────────────────────────────────
void testeIndexCautareOR() {
    BEGIN_SUITE("Index::CautareOR")

    Index idx;
    idx.adaugaDocumentDirect("p.txt", "C++ ofera performanta ridicata.");
    idx.adaugaDocumentDirect("q.txt", "Java este portabil si robust.");
    idx.adaugaDocumentDirect("r.txt", "Python preferat pentru AI.");
    idx.construiesteIndex();

    // OR: cpp + java -> p.txt si q.txt (c++ normalizat ca c)
    auto r = idx.cauta("java python", OperatieQuery::OR);
    TEST_EQ("OR 'java python' -> 2 doc", r.size(), (size_t)2);
    TEST("OR contine q.txt", r.count("q.txt") > 0);
    TEST("OR contine r.txt", r.count("r.txt") > 0);

    // OR cu un singur cuvant existent
    auto r2 = idx.cauta("performanta", OperatieQuery::OR);
    TEST_EQ("OR singur cuvant -> 1 doc", r2.size(), (size_t)1);

    // OR cu cuvant inexistent
    auto r3 = idx.cauta("inexistent_xyz", OperatieQuery::OR);
    TEST("OR cuvant inexistent -> vid", r3.empty());

    // OR cu toate documentele
    auto r4 = idx.cauta("performanta portabil python", OperatieQuery::OR);
    TEST_EQ("OR 3 cuvinte -> 3 documente", r4.size(), (size_t)3);

    END_SUITE()
}

// ── 8. Observer Pattern (Logger) ──────────────────────────────────
void testeObserverPattern() {
    BEGIN_SUITE("Observer::Pattern")

    // TestObserver in-memory
    auto obs = std::make_shared<TestObserver>();
    Index idx;
    idx.adaugaObserver(obs);
    idx.adaugaDocumentDirect("obs1.txt", "Testarea unitara este importanta.");
    idx.adaugaDocumentDirect("obs2.txt", "Observer pattern faciliteaza decuplarea.");
    idx.construiesteIndex();

    TEST("inainte de cautare: 0 evenimente",
         obs->evenimente.empty());

    idx.cauta("testarea", OperatieQuery::SIMPLU);
    TEST_EQ("dupa 1 cautare: 1 eveniment",
            obs->evenimente.size(), (size_t)1);
    TEST_EQ("tip operatie SIMPLU",
            obs->evenimente[0].tipOperatie, std::string("SIMPLU"));
    TEST_EQ("interogare inregistrata",
            obs->evenimente[0].interogare, std::string("testarea"));
    TEST_EQ("1 rezultat gasit",
            obs->evenimente[0].rezultate.size(), (size_t)1);

    idx.cauta("observer pattern", OperatieQuery::AND);
    TEST_EQ("dupa 2 cautari: 2 evenimente",
            obs->evenimente.size(), (size_t)2);
    TEST_EQ("tip operatie AND",
            obs->evenimente[1].tipOperatie, std::string("AND"));

    idx.cauta("testarea observer", OperatieQuery::OR);
    TEST_EQ("dupa 3 cautari: 3 evenimente",
            obs->evenimente.size(), (size_t)3);
    TEST_EQ("tip operatie OR",
            obs->evenimente[2].tipOperatie, std::string("OR"));

    // Multiple observatori
    auto obs2 = std::make_shared<TestObserver>();
    idx.adaugaObserver(obs2);
    obs->reseteaza();
    idx.cauta("testarea");
    TEST_EQ("obs1 notificat", obs->evenimente.size(), (size_t)1);
    TEST_EQ("obs2 notificat", obs2->evenimente.size(), (size_t)1);

    END_SUITE()
}

// ── 9. Logger (Observer concret cu fisier) ────────────────────────
void testeLogger() {
    BEGIN_SUITE("Observer::Logger")

    const std::string logPath = "/tmp/test_motor_log.log";
    auto logger = std::make_shared<Logger>(logPath);
    logger->stergeFisier();

    Index idx;
    idx.adaugaObserver(logger);
    idx.adaugaDocumentDirect("log1.txt",
        "Logarea evenimentelor este esentiala.");
    idx.construiesteIndex();

    // Inainte de cautare: log vid
    TEST("log vid initial", logger->citesteLogs().empty());

    idx.cauta("logarea");
    auto logs = logger->citesteLogs();
    TEST_EQ("dupa 1 cautare: 1 intrare log", logs.size(), (size_t)1);
    TEST("log contine SIMPLU",
         logs[0].find("SIMPLU") != std::string::npos);
    TEST("log contine interogarea",
         logs[0].find("logarea") != std::string::npos);

    idx.cauta("logarea evenimentelor", OperatieQuery::AND);
    logs = logger->citesteLogs();
    TEST_EQ("dupa 2 cautari: 2 intrari log", logs.size(), (size_t)2);
    TEST("a doua intrare contine AND",
         logs[1].find("AND") != std::string::npos);

    // Verificam ca timestamp-ul e prezent (contine ':')
    TEST("log contine timestamp",
         logs[0].find(':') != std::string::npos);

    fs::remove(logPath);
    END_SUITE()
}

// ── 10. Cazuri Edge ───────────────────────────────────────────────
void testeCazuriEdge() {
    BEGIN_SUITE("EdgeCases")

    Index idx;
    idx.construiesteIndex();
    TEST("index gol: cautare returneaza vid",
         idx.cauta("orice").empty());

    // Document cu doar stop-words
    Index idx2;
    idx2.adaugaDocumentDirect("stop.txt", "si in la pe cu de");
    idx2.construiesteIndex();
    TEST("document cu doar stop-words: index vid",
         idx2.indexInvers.empty());

    // AND cu un singur cuvant
    Index idx3;
    idx3.adaugaDocumentDirect("single.txt", "test unic document");
    idx3.construiesteIndex();
    auto r = idx3.cauta("test", OperatieQuery::AND);
    TEST_EQ("AND cu 1 cuvant functioneaza", r.size(), (size_t)1);

    // OR cu zero cuvinte utile (toate stop-words)
    auto r2 = idx3.cauta("si in de", OperatieQuery::OR);
    TEST("OR cu doar stop-words -> vid", r2.empty());

    // Cifre in cuvinte
    Index idx4;
    idx4.adaugaDocumentDirect("num.txt", "versiunea 2 si varianta 3a sunt noi");
    idx4.construiesteIndex();
    TEST("cifre indexate corect",
         idx4.indexInvers.count("2") > 0 || idx4.indexInvers.count("3a") > 0);

    // Cuvant cu diacritice si caractere speciale
    auto norm = util::normalizeazaCuvant("C++,;!");
    TEST("normalizeaza C++ -> c", norm == "c");

    END_SUITE()
}

// ── 11. Validare tranzactii Index ─────────────────────────────────
void testeValidariIndex() {
    BEGIN_SUITE("Validari::Index")

    Index idx;
    idx.adaugaDocumentDirect("v1.txt", "alpha beta gamma");
    idx.adaugaDocumentDirect("v2.txt", "beta gamma delta");
    idx.adaugaDocumentDirect("v3.txt", "gamma delta epsilon");
    idx.construiesteIndex();

    // Consistenta index: numarul de aparitii
    TEST_EQ("'gamma' in 3 documente",
            idx.indexInvers["gamma"].size(), (size_t)3);
    TEST_EQ("'beta' in 2 documente",
            idx.indexInvers["beta"].size(), (size_t)2);
    TEST_EQ("'alpha' in 1 document",
            idx.indexInvers["alpha"].size(), (size_t)1);

    // AND este comutativ: A AND B == B AND A
    auto ab = idx.cauta("alpha beta", OperatieQuery::AND);
    auto ba = idx.cauta("beta alpha", OperatieQuery::AND);
    TEST("AND comutativ", ab == ba);

    // OR este comutativ
    auto or_ab = idx.cauta("alpha delta", OperatieQuery::OR);
    auto or_ba = idx.cauta("delta alpha", OperatieQuery::OR);
    TEST("OR comutativ", or_ab == or_ba);

    // A AND B ⊆ A OR B
    auto r_and = idx.cauta("beta delta", OperatieQuery::AND);
    auto r_or  = idx.cauta("beta delta", OperatieQuery::OR);
    bool subset = true;
    for (const auto& x : r_and)
        if (!r_or.count(x)) { subset = false; break; }
    TEST("AND rezultate subset din OR", subset);

    // Reconstruct index: rezultate consistente
    idx.construiesteIndex();
    auto r2 = idx.cautaSimpu("gamma");
    TEST_EQ("dupa reconstruct: 'gamma' in 3 doc", r2.size(), (size_t)3);

    END_SUITE()
}


// ════════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════════
int main() {
    std::cout << "══════════════════════════════════\n";
    std::cout << " TESTE UNITARE – Motor de Cautare\n";
    std::cout << "══════════════════════════════════\n\n";

    testeUtil();
    std::cout << "\n";
    testeStopWords();
    std::cout << "\n";
    testeDocument();
    std::cout << "\n";
    testeIndexConstructie();
    std::cout << "\n";
    testeIndexCautareSimple();
    std::cout << "\n";
    testeIndexCautareAND();
    std::cout << "\n";
    testeIndexCautareOR();
    std::cout << "\n";
    testeObserverPattern();
    std::cout << "\n";
    testeLogger();
    std::cout << "\n";
    testeCazuriEdge();
    std::cout << "\n";
    testeValidariIndex();

    afiseazaSumar();

    // Cod de iesire: 0 daca toate trec
    for (const auto& r : REZULTATE)
        if (!r.trecut) return 1;
    return 0;
}
