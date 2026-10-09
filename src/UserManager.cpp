#include "UserManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
const unsigned int kIterations = 12000;

class Sha256 {
public:
    Sha256() { reset(); }

    void update(const unsigned char* input, std::size_t length) {
        for (std::size_t i = 0; i < length; ++i) {
            block_[used_++] = input[i];
            if (used_ == 64) {
                transform(block_);
                bitLength_ += 512;
                used_ = 0;
            }
        }
    }

    std::vector<unsigned char> finish() {
        std::size_t i = used_;
        if (used_ < 56) {
            block_[i++] = 0x80;
            while (i < 56) block_[i++] = 0;
        } else {
            block_[i++] = 0x80;
            while (i < 64) block_[i++] = 0;
            transform(block_);
            block_.fill(0);
        }
        bitLength_ += static_cast<std::uint64_t>(used_) * 8;
        for (unsigned int j = 0; j < 8; ++j)
            block_[63 - j] = static_cast<unsigned char>(bitLength_ >> (j * 8));
        transform(block_);
        std::vector<unsigned char> result(32);
        for (unsigned int j = 0; j < 8; ++j) {
            result[j * 4] = static_cast<unsigned char>(state_[j] >> 24);
            result[j * 4 + 1] = static_cast<unsigned char>(state_[j] >> 16);
            result[j * 4 + 2] = static_cast<unsigned char>(state_[j] >> 8);
            result[j * 4 + 3] = static_cast<unsigned char>(state_[j]);
        }
        return result;
    }

private:
    std::array<unsigned char, 64> block_{};
    std::array<std::uint32_t, 8> state_{};
    std::size_t used_ = 0;
    std::uint64_t bitLength_ = 0;

    static std::uint32_t rotate(std::uint32_t value, unsigned int amount) {
        return (value >> amount) | (value << (32 - amount));
    }

    void reset() {
        state_ = {{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    }

    void transform(const std::array<unsigned char, 64>& data) {
        static const std::uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        std::uint32_t words[64];
        for (unsigned int i = 0; i < 16; ++i)
            words[i] = (static_cast<std::uint32_t>(data[i*4]) << 24) |
                       (static_cast<std::uint32_t>(data[i*4+1]) << 16) |
                       (static_cast<std::uint32_t>(data[i*4+2]) << 8) | data[i*4+3];
        for (unsigned int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotate(words[i-15],7) ^ rotate(words[i-15],18) ^ (words[i-15] >> 3);
            const std::uint32_t s1 = rotate(words[i-2],17) ^ rotate(words[i-2],19) ^ (words[i-2] >> 10);
            words[i] = words[i-16] + s0 + words[i-7] + s1;
        }
        std::uint32_t a=state_[0], b=state_[1], c=state_[2], d=state_[3];
        std::uint32_t e=state_[4], f=state_[5], g=state_[6], h=state_[7];
        for (unsigned int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotate(e,6) ^ rotate(e,11) ^ rotate(e,25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + choice + k[i] + words[i];
            const std::uint32_t s0 = rotate(a,2) ^ rotate(a,13) ^ rotate(a,22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + majority;
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        state_[0]+=a; state_[1]+=b; state_[2]+=c; state_[3]+=d;
        state_[4]+=e; state_[5]+=f; state_[6]+=g; state_[7]+=h;
    }
};

std::vector<unsigned char> sha256(const std::vector<unsigned char>& data) {
    Sha256 hash;
    if (!data.empty()) hash.update(data.data(), data.size());
    return hash.finish();
}

std::vector<unsigned char> hmacSha256(const std::string& password,
                                      const std::vector<unsigned char>& message) {
    std::vector<unsigned char> key(password.begin(), password.end());
    if (key.size() > 64) key = sha256(key);
    key.resize(64, 0);
    std::vector<unsigned char> inner(64), outer(64);
    for (std::size_t i = 0; i < 64; ++i) {
        inner[i] = static_cast<unsigned char>(key[i] ^ 0x36);
        outer[i] = static_cast<unsigned char>(key[i] ^ 0x5c);
    }
    inner.insert(inner.end(), message.begin(), message.end());
    const std::vector<unsigned char> innerHash = sha256(inner);
    outer.insert(outer.end(), innerHash.begin(), innerHash.end());
    return sha256(outer);
}

std::vector<unsigned char> pbkdf2(const std::string& password,
                                  const std::vector<unsigned char>& salt) {
    std::vector<unsigned char> block = salt;
    block.insert(block.end(), {0, 0, 0, 1});
    std::vector<unsigned char> result = hmacSha256(password, block);
    std::vector<unsigned char> previous = result;
    for (unsigned int i = 1; i < kIterations; ++i) {
        previous = hmacSha256(password, previous);
        for (std::size_t j = 0; j < result.size(); ++j) result[j] ^= previous[j];
    }
    return result;
}

std::string hex(const std::vector<unsigned char>& bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) out << std::setw(2) << static_cast<unsigned int>(byte);
    return out.str();
}

std::vector<unsigned char> unhex(const std::string& value) {
    if (value.size() % 2) throw std::invalid_argument("Invalid stored credential.");
    std::vector<unsigned char> bytes;
    for (std::size_t i = 0; i < value.size(); i += 2)
        bytes.push_back(static_cast<unsigned char>(std::stoul(value.substr(i, 2), nullptr, 16)));
    return bytes;
}

std::string encode(const std::string& value) {
    return hex(std::vector<unsigned char>(value.begin(), value.end()));
}

std::string decode(const std::string& value) {
    const std::vector<unsigned char> bytes = unhex(value);
    return std::string(bytes.begin(), bytes.end());
}

std::string normalize(const std::string& username) {
    std::string result = username;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::string makeRandomHex(std::size_t bytes) {
    std::random_device source;
    std::vector<unsigned char> value(bytes);
    for (unsigned char& byte : value) byte = static_cast<unsigned char>(source());
    return hex(value);
}

bool constantTimeEqual(const std::vector<unsigned char>& left,
                       const std::vector<unsigned char>& right) {
    if (left.size() != right.size()) return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < left.size(); ++i) difference |= left[i] ^ right[i];
    return difference == 0;
}
}

UserManager::UserManager(const std::string& filePath) : filePath_(filePath) { load(); }

void UserManager::load() {
    std::ifstream input(filePath_.c_str());
    std::string username, fullName, salt, passwordHash;
    while (std::getline(input, username, '|')) {
        if (!std::getline(input, fullName, '|') || !std::getline(input, salt, '|') ||
            !std::getline(input, passwordHash)) break;
        try {
            AuthUser user{decode(fullName), decode(username)};
            users_[normalize(user.username)] = UserRecord{user, salt, passwordHash};
        } catch (...) {
            // Ignore malformed credential rows; never treat them as authenticated users.
        }
    }
}

void UserManager::save() const {
    std::ofstream output(filePath_.c_str(), std::ios::trunc);
    if (!output) throw std::runtime_error("Could not save local user accounts.");
    for (const auto& entry : users_) {
        const UserRecord& record = entry.second;
        output << encode(record.user.username) << '|' << encode(record.user.fullName) << '|'
               << record.salt << '|' << record.passwordHash << '\n';
    }
    if (!output) throw std::runtime_error("Could not write local user accounts.");
}

AuthUser UserManager::registerUser(const std::string& fullName, const std::string& username,
                                   const std::string& password) {
    if (fullName.empty() || fullName.size() > 100)
        throw std::invalid_argument("Enter a name up to 100 characters.");
    if (username.size() < 3 || username.size() > 254)
        throw std::invalid_argument("Username or email must be between 3 and 254 characters.");
    const bool emailLike = username.find('@') != std::string::npos;
    if ((emailLike && (username.find('@') == 0 || username.find('@') != username.rfind('@') ||
                       username.find('.') == std::string::npos)) ||
        (!emailLike && !std::all_of(username.begin(), username.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';
        })))
        throw std::invalid_argument("Enter a valid username or email address.");
    if (password.size() < 8 || password.size() > 128)
        throw std::invalid_argument("Password must be between 8 and 128 characters.");
    const std::string key = normalize(username);
    if (users_.find(key) != users_.end()) throw std::logic_error("Username or email is already registered.");
    const std::string salt = makeRandomHex(16);
    UserRecord record{AuthUser{fullName, username}, salt,
                      hex(pbkdf2(password, unhex(salt)))};
    users_[key] = record;
    try { save(); }
    catch (...) { users_.erase(key); throw; }
    return record.user;
}

AuthSession UserManager::login(const std::string& username, const std::string& password) {
    const auto found = users_.find(normalize(username));
    if (found == users_.end() ||
        !constantTimeEqual(pbkdf2(password, unhex(found->second.salt)),
                           unhex(found->second.passwordHash)))
        throw std::invalid_argument("Invalid username/email or password.");
    const std::string token = makeRandomHex(32);
    sessions_[token] = found->second.user;
    return AuthSession{found->second.user, token};
}

bool UserManager::logout(const std::string& token) { return sessions_.erase(token) != 0; }

bool UserManager::findSession(const std::string& token, AuthUser& user) const {
    const auto found = sessions_.find(token);
    if (found == sessions_.end()) return false;
    user = found->second;
    return true;
}
