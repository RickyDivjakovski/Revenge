#include "revenge/launch.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
namespace revenge {
namespace {
std::string lower(std::string s) { for (auto& c : s) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string filename_lower(const std::filesystem::path& p) {
    auto s=p.filename().u8string(); return lower(std::string(s.begin(),s.end()));
}
}
bool has_console_markers(const Image& image, const std::filesystem::path& path) {
    if(filename_lower(path).find("scarlett")!=std::string::npos) return true;
    for (const auto& section:image.sections) if(lower(section.name)==".xbld") return true;
    for (const auto& dep:image.imports) {
        const auto n=lower(dep.dll);
        for (std::string_view marker:{"d3d12_x.dll","d3d12_xs.dll","xg_x.dll","xg_xs.dll","xmem.dll","xboxbvhbuilder.dll","xcrdapi.dll","chad.dll","xapu.dll","acphal.dll","bink2scarlett.dll","amd_fidelityfx_loader_xbox.dll"})
            if(n==marker) return true;
    }
    return false;
}
std::vector<std::filesystem::path> game_candidates(const std::filesystem::path& directory) {
    if(!std::filesystem::is_directory(directory)) throw std::runtime_error("Select an existing game directory.");
    std::vector<std::filesystem::path> result;
    // Deliberately search only the selected directory. Avoid installers and tools in subdirectories.
    for(const auto& entry:std::filesystem::directory_iterator(directory)) {
        if(entry.is_regular_file() && !entry.is_symlink() && filename_lower(entry.path().extension())==".exe") result.push_back(entry.path());
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return filename_lower(a)<filename_lower(b);});
    return result;
}
}
