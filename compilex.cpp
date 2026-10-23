// ===============================================================
//  CompileX (Code Lipi)  -  UITS Compiler Lab
//  Ekta chhoto programming language jar function er nam Banglish e.
//
//  Compiler er 3 ta dhap (phase):
//    1. LEXER        source code -> tokens
//    2. PARSER       tokens      -> AST (syntax tree)
//    3. INTERPRETER  AST         -> output (tree walk kore chalay)
//
//  Build:  g++ -std=c++17 -o compilex compilex.cpp
//  Run:    ./compilex                    (interactive: nijer hate type koro)
//          ./compilex demo.cx            (file chalao)
//          ./compilex demo.cx --tokens   (lexer er output)
//          ./compilex demo.cx --ast      (parser er output, text tree)
//          ./compilex demo.cx --html     (parser er output, browser e chhobi)
//          ./compilex functions.cx --report  (proti function er token + tree + output, HTML)
// ===============================================================
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

// ---------------------------------------------------------------
// Error: line number soho error dekhanor jonno
// ---------------------------------------------------------------
struct CompileXError {
    string message;
    int line;
};

[[noreturn]] void fail(const string& msg, int line) {
    throw CompileXError{msg, line};
}

// ---------------------------------------------------------------
// Value: CompileX er jekono man (nil, bool, number, text, list)
// ---------------------------------------------------------------
enum class VT { Nil, Bool, Num, Str, List };

struct Value {
    VT type = VT::Nil;
    bool b = false;
    double num = 0;
    string str;
    vector<Value> list;
};

Value mkNum(double d)        { Value v; v.type = VT::Num;  v.num = d; return v; }
Value mkStr(const string& s) { Value v; v.type = VT::Str;  v.str = s; return v; }
Value mkBool(bool b)         { Value v; v.type = VT::Bool; v.b = b;   return v; }
Value mkList(const vector<Value>& l) { Value v; v.type = VT::List; v.list = l; return v; }

string showNum(double d) {                      // 5.0 -> "5",  2.5 -> "2.5"
    if (d == floor(d) && fabs(d) < 1e15) return to_string((long long)d);
    ostringstream o;
    o << d;
    return o.str();
}

string show(const Value& v, bool quote = false) {   // output er jonno text
    switch (v.type) {
        case VT::Nil:  return "nil";
        case VT::Bool: return v.b ? "sotti" : "mittha";
        case VT::Num:  return showNum(v.num);
        case VT::Str:  return quote ? "\"" + v.str + "\"" : v.str;
        case VT::List: {
            string s = "[";
            for (size_t i = 0; i < v.list.size(); i++) {
                if (i) s += ", ";
                s += show(v.list[i], true);
            }
            return s + "]";
        }
    }
    return "";
}

bool equals(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VT::Nil:  return true;
        case VT::Bool: return a.b == b.b;
        case VT::Num:  return a.num == b.num;
        case VT::Str:  return a.str == b.str;
        case VT::List:
            if (a.list.size() != b.list.size()) return false;
            for (size_t i = 0; i < a.list.size(); i++)
                if (!equals(a.list[i], b.list[i])) return false;
            return true;
    }
    return false;
}

int compareValues(const Value& a, const Value& b, int line) {   // -1, 0, 1
    if (a.type == VT::Num && b.type == VT::Num) return (a.num > b.num) - (a.num < b.num);
    if (a.type == VT::Str && b.type == VT::Str) {
        int c = a.str.compare(b.str);
        return (c > 0) - (c < 0);
    }
    fail("Ei dui type tulona kora jay na", line);
}

bool truthy(const Value& v) {
    switch (v.type) {
        case VT::Nil:  return false;
        case VT::Bool: return v.b;
        case VT::Num:  return v.num != 0;
        case VT::Str:  return !v.str.empty();
        case VT::List: return !v.list.empty();
    }
    return false;
}

// ===============================================================
// PHASE 1 : LEXER  (text ke tukro tukro token e bhag kora)
// ===============================================================
enum class TT { Number, String, Name, Keyword, Op, End };

struct Token {
    TT type;
    string text;
    double num = 0;
    int line = 0;
};

const vector<string> KEYWORDS = {"dhori", "jodi", "tahole", "nahole", "jotokhon",
                                 "koro", "sesh", "sotti", "mittha", "ebong", "ba"};

bool isKeyword(const string& s) {
    return find(KEYWORDS.begin(), KEYWORDS.end(), s) != KEYWORDS.end();
}

vector<Token> lexer(const string& src) {
    vector<Token> tokens;
    int line = 1;
    size_t i = 0;

    while (i < src.size()) {
        char c = src[i];

        if (c == '\n') { line++; i++; }                          // notun line
        else if (isspace((unsigned char)c)) { i++; }             // space ignore
        else if (c == '#') {                                     // comment
            while (i < src.size() && src[i] != '\n') i++;
        }
        else if (isdigit((unsigned char)c)) {                    // 12  3.5
            size_t start = i;
            while (isdigit((unsigned char)src[i])) i++;
            if (src[i] == '.' && isdigit((unsigned char)src[i + 1])) {
                i++;
                while (isdigit((unsigned char)src[i])) i++;
            }
            string text = src.substr(start, i - start);
            tokens.push_back({TT::Number, text, stod(text), line});
        }
        else if (c == '"') {                                     // "hello"
            i++;
            string s;
            while (i < src.size() && src[i] != '"' && src[i] != '\n') s += src[i++];
            if (i >= src.size() || src[i] != '"') fail("String shesh hoy ni (\" missing)", line);
            i++;
            tokens.push_back({TT::String, s, 0, line});
        }
        else if (isalpha((unsigned char)c) || c == '_') {        // nam / keyword
            size_t start = i;
            while (isalnum((unsigned char)src[i]) || src[i] == '_') i++;
            string word = src.substr(start, i - start);
            tokens.push_back({isKeyword(word) ? TT::Keyword : TT::Name, word, 0, line});
        }
        else {                                                   // operator
            string two = src.substr(i, 2);
            if (two == "==" || two == "!=" || two == "<=" || two == ">=") {
                tokens.push_back({TT::Op, two, 0, line});
                i += 2;
            } else if (string("+-*/%<>=(),[]").find(c) != string::npos) {
                tokens.push_back({TT::Op, string(1, c), 0, line});
                i++;
            } else {
                fail(string("Unknown character '") + c + "'", line);
            }
        }
    }
    tokens.push_back({TT::End, "", 0, line});
    return tokens;
}

// ===============================================================
// PHASE 2 : PARSER  (tokens theke AST banano)
// ===============================================================
// Grammar (songkhep e):
//   program   : statement*
//   statement : dhori NAME = expr | NAME = expr | if | while | expr
//   if        : jodi expr tahole statement* [nahole statement*] sesh
//   while     : jotokhon expr koro statement* sesh
//   expr      : and_expr ( ba and_expr )*
//   and_expr  : compare ( ebong compare )*
//   compare   : sum ( (== != < > <= >=) sum )?
//   sum       : term ( (+ -) term )*
//   term      : unary ( (* / %) unary )*
//   unary     : - unary | postfix
//   postfix   : primary ( [expr] )*
//   primary   : number | string | sotti | mittha | [list]
//             | NAME(args) | NAME | ( expr )
enum class NK { Program, Block, Let, Assign, If, While, ExprStmt,
                Num, Str, Bool, List, Var, Neg, Index, Binop, Call };

const char* NK_NAMES[] = {"Program", "Block", "Let", "Assign", "If", "While", "ExprStmt",
                          "Num", "Str", "Bool", "List", "Var", "Neg", "Index", "Binop", "Call"};

struct Node;
using NodeP = shared_ptr<Node>;

struct Node {
    NK kind;
    string text;          // nam / operator / string value
    double num = 0;
    int line = 0;
    vector<NodeP> kids;   // child node gulo
};

string toLower(string s) {                          // Gun -> gun (function nam capital/small dutoi cholbe)
    for (auto& c : s) c = tolower((unsigned char)c);
    return s;
}

NodeP makeNode(NK kind, int line, const string& text = "") {
    auto n = make_shared<Node>();
    n->kind = kind;
    n->line = line;
    n->text = text;
    return n;
}

class Parser {
    vector<Token> t;
    size_t pos = 0;

    // ---- chhoto helper ----
    const Token& peek() { return t[pos]; }
    const Token& next() { return t[pos++]; }

    bool is(const string& text) {
        return (peek().type == TT::Keyword || peek().type == TT::Op) && peek().text == text;
    }
    void expect(const string& text) {
        if (!is(text)) {
            string found = peek().type == TT::End ? "end of file" : peek().text;
            fail("Expected '" + text + "' but found '" + found + "'", peek().line);
        }
        pos++;
    }

    // ---- statements ----
    NodeP block(const vector<string>& stoppers) {
        NodeP b = makeNode(NK::Block, peek().line);
        while (true) {
            bool stop = false;
            for (auto& s : stoppers) if (is(s)) stop = true;
            if (stop) break;
            if (peek().type == TT::End) fail("'sesh' missing (block shesh hoy ni)", peek().line);
            b->kids.push_back(statement());
        }
        return b;
    }

    NodeP statement() {
        int line = peek().line;

        if (is("dhori")) {                                   // dhori x = 5
            next();
            if (peek().type != TT::Name) fail("'dhori' er pore variable nam dao", peek().line);
            NodeP n = makeNode(NK::Let, line, next().text);
            expect("=");
            n->kids.push_back(expr());
            return n;
        }

        if (peek().type == TT::Name && t[pos + 1].type == TT::Op && t[pos + 1].text == "=") {
            NodeP n = makeNode(NK::Assign, line, next().text);   // x = 5
            next();                                              // '=' skip
            n->kids.push_back(expr());
            return n;
        }

        if (is("jodi")) {                                    // jodi ... tahole ... sesh
            next();
            NodeP n = makeNode(NK::If, line);
            n->kids.push_back(expr());
            expect("tahole");
            n->kids.push_back(block({"nahole", "sesh"}));
            if (is("nahole")) {
                next();
                n->kids.push_back(block({"sesh"}));
            } else {
                n->kids.push_back(makeNode(NK::Block, line));    // khali else
            }
            expect("sesh");
            return n;
        }

        if (is("jotokhon")) {                                // jotokhon ... koro ... sesh
            next();
            NodeP n = makeNode(NK::While, line);
            n->kids.push_back(expr());
            expect("koro");
            n->kids.push_back(block({"sesh"}));
            expect("sesh");
            return n;
        }

        NodeP n = makeNode(NK::ExprStmt, line);              // jemon: dekhao("hi")
        n->kids.push_back(expr());
        return n;
    }

    // ---- expressions (precedence: kom theke beshi) ----
    NodeP binary(const string& op, int line, NodeP l, NodeP r) {
        NodeP n = makeNode(NK::Binop, line, op);
        n->kids = {l, r};
        return n;
    }

    NodeP expr() {                                           // ba
        NodeP left = andExpr();
        while (is("ba")) { int ln = next().line; left = binary("ba", ln, left, andExpr()); }
        return left;
    }
    NodeP andExpr() {                                        // ebong
        NodeP left = compare();
        while (is("ebong")) { int ln = next().line; left = binary("ebong", ln, left, compare()); }
        return left;
    }
    NodeP compare() {                                        // == != < > <= >=
        NodeP left = sum();
        for (string op : {"==", "!=", "<=", ">=", "<", ">"}) {
            if (is(op)) { int ln = next().line; return binary(op, ln, left, sum()); }
        }
        return left;
    }
    NodeP sum() {                                            // + -
        NodeP left = term();
        while (is("+") || is("-")) {
            Token op = next();
            left = binary(op.text, op.line, left, term());
        }
        return left;
    }
    NodeP term() {                                           // * / %
        NodeP left = unary();
        while (is("*") || is("/") || is("%")) {
            Token op = next();
            left = binary(op.text, op.line, left, unary());
        }
        return left;
    }
    NodeP unary() {                                          // -x
        if (is("-")) {
            NodeP n = makeNode(NK::Neg, next().line);
            n->kids.push_back(unary());
            return n;
        }
        return postfix();
    }
    NodeP postfix() {                                        // list[0]
        NodeP node = primary();
        while (is("[")) {
            NodeP n = makeNode(NK::Index, next().line);
            n->kids.push_back(node);
            n->kids.push_back(expr());
            expect("]");
            node = n;
        }
        return node;
    }

    NodeP primary() {
        Token tok = next();

        if (tok.type == TT::Number) {
            NodeP n = makeNode(NK::Num, tok.line);
            n->num = tok.num;
            return n;
        }
        if (tok.type == TT::String) return makeNode(NK::Str, tok.line, tok.text);

        if (tok.type == TT::Keyword && (tok.text == "sotti" || tok.text == "mittha"))
            return makeNode(NK::Bool, tok.line, tok.text);

        if (tok.type == TT::Op && tok.text == "(") {         // ( expr )
            NodeP e = expr();
            expect(")");
            return e;
        }

        if (tok.type == TT::Op && tok.text == "[") {         // [1, 2, 3]
            NodeP n = makeNode(NK::List, tok.line);
            if (!is("]")) {
                n->kids.push_back(expr());
                while (is(",")) { next(); n->kids.push_back(expr()); }
            }
            expect("]");
            return n;
        }

        if (tok.type == TT::Name) {
            if (is("(")) {                                   // function call
                next();
                NodeP n = makeNode(NK::Call, tok.line, toLower(tok.text));
                if (!is(")")) {
                    n->kids.push_back(expr());
                    while (is(",")) { next(); n->kids.push_back(expr()); }
                }
                expect(")");
                return n;
            }
            return makeNode(NK::Var, tok.line, tok.text);
        }

        fail("Unexpected '" + (tok.type == TT::End ? string("end of file") : tok.text) + "'", tok.line);
    }

public:
    Parser(const vector<Token>& tokens) : t(tokens) {}

    NodeP program() {
        NodeP p = makeNode(NK::Program, 1);
        while (peek().type != TT::End) p->kids.push_back(statement());
        return p;
    }
};

// ===============================================================
// PHASE 3 : INTERPRETER  (AST walk kore code chalano)
// ===============================================================
class Interpreter {
    map<string, Value> vars;        // sob variable ekhane thake

public:
    // report er jonno: ekhon kon variable e ki man ache
    vector<pair<string, string>> variables() const {
        vector<pair<string, string>> out;
        for (auto& kv : vars) out.push_back({kv.first, show(kv.second, true)});
        return out;
    }

    // ---- statement chalano ----
    void exec(const NodeP& n) {
        switch (n->kind) {
            case NK::Program:
            case NK::Block:
                for (auto& k : n->kids) exec(k);
                break;
            case NK::Let:
                vars[n->text] = eval(n->kids[0]);
                break;
            case NK::Assign:
                if (!vars.count(n->text)) fail("'" + n->text + "' age 'dhori' diye declare koro nai", n->line);
                vars[n->text] = eval(n->kids[0]);
                break;
            case NK::If:
                if (truthy(eval(n->kids[0]))) exec(n->kids[1]);
                else exec(n->kids[2]);
                break;
            case NK::While:
                while (truthy(eval(n->kids[0]))) exec(n->kids[1]);
                break;
            case NK::ExprStmt:
                eval(n->kids[0]);
                break;
            default:
                break;
        }
    }

    // ---- expression er man ber kora ----
    Value eval(const NodeP& n) {
        switch (n->kind) {
            case NK::Num:  return mkNum(n->num);
            case NK::Str:  return mkStr(n->text);
            case NK::Bool: return mkBool(n->text == "sotti");
            case NK::List: {
                vector<Value> items;
                for (auto& k : n->kids) items.push_back(eval(k));
                return mkList(items);
            }
            case NK::Var:
                if (!vars.count(n->text)) fail("Variable '" + n->text + "' paoa jay ni", n->line);
                return vars[n->text];
            case NK::Neg: {
                Value v = eval(n->kids[0]);
                if (v.type != VT::Num) fail("'-' shudhu number e hoy", n->line);
                return mkNum(-v.num);
            }
            case NK::Index: return index(n);
            case NK::Binop: return binop(n);
            case NK::Call:  return call(n);
            default:        return Value();
        }
    }

private:
    Value index(const NodeP& n) {
        Value obj = eval(n->kids[0]), idx = eval(n->kids[1]);
        if (idx.type != VT::Num || idx.num != floor(idx.num) || idx.num < 0) fail("Index thik nai", n->line);
        size_t i = (size_t)idx.num;
        if (obj.type == VT::List && i < obj.list.size()) return obj.list[i];
        if (obj.type == VT::Str && i < obj.str.size()) return mkStr(string(1, obj.str[i]));
        fail("Index thik nai", n->line);
    }

    double needNum(const Value& v, int line) {
        if (v.type != VT::Num) fail("Number dewa hoy ni", line);
        return v.num;
    }

    Value binop(const NodeP& n) {
        const string& op = n->text;
        int line = n->line;

        if (op == "ba")    return mkBool(truthy(eval(n->kids[0])) || truthy(eval(n->kids[1])));
        if (op == "ebong") return mkBool(truthy(eval(n->kids[0])) && truthy(eval(n->kids[1])));

        Value a = eval(n->kids[0]), b = eval(n->kids[1]);

        if (op == "==") return mkBool(equals(a, b));
        if (op == "!=") return mkBool(!equals(a, b));
        if (op == "<")  return mkBool(compareValues(a, b, line) < 0);
        if (op == ">")  return mkBool(compareValues(a, b, line) > 0);
        if (op == "<=") return mkBool(compareValues(a, b, line) <= 0);
        if (op == ">=") return mkBool(compareValues(a, b, line) >= 0);

        if (op == "+" && a.type == VT::Str && b.type == VT::Str) return mkStr(a.str + b.str);
        if (op == "+" && a.type == VT::List && b.type == VT::List) {     // list + list
            Value r = a;
            r.list.insert(r.list.end(), b.list.begin(), b.list.end());
            return r;
        }

        double x = needNum(a, line), y = needNum(b, line);
        if (op == "+") return mkNum(x + y);
        if (op == "-") return mkNum(x - y);
        if (op == "*") return mkNum(x * y);
        if (op == "/") {
            if (y == 0) fail("Shunno diye vag kora jay na", line);
            return mkNum(x / y);
        }
        if (op == "%") {
            if (y == 0) fail("Shunno diye vagsesh kora jay na", line);
            return mkNum(fmod(x, y));
        }
        fail("Unknown operator " + op, line);
    }

    // ---- bodol, mucha, saf: variable er man *bodlay*, tai alada ----
    Value special(const NodeP& n) {
        const string& name = n->text;
        int line = n->line;
        size_t need = (name == "saf") ? 1 : 2;
        if (n->kids.size() != need) fail("'" + name + "' e " + to_string(need) + " ta argument lage", line);

        for (size_t i = 0; i < (name == "bodol" ? 2u : 1u); i++) {
            if (n->kids[i]->kind != NK::Var) fail("'" + name + "' e variable er nam dao", line);
            if (!vars.count(n->kids[i]->text)) fail("Variable '" + n->kids[i]->text + "' paoa jay ni", line);
        }

        if (name == "bodol") {                               // 18. Swap
            swap(vars[n->kids[0]->text], vars[n->kids[1]->text]);
        }
        else if (name == "mucha") {                          // 19. Delete
            Value& data = vars[n->kids[0]->text];
            Value target = eval(n->kids[1]);
            if (data.type == VT::List) {
                for (size_t i = 0; i < data.list.size(); i++)
                    if (equals(data.list[i], target)) { data.list.erase(data.list.begin() + i); break; }
            } else if (data.type == VT::Str && target.type == VT::Str) {
                size_t p = data.str.find(target.str);
                if (p != string::npos) data.str.erase(p, target.str.size());
            } else {
                fail("'mucha' list ba text e kaj kore", line);
            }
        }
        else {                                               // 20. Clear
            Value& data = vars[n->kids[0]->text];
            if (data.type == VT::List) data.list.clear();
            else if (data.type == VT::Str) data.str.clear();
            else if (data.type == VT::Num) data.num = 0;
            else data = Value();
        }
        return Value();
    }

    // ---- baki 17 ta built-in function ----
    Value call(const NodeP& n) {
        const string& name = n->text;
        int line = n->line;

        if (name == "bodol" || name == "mucha" || name == "saf") return special(n);

        vector<Value> args;
        for (auto& k : n->kids) args.push_back(eval(k));

        // argument sonkhya check
        auto need = [&](size_t k) {
            if (args.size() != k) fail("'" + name + "' e " + to_string(k) + " ta argument lage", line);
        };
        // i-th argument number kina check kore man dey
        auto num = [&](size_t i) { return needNum(args[i], line); };
        // gor/boro/choto er jonno: (1,2,3) ba ([1,2,3]) dui vabei cholbe
        auto numbers = [&]() {
            const vector<Value>& src = (args.size() == 1 && args[0].type == VT::List) ? args[0].list : args;
            if (src.empty()) fail("'" + name + "' e kichu number dao", line);
            vector<double> out;
            for (auto& v : src) out.push_back(needNum(v, line));
            return out;
        };

        // ---------- Math ----------
        if (name == "jog")     { need(2); return mkNum(num(0) + num(1)); }          // 1  Add
        if (name == "biyog")   { need(2); return mkNum(num(0) - num(1)); }          // 2  Subtract
        if (name == "gun")     { need(2); return mkNum(num(0) * num(1)); }          // 3  Multiply
        if (name == "vag") {                                                        // 4  Divide
            need(2);
            if (num(1) == 0) fail("Shunno diye vag kora jay na", line);
            return mkNum(num(0) / num(1));
        }
        if (name == "vagsesh") {                                                    // 5  Modulus
            need(2);
            if (num(1) == 0) fail("Shunno diye vagsesh kora jay na", line);
            return mkNum(fmod(num(0), num(1)));
        }
        if (name == "ghat")    { need(2); return mkNum(pow(num(0), num(1))); }      // 6  Power

        // ---------- Number list ----------
        if (name == "gor") {                                                        // 7  Average
            vector<double> v = numbers();
            double total = 0;
            for (double x : v) total += x;
            return mkNum(total / v.size());
        }
        if (name == "boro")  { vector<double> v = numbers(); return mkNum(*max_element(v.begin(), v.end())); }  // 8  Maximum
        if (name == "choto") { vector<double> v = numbers(); return mkNum(*min_element(v.begin(), v.end())); }  // 9  Minimum

        // ---------- Input / Output ----------
        if (name == "dekhao") {                                                     // 10 Print
            for (size_t i = 0; i < args.size(); i++) {
                if (i) cout << " ";
                cout << show(args[i]);
            }
            cout << "\n";
            return Value();
        }
        if (name == "nao") {                                                        // 11 Input
            if (args.size() > 1) fail("'nao' e 0 ba 1 ta argument lage", line);
            if (args.size() == 1) cout << show(args[0]) << flush;
            string text;
            getline(cin, text);
            char* end = nullptr;
            double d = strtod(text.c_str(), &end);
            if (!text.empty() && *end == '\0') return mkNum(d);     // number hole number
            return mkStr(text);                                      // nahole text
        }

        // ---------- Text / List ----------
        if (name == "lomba") {                                                      // 12 Length
            need(1);
            if (args[0].type == VT::Str)  return mkNum(args[0].str.size());
            if (args[0].type == VT::List) return mkNum(args[0].list.size());
            fail("'lomba' text ba list e kaj kore", line);
        }
        if (name == "ulta") {                                                       // 13 Reverse
            need(1);
            Value v = args[0];
            if (v.type == VT::Str)  { reverse(v.str.begin(), v.str.end());   return v; }
            if (v.type == VT::List) { reverse(v.list.begin(), v.list.end()); return v; }
            fail("'ulta' text ba list e kaj kore", line);
        }
        if (name == "sajao") {                                                      // 14 Sort
            need(1);
            if (args[0].type != VT::List) fail("'sajao' list e kaj kore", line);
            Value v = args[0];
            sort(v.list.begin(), v.list.end(),
                 [&](const Value& a, const Value& b) { return compareValues(a, b, line) < 0; });
            return v;
        }
        if (name == "khojo") {                                                      // 15 Search (position, na pele -1)
            need(2);
            if (args[0].type == VT::Str && args[1].type == VT::Str) {
                size_t p = args[0].str.find(args[1].str);
                return mkNum(p == string::npos ? -1 : (double)p);
            }
            if (args[0].type == VT::List) {
                for (size_t i = 0; i < args[0].list.size(); i++)
                    if (equals(args[0].list[i], args[1])) return mkNum((double)i);
                return mkNum(-1);
            }
            fail("'khojo' list ba text e kaj kore", line);
        }
        if (name == "gono") {                                                       // 16 Count
            if (args.empty() || args.size() > 2) fail("'gono' e 1 ba 2 ta argument lage", line);
            const Value& c = args[0];
            if (c.type != VT::List && c.type != VT::Str) fail("'gono' list ba text e kaj kore", line);
            if (args.size() == 1) return mkNum(c.type == VT::List ? c.list.size() : c.str.size());
            int count = 0;
            if (c.type == VT::List) {
                for (auto& item : c.list) if (equals(item, args[1])) count++;
            } else {
                if (args[1].type != VT::Str || args[1].str.empty()) fail("'gono' e text dao", line);
                for (size_t p = c.str.find(args[1].str); p != string::npos;
                     p = c.str.find(args[1].str, p + args[1].str.size())) count++;
            }
            return mkNum(count);
        }
        if (name == "melao") {                                                      // 17 Compare (-1, 0, 1)
            need(2);
            return mkNum(compareValues(args[0], args[1], line));
        }

        // ---------- Extra 5 ta chhoto function ----------
        if (name == "mot") {                                                        // 21 Sum
            vector<double> v = numbers();
            double total = 0;
            for (double x : v) total += x;
            return mkNum(total);
        }
        if (name == "borgo") { need(1); return mkNum(num(0) * num(0)); }            // 22 Square
        if (name == "mul") {                                                        // 23 Square root
            need(1);
            if (num(0) < 0) fail("Negative number er mul hoy na", line);
            return mkNum(sqrt(num(0)));
        }
        if (name == "gol")   { need(1); return mkNum(round(num(0))); }              // 24 Round
        if (name == "porom") { need(1); return mkNum(fabs(num(0))); }               // 25 Absolute

        fail("Function '" + name + "' nai", line);
    }
};

// ===============================================================
// Token / AST print (viva te dekhanor jonno)
// ===============================================================
void printTokens(const vector<Token>& tokens) {
    const char* names[] = {"NUMBER", "STRING", "NAME", "KEYWORD", "OP", "EOF"};
    cout << "=== TOKENS ===\n";
    for (auto& t : tokens)
        cout << "(" << names[(int)t.type] << ", '" << t.text << "', line " << t.line << ")\n";
}

// ---- AST ke shundor kore dekhanor jonno ----
// Node er nam sohoj kore dey (parent o index dekhe, jemon If er 2nd child = "tahole")
string nodeLabel(const NodeP& n, const NodeP& parent, size_t idx) {
    switch (n->kind) {
        case NK::Program:  return "Program";
        case NK::Block:
            if (parent && parent->kind == NK::If)    return idx == 1 ? "tahole" : "nahole";
            if (parent && parent->kind == NK::While) return "koro";
            return "Block";
        case NK::Let:      return "dhori " + n->text;
        case NK::Assign:   return n->text + " =";
        case NK::If:       return "jodi";
        case NK::While:    return "jotokhon";
        case NK::ExprStmt: return "Statement";
        case NK::Num:      return showNum(n->num);
        case NK::Str:      return "\"" + n->text + "\"";
        case NK::Bool:     return n->text;
        case NK::List:     return "[ ]";
        case NK::Var:      return n->text;
        case NK::Neg:      return "- (minus)";
        case NK::Index:    return "[ index ]";
        case NK::Binop:    return n->text;
        case NK::Call:     return n->text + "()";
    }
    return "";
}

// If er khali 'nahole' block dekhanor dorkar nai
bool hidden(const NodeP& n, size_t i) {
    return n->kind == NK::If && i == 2 && n->kids[2]->kids.empty();
}

// Text tree:   +-- node   (shesh child e `-- )
void printTree(const NodeP& n, const NodeP& parent, size_t idx, const string& prefix, bool last, bool root) {
    cout << prefix << (root ? "" : (last ? "`-- " : "+-- ")) << nodeLabel(n, parent, idx) << "\n";
    string childPrefix = prefix + (root ? "" : (last ? "    " : "|   "));
    vector<size_t> shown;
    for (size_t i = 0; i < n->kids.size(); i++) if (!hidden(n, i)) shown.push_back(i);
    for (size_t j = 0; j < shown.size(); j++)
        printTree(n->kids[shown[j]], n, shown[j], childPrefix, j + 1 == shown.size(), false);
}

// ---- HTML tree: browser e boro kore diagram dekhay ----
string htmlEscape(const string& s) {
    string out;
    for (char c : s) {
        if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '&') out += "&amp;";
        else out += c;
    }
    return out;
}

string cssClass(NK k) {
    switch (k) {
        case NK::Call: return "call";
        case NK::Num: case NK::Str: case NK::Bool: case NK::List: return "lit";
        case NK::Var: return "var";
        case NK::Binop: case NK::Neg: case NK::Index: return "op";
        default: return "stmt";
    }
}

void htmlNode(ostream& o, const NodeP& n, const NodeP& parent, size_t idx) {
    o << "<li><span class=\"" << cssClass(n->kind) << "\">" << htmlEscape(nodeLabel(n, parent, idx)) << "</span>";
    bool any = false;
    for (size_t i = 0; i < n->kids.size(); i++) if (!hidden(n, i)) any = true;
    if (any) {
        o << "<ul>";
        for (size_t i = 0; i < n->kids.size(); i++)
            if (!hidden(n, i)) htmlNode(o, n->kids[i], n, i);
        o << "</ul>";
    }
    o << "</li>";
}

// Parse tree er CSS (--html ar --report dutoi use kore)
const char* TREE_CSS = R"CSS(
.wrap{overflow-x:auto;padding-bottom:10px}
.tree{display:inline-block;min-width:100%}
.tree ul{padding-top:20px;position:relative;display:flex;justify-content:center;margin:0}
.tree>ul{padding-top:0}
.tree li{list-style:none;text-align:center;position:relative;padding:20px 5px 0 5px}
.tree li::before,.tree li::after{content:'';position:absolute;top:0;right:50%;border-top:2px solid #888;width:50%;height:20px}
.tree li::after{right:auto;left:50%;border-left:2px solid #888}
.tree li:only-child::after,.tree li:only-child::before{display:none}
.tree li:only-child{padding-top:0}
.tree>ul>li{padding-top:0}
.tree>ul>li::before,.tree>ul>li::after{display:none}
.tree li:first-child::before,.tree li:last-child::after{border:0 none}
.tree li:last-child::before{border-right:2px solid #888;border-radius:0 6px 0 0}
.tree li:first-child::after{border-radius:6px 0 0 0}
.tree ul ul::before{content:'';position:absolute;top:0;left:50%;border-left:2px solid #888;width:0;height:20px}
.tree span{display:inline-block;padding:6px 10px;border:2px solid;border-radius:8px;font:14px Consolas,monospace;white-space:nowrap}
.stmt{background:#dbeafe;border-color:#3b82f6}
.call{background:#dcfce7;border-color:#22c55e}
.lit{background:#ffedd5;border-color:#f97316}
.var{background:#f3e8ff;border-color:#a855f7}
.op{background:#fee2e2;border-color:#ef4444}
.legend{text-align:center;margin-bottom:10px}
.legend span{margin:0 6px;padding:3px 8px;border:2px solid;border-radius:6px;font-size:13px}
)CSS";

const char* LEGEND_HTML = R"(<div class="legend">
<span class="stmt">Statement</span><span class="call">Function call</span><span class="lit">Value</span><span class="var">Variable</span><span class="op">Operator</span></div>)";

void writeHtml(const NodeP& root, const string& filename) {
    ofstream o(filename);
    o << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>CompileX Parse Tree</title><style>"
      << "body{font-family:Segoe UI,Arial,sans-serif;background:#f5f6fa;margin:20px}h2{text-align:center;color:#2c3e50}"
      << TREE_CSS << "</style></head><body><h2>CompileX - Parse Tree (AST)</h2>" << LEGEND_HTML
      << "<div class=\"wrap\"><div class=\"tree\"><ul>";
    htmlNode(o, root, nullptr, 0);
    o << "</ul></div></div></body></html>";
}

// ===============================================================
// REPL : file chhara nijer hate type kore chalano
// ===============================================================
int countBlocks(const vector<Token>& tokens) {      // koyta block ekhono 'sesh' hoy ni
    int depth = 0;
    for (auto& t : tokens) {
        if (t.type != TT::Keyword) continue;
        if (t.text == "jodi" || t.text == "jotokhon") depth++;
        if (t.text == "sesh") depth--;
    }
    return depth;
}

void repl() {
    Interpreter interp;                             // variable gulo mone rakhe
    cout << "CompileX interactive mode  (bondho korte: exit)\n";
    string buffer;

    while (true) {
        cout << (buffer.empty() ? "cx> " : "... ") << flush;
        string line;
        if (!getline(cin, line)) break;
        if (buffer.empty() && line == "exit") break;
        buffer += line + "\n";

        try {
            vector<Token> tokens = lexer(buffer);
            if (countBlocks(tokens) > 0) continue;  // jodi/jotokhon block shesh hoy ni, aro line nao
            NodeP ast = Parser(tokens).program();
            buffer.clear();

            for (auto& stmt : ast->kids) {
                if (stmt->kind == NK::ExprStmt) {   // jog(2,3) likhle 5 dekhabe
                    Value v = interp.eval(stmt->kids[0]);
                    if (v.type != VT::Nil) cout << show(v, true) << "\n";
                } else {
                    interp.exec(stmt);
                }
            }
        } catch (const CompileXError& e) {
            buffer.clear();
            cout << "Error";
            if (e.line) cout << " (line " << e.line << ")";
            cout << ": " << e.message << "\n";
        }
    }
}

// ===============================================================
// REPORT : proti step er Source + Token + Parse Tree + Output (HTML)
// ===============================================================
string trimStr(const string& s) {
    size_t a = s.find_first_not_of(" \t\r");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

string tokenMeaning(const vector<Token>& t, size_t i) {
    const Token& k = t[i];
    switch (k.type) {
        case TT::Keyword: return "Keyword (language er reserved shobdo)";
        case TT::Number:  return "Number (sonkha)";
        case TT::String:  return "Text (string)";
        case TT::End:     return "File shesh";
        case TT::Name:
            if (t[i + 1].type == TT::Op && t[i + 1].text == "(") return "Function er nam";
            return "Variable er nam";
        case TT::Op:
            if (k.text == "(") return "Bracket khola (argument shuru)";
            if (k.text == ")") return "Bracket bondho";
            if (k.text == ",") return "Argument alada kore";
            if (k.text == "=") return "Man deoa (assign)";
            if (k.text == "[") return "List shuru";
            if (k.text == "]") return "List shesh";
            if (k.text == "==" || k.text == "!=" || k.text == "<" || k.text == ">" ||
                k.text == "<=" || k.text == ">=") return "Tulona operator";
            return "Gonit operator";
    }
    return "";
}

void writeReport(const string& source, const string& filename) {
    Interpreter interp;
    stringstream fakeInput;                                  // nao() er jonno nokol input
    streambuf* realIn = cin.rdbuf(fakeInput.rdbuf());

    vector<pair<string, string>> toc;                        // (id, title)
    ostringstream body;
    string title, note, inputText, buffer;
    istringstream lines(source);
    string raw;

    while (getline(lines, raw)) {
        string line = trimStr(raw);
        if (buffer.empty()) {
            if (line.empty()) continue;
            if (line[0] == '#') {                            // # title: / # note: / # input:
                string c = trimStr(line.substr(1));
                if (c.rfind("title:", 0) == 0) title = trimStr(c.substr(6));
                else if (c.rfind("note:", 0) == 0) note = trimStr(c.substr(5));
                else if (c.rfind("input:", 0) == 0) inputText = trimStr(c.substr(6));
                continue;
            }
        }
        buffer += raw + "\n";

        vector<Token> tokens;
        NodeP ast;
        string errText, outText, resultText;
        try {
            tokens = lexer(buffer);
            if (countBlocks(tokens) > 0) continue;           // block ekhono shesh hoy ni
            ast = Parser(tokens).program();
        } catch (const CompileXError& e) {
            errText = "Error (line " + to_string(e.line) + "): " + e.message;
        }

        if (ast) {                                           // Phase 3: chalao, output dhore rakho
            cin.clear();
            fakeInput.clear();
            fakeInput.str(inputText.empty() ? "" : inputText + "\n");
            ostringstream cap;
            streambuf* realOut = cout.rdbuf(cap.rdbuf());
            try {
                for (auto& stmt : ast->kids) {
                    if (stmt->kind == NK::ExprStmt) {
                        Value v = interp.eval(stmt->kids[0]);
                        if (v.type != VT::Nil) resultText = show(v, true);
                    } else {
                        interp.exec(stmt);
                    }
                }
            } catch (const CompileXError& e) {
                errText = "Error (line " + to_string(e.line) + "): " + e.message;
            }
            cout.rdbuf(realOut);
            outText = cap.str();
        }

        if (!title.empty()) {                                // shudhu title wala step dekhabe
            string id = "s" + to_string(toc.size() + 1);
            toc.push_back({id, title});
            string src = buffer;
            while (!src.empty() && (src.back() == '\n' || src.back() == '\r')) src.pop_back();

            body << "<section class=\"card\" id=\"" << id << "\"><h2>" << htmlEscape(title) << "</h2>";
            if (!note.empty()) body << "<p class=\"note\">" << htmlEscape(note) << "</p>";

            body << "<h3><span class=\"step\">1</span> Source code (input)</h3><pre>" << htmlEscape(src) << "</pre>";
            if (!inputText.empty())
                body << "<p class=\"kb\">Keyboard theke input: <b>" << htmlEscape(inputText) << "</b></p>";

            size_t shownTokens = tokens.empty() ? 0 : tokens.size() - 1;     // EOF bad
            body << "<h3><span class=\"step\">2</span> Lexer &rarr; Token (" << shownTokens
                 << " ta)</h3><table class=\"tok\"><tr><th>#</th><th>Token type</th><th>Lexeme</th><th>Mane</th></tr>";
            for (size_t i = 0; i < shownTokens; i++) {
                const Token& t = tokens[i];
                const char* tn[] = {"NUMBER", "STRING", "NAME", "KEYWORD", "OP", "EOF"};
                string lex = t.type == TT::String ? "\"" + t.text + "\"" : t.text;
                body << "<tr><td>" << i + 1 << "</td><td><span class=\"tk tk-" << tn[(int)t.type] << "\">"
                     << tn[(int)t.type] << "</span></td><td><code>" << htmlEscape(lex) << "</code></td><td>"
                     << htmlEscape(tokenMeaning(tokens, i)) << "</td></tr>";
            }
            body << "</table>";

            if (ast) {
                body << "<h3><span class=\"step\">3</span> Parser &rarr; Parse Tree (AST)</h3>" << LEGEND_HTML
                     << "<div class=\"wrap\"><div class=\"tree\"><ul>";
                htmlNode(body, ast, nullptr, 0);
                body << "</ul></div></div>";
            }

            body << "<h3><span class=\"step\">4</span> Interpreter &rarr; Output</h3>";
            if (!outText.empty()) body << "<pre class=\"out\">" << htmlEscape(outText) << "</pre>";
            if (!resultText.empty())
                body << "<p class=\"res\">Result: <b>" << htmlEscape(resultText) << "</b></p>";
            if (!errText.empty()) body << "<p class=\"err\">" << htmlEscape(errText) << "</p>";
            auto vars = interp.variables();
            if (!vars.empty()) {
                body << "<p class=\"vars\">Variable gulo ekhon: ";
                for (auto& v : vars)
                    body << "<span class=\"chip\">" << htmlEscape(v.first) << " = " << htmlEscape(v.second) << "</span> ";
                body << "</p>";
            }
            body << "</section>";
        }
        title.clear(); note.clear(); inputText.clear(); buffer.clear();
    }
    cin.rdbuf(realIn);

    ofstream o(filename);
    o << R"(<!DOCTYPE html><html><head><meta charset="utf-8"><title>CompileX Report</title><style>
body{font-family:Segoe UI,Arial,sans-serif;background:#f5f6fa;margin:0;color:#1f2937}
header{background:#1e3a5f;color:#fff;padding:24px;text-align:center}
header h1{margin:0 0 6px}header p{margin:0;opacity:.85}
main{max-width:1100px;margin:20px auto;padding:0 16px}
.phases{display:flex;gap:10px;justify-content:center;flex-wrap:wrap;margin:18px 0}
.phase{background:#fff;border:2px solid #cbd5e1;border-radius:10px;padding:10px 16px;text-align:center;min-width:200px}
.phase b{display:block;color:#1e3a5f}.arrow{align-self:center;font-size:22px;color:#64748b}
.toc{background:#fff;border-radius:10px;padding:12px 16px;margin-bottom:20px}
.toc a{display:inline-block;margin:3px;padding:4px 10px;background:#e0e7ff;border-radius:14px;text-decoration:none;color:#1e3a8a;font-size:14px}
.card{background:#fff;border-radius:12px;padding:18px 22px;margin-bottom:26px;box-shadow:0 2px 8px rgba(0,0,0,.08)}
.card h2{margin:0 0 6px;color:#1e3a5f;border-bottom:3px solid #3b82f6;padding-bottom:6px}
.note{background:#fef9c3;border-left:4px solid #eab308;padding:8px 12px;margin:10px 0;border-radius:4px}
h3{margin:18px 0 8px;font-size:16px;color:#334155}
.step{display:inline-block;background:#3b82f6;color:#fff;border-radius:50%;width:22px;height:22px;text-align:center;line-height:22px;font-size:13px;margin-right:6px}
pre{background:#0f172a;color:#e2e8f0;padding:10px 14px;border-radius:8px;font:14px Consolas,monospace;margin:0;overflow-x:auto}
pre.out{background:#052e16;color:#bbf7d0}
.kb,.res,.vars{margin:8px 0}.err{color:#b91c1c;font-weight:bold}
.chip{display:inline-block;background:#f1f5f9;border:1px solid #cbd5e1;border-radius:12px;padding:2px 9px;font:13px Consolas,monospace}
table.tok{border-collapse:collapse;width:100%;font-size:14px}
table.tok th,table.tok td{border:1px solid #e2e8f0;padding:5px 9px;text-align:left}
table.tok th{background:#f1f5f9}
.tk{padding:2px 8px;border-radius:10px;font:12px Consolas,monospace;color:#fff}
.tk-NAME{background:#16a34a}.tk-NUMBER{background:#ea580c}.tk-STRING{background:#d97706}
.tk-KEYWORD{background:#2563eb}.tk-OP{background:#dc2626}
@media print{.card{page-break-inside:avoid;box-shadow:none;border:1px solid #ccc}}
)" << TREE_CSS << R"(</style></head><body>
<header><h1>CompileX (Code Lipi) - Token ar Parse Tree Report</h1><p>UITS Compiler Lab &middot; proti function er Lexer, Parser, Interpreter output</p></header><main>
<div class="phases">
<div class="phase"><b>1. Lexer</b>code ke token e bhag kore</div><div class="arrow">&rarr;</div>
<div class="phase"><b>2. Parser</b>token theke parse tree banay</div><div class="arrow">&rarr;</div>
<div class="phase"><b>3. Interpreter</b>tree walk kore output dey</div></div>
<div class="toc"><b>Function list:</b><br>)";
    for (auto& e : toc) o << "<a href=\"#" << e.first << "\">" << htmlEscape(e.second) << "</a>";
    o << "</div>" << body.str() << "</main></body></html>";
}

// ===============================================================
// MAIN : file pore 3 ta phase chalay
// ===============================================================
int main(int argc, char* argv[]) {
    if (argc < 2 || string(argv[1]) == "--repl") {       // file nai -> interactive mode
        repl();
        return 0;
    }

    ifstream file(argv[1]);
    if (!file) {
        cout << "File khola jay ni: " << argv[1] << "\n";
        return 1;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    string source = buffer.str();

    bool showTokens = false, showAst = false, showHtml = false, showReport = false;
    for (int i = 2; i < argc; i++) {
        if (string(argv[i]) == "--tokens") showTokens = true;
        if (string(argv[i]) == "--ast") showAst = true;
        if (string(argv[i]) == "--html") showHtml = true;
        if (string(argv[i]) == "--report") showReport = true;
    }

    try {
        if (showReport) {                                  // proti step er Token + Tree + Output
            writeReport(source, "report.html");
            cout << "report.html toiri hoyeche. Browser e kholo.\n";
#ifdef _WIN32
            system("start report.html");
#endif
            return 0;
        }
        vector<Token> tokens = lexer(source);              // Phase 1
        if (showTokens) { printTokens(tokens); return 0; }

        NodeP ast = Parser(tokens).program();              // Phase 2
        if (showAst) { cout << "=== AST ===\n"; printTree(ast, nullptr, 0, "", true, true); return 0; }
        if (showHtml) {
            writeHtml(ast, "ast.html");
            cout << "ast.html toiri hoyeche. Browser e kholo.\n";
#ifdef _WIN32
            system("start ast.html");                       // Windows e nijei khule jabe
#endif
            return 0;
        }

        Interpreter().exec(ast);                           // Phase 3
    } catch (const CompileXError& e) {
        cout << "Error";
        if (e.line) cout << " (line " << e.line << ")";
        cout << ": " << e.message << "\n";
        return 1;
    }
    return 0;
}
