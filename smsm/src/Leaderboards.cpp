#include "Leaderboards.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace Leaderboards {

namespace {
constexpr const wchar_t* Host = L"board.portal2.sr";

struct Board {
    State state = State::Idle;
    std::vector<Entry> entries;
};

std::mutex mutex;
std::map<int, Board> boards;
std::deque<int> queue;
std::thread worker;
bool working = false;
std::atomic<bool> stopping { false };

// Just enough JSON for board.portal2.sr's responses.
struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    double number = 0;
    std::string string;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    const Json* Find(const char* key) const
    {
        for (auto& member : this->object) {
            if (member.first == key)
                return &member.second;
        }
        return nullptr;
    }
};

class Parser {
public:
    Parser(const char* begin, const char* end)
        : p(begin)
        , end(end)
    {
    }

    bool Parse(Json& out)
    {
        out = this->Value();
        this->Space();
        return this->ok && this->p == this->end;
    }

private:
    void Space()
    {
        while (this->p < this->end && (*this->p == ' ' || *this->p == '\t' || *this->p == '\n' || *this->p == '\r'))
            ++this->p;
    }

    bool Consume(char c)
    {
        this->Space();
        if (this->p < this->end && *this->p == c) {
            ++this->p;
            return true;
        }
        return false;
    }

    bool Literal(const char* word)
    {
        const char* q = this->p;
        for (; *word; ++word, ++q) {
            if (q >= this->end || *q != *word)
                return false;
        }
        this->p = q;
        return true;
    }

    static void AppendUtf8(std::string& out, unsigned int code)
    {
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | code >> 6);
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | code >> 12);
            out += static_cast<char>(0x80 | (code >> 6 & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | code >> 18);
            out += static_cast<char>(0x80 | (code >> 12 & 0x3F));
            out += static_cast<char>(0x80 | (code >> 6 & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    unsigned int Hex4()
    {
        if (this->end - this->p < 4) {
            this->ok = false;
            return 0;
        }
        char digits[5] = { this->p[0], this->p[1], this->p[2], this->p[3], 0 };
        this->p += 4;
        return static_cast<unsigned int>(std::strtoul(digits, nullptr, 16));
    }

    std::string String()
    {
        std::string out;
        if (!this->Consume('"')) {
            this->ok = false;
            return out;
        }
        while (this->p < this->end && *this->p != '"') {
            char c = *this->p++;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (this->p >= this->end)
                break;
            switch (c = *this->p++) {
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned int code = this->Hex4();
                if (code >= 0xD800 && code < 0xDC00 && this->end - this->p >= 6 && this->p[0] == '\\' && this->p[1] == 'u') {
                    this->p += 2;
                    code = 0x10000 + ((code - 0xD800) << 10) + (this->Hex4() - 0xDC00);
                }
                AppendUtf8(out, code);
                break;
            }
            default: out += c; break; // \" \\ \/
            }
        }
        if (this->p >= this->end)
            this->ok = false;
        else
            ++this->p;
        return out;
    }

    Json Value()
    {
        Json value;
        this->Space();
        if (this->p >= this->end) {
            this->ok = false;
        } else if (*this->p == '{') {
            ++this->p;
            value.type = Json::Type::Object;
            if (!this->Consume('}')) {
                do {
                    std::string key = this->String();
                    if (!this->Consume(':')) {
                        this->ok = false;
                        break;
                    }
                    value.object.emplace_back(std::move(key), this->Value());
                } while (this->ok && this->Consume(','));
                if (!this->Consume('}'))
                    this->ok = false;
            }
        } else if (*this->p == '[') {
            ++this->p;
            value.type = Json::Type::Array;
            if (!this->Consume(']')) {
                do {
                    value.array.push_back(this->Value());
                } while (this->ok && this->Consume(','));
                if (!this->Consume(']'))
                    this->ok = false;
            }
        } else if (*this->p == '"') {
            value.type = Json::Type::String;
            value.string = this->String();
        } else if (this->Literal("true")) {
            value.type = Json::Type::Bool;
            value.number = 1;
        } else if (this->Literal("false")) {
            value.type = Json::Type::Bool;
        } else if (this->Literal("null")) {
            value.type = Json::Type::Null;
        } else {
            char* stop = nullptr;
            value.type = Json::Type::Number;
            value.number = std::strtod(this->p, &stop);
            if (stop == this->p)
                this->ok = false;
            this->p = stop;
        }
        return value;
    }

    const char* p;
    const char* end;
    bool ok = true;
};

int Number(const Json* json)
{
    return json && json->type == Json::Type::Number ? static_cast<int>(json->number) : 0;
}

// board.portal2.sr/chamber/<id>/json: { "<steamid64>": { "scoreData": {...}, "userData": {...} }, ... }
bool ParseBoard(const std::string& body, std::vector<Entry>& entries)
{
    Json root;
    if (!Parser(body.data(), body.data() + body.size()).Parse(root) || root.type != Json::Type::Object)
        return false;

    for (auto& member : root.object) {
        const Json* score = member.second.Find("scoreData");
        const Json* user = member.second.Find("userData");
        if (!score || !user)
            continue;
        const Json* name = user->Find("boardname");
        Entry entry;
        entry.steamId = std::strtoull(member.first.c_str(), nullptr, 10);
        entry.name = name && name->type == Json::Type::String ? name->string : member.first;
        entry.centiseconds = Number(score->Find("score"));
        entry.rank = Number(score->Find("scoreRank"));
        entries.push_back(std::move(entry));
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.rank < b.rank; });
    return true;
}

#ifdef _WIN32
bool HttpGet(const std::wstring& path, std::string& body)
{
    bool success = false;
    HINTERNET session = WinHttpOpen(L"SMSM", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connection = session ? WinHttpConnect(session, Host, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (request) {
        WinHttpSetTimeouts(request, 5000, 5000, 10000, 10000);
        DWORD status = 0, size = sizeof(status);
        if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            && WinHttpReceiveResponse(request, nullptr)
            && WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)
            && status == 200) {
            DWORD available = 0;
            success = true;
            while (WinHttpQueryDataAvailable(request, &available) && available) {
                const size_t offset = body.size();
                body.resize(offset + available);
                DWORD read = 0;
                if (!WinHttpReadData(request, &body[offset], available, &read)) {
                    success = false;
                    break;
                }
                body.resize(offset + read);
            }
        }
    }
    if (request)
        WinHttpCloseHandle(request);
    if (connection)
        WinHttpCloseHandle(connection);
    if (session)
        WinHttpCloseHandle(session);
    return success;
}
#else
// libcurl, loaded at runtime rather than linked: the Steam runtime the game runs in ships a 32-bit one.
struct Curl {
    using EasyInit = void* (*)();
    using EasySetOpt = int (*)(void* curl, int option, ...);
    using EasyPerform = int (*)(void* curl);
    using EasyGetInfo = int (*)(void* curl, int info, ...);
    using EasyCleanup = void (*)(void* curl);

    static constexpr int OptWriteData = 10001;
    static constexpr int OptUrl = 10002;
    static constexpr int OptCaInfo = 10065;
    static constexpr int OptWriteFunction = 20011;
    static constexpr int OptTimeout = 13;
    static constexpr int OptConnectTimeout = 78;
    static constexpr int OptNoSignal = 99;
    static constexpr int InfoResponseCode = 0x200002;

    EasyInit init = nullptr;
    EasySetOpt setOpt = nullptr;
    EasyPerform perform = nullptr;
    EasyGetInfo getInfo = nullptr;
    EasyCleanup cleanup = nullptr;
    const char* caBundle = nullptr;

    Curl()
    {
        void* library = dlopen("libcurl.so.4", RTLD_NOW);
        if (!library)
            library = dlopen("libcurl-gnutls.so.4", RTLD_NOW);
        if (!library)
            return;
        init = reinterpret_cast<EasyInit>(dlsym(library, "curl_easy_init"));
        setOpt = reinterpret_cast<EasySetOpt>(dlsym(library, "curl_easy_setopt"));
        perform = reinterpret_cast<EasyPerform>(dlsym(library, "curl_easy_perform"));
        getInfo = reinterpret_cast<EasyGetInfo>(dlsym(library, "curl_easy_getinfo"));
        cleanup = reinterpret_cast<EasyCleanup>(dlsym(library, "curl_easy_cleanup"));
        // The runtime's libcurl looks for certificates where Ubuntu keeps them; other distros don't.
        for (const char* path : { "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/ca-bundle.pem", "/etc/ssl/cert.pem" }) {
            if (access(path, R_OK) == 0) {
                caBundle = path;
                break;
            }
        }
    }

    bool Available() const { return init && setOpt && perform && getInfo && cleanup; }

    static size_t Write(char* data, size_t size, size_t count, void* body)
    {
        static_cast<std::string*>(body)->append(data, size * count);
        return size * count;
    }
};

bool HttpGet(const std::wstring& path, std::string& body)
{
    static const Curl curl;
    void* request = curl.Available() ? curl.init() : nullptr;
    if (!request)
        return false;
    std::string url = "https://";
    for (const wchar_t* c = Host; *c; ++c)
        url += static_cast<char>(*c);
    for (wchar_t c : path)
        url += static_cast<char>(c);

    curl.setOpt(request, Curl::OptUrl, url.c_str());
    curl.setOpt(request, Curl::OptWriteFunction, &Curl::Write);
    curl.setOpt(request, Curl::OptWriteData, &body);
    curl.setOpt(request, Curl::OptConnectTimeout, 5L);
    curl.setOpt(request, Curl::OptTimeout, 15L);
    curl.setOpt(request, Curl::OptNoSignal, 1L); // it runs on the worker thread
    if (curl.caBundle)
        curl.setOpt(request, Curl::OptCaInfo, curl.caBundle);
    long status = 0;
    const bool success = curl.perform(request) == 0 && curl.getInfo(request, Curl::InfoResponseCode, &status) == 0 && status == 200;
    curl.cleanup(request);
    return success;
}
#endif

void Work()
{
    while (!stopping) {
        int chamberId;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.empty()) {
                working = false;
                return;
            }
            chamberId = queue.front();
            queue.pop_front();
            boards[chamberId].state = State::Loading;
        }

        std::string body;
        std::vector<Entry> entries;
        const bool success = HttpGet(L"/chamber/" + std::to_wstring(chamberId) + L"/json", body) && ParseBoard(body, entries);

        std::lock_guard<std::mutex> lock(mutex);
        Board& board = boards[chamberId];
        board.state = success ? State::Ready : State::Failed;
        board.entries = std::move(entries);
    }
    std::lock_guard<std::mutex> lock(mutex);
    working = false;
}

// Call with the mutex held.
void StartWorker()
{
    if (working || stopping || queue.empty())
        return;
    if (worker.joinable())
        worker.join(); // already finished, since working is false
    working = true;
    worker = std::thread(Work);
}

// Call with the mutex held.
bool Wanted(int chamberId)
{
    auto found = boards.find(chamberId);
    return found == boards.end() || found->second.state == State::Idle || found->second.state == State::Failed;
}
}

void Prefetch(const std::vector<int>& chamberIds)
{
    std::lock_guard<std::mutex> lock(mutex);
    for (int chamberId : chamberIds) {
        if (Wanted(chamberId) && std::find(queue.begin(), queue.end(), chamberId) == queue.end())
            queue.push_back(chamberId);
    }
    StartWorker();
}

void Request(int chamberId)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!Wanted(chamberId))
        return;
    queue.erase(std::remove(queue.begin(), queue.end(), chamberId), queue.end());
    queue.push_front(chamberId);
    boards[chamberId].state = State::Idle; // retrying a failed board
    StartWorker();
}

State Get(int chamberId, std::vector<Entry>& entries)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto found = boards.find(chamberId);
    if (found == boards.end())
        return State::Idle;
    if (found->second.state == State::Ready)
        entries = found->second.entries;
    return found->second.state;
}

void Shutdown()
{
    stopping = true;
    if (worker.joinable())
        worker.join(); // at most one request in flight, bounded by the HTTP timeouts
    std::lock_guard<std::mutex> lock(mutex);
    queue.clear();
    boards.clear();
    working = false;
    stopping = false;
}

}
