#pragma once
#include "../core/Types.h"
#include <vector>
#include <string>

namespace rfps {

class ProfileManager {
public:
    ProfileManager();
    bool LoadFromDisk(const std::wstring& filePath);
    bool SaveToDisk(const std::wstring& filePath) const;

    std::vector<Profile>& GetProfiles() { return m_profiles; }
    void AddOrUpdateProfile(const Profile& profile);
    Profile FindForProcess(const std::wstring& processName) const;

private:
    std::vector<Profile> m_profiles;
};

} // namespace rfps