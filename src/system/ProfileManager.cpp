#include "ProfileManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace rfps {

ProfileManager::ProfileManager() {
    Profile def;
    def.name = L"Global Default";
    m_profiles.push_back(def);
}

bool ProfileManager::LoadFromDisk(const std::wstring& filePath) {
    QFile file(QString::fromStdWString(filePath));
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return false;

    m_profiles.clear();
    for (const auto& val : doc.array()) {
        QJsonObject obj = val.toObject();
        Profile p;
        p.name = obj["name"].toString().toStdWString();
        p.targetProcessName = obj["process"].toString().toStdWString();
        p.config.upscaleFactor = static_cast<UpscaleFactor>(obj["upscaleFactor"].toInt(2));
        p.config.upscaleQuality = static_cast<UpscaleQuality>(obj["upscaleQuality"].toInt(1));
        p.config.sharpness = static_cast<float>(obj["sharpness"].toDouble(0.65));
        p.config.frameGenMode = static_cast<FrameGenMode>(obj["frameGenMode"].toInt(2));
        p.config.vsync = obj["vsync"].toBool(true);
        m_profiles.push_back(p);
    }
    return !m_profiles.empty();
}

bool ProfileManager::SaveToDisk(const std::wstring& filePath) const {
    QJsonArray arr;
    for (const auto& p : m_profiles) {
        QJsonObject obj;
        obj["name"] = QString::fromStdWString(p.name);
        obj["process"] = QString::fromStdWString(p.targetProcessName);
        obj["upscaleFactor"] = static_cast<int>(p.config.upscaleFactor);
        obj["upscaleQuality"] = static_cast<int>(p.config.upscaleQuality);
        obj["sharpness"] = p.config.sharpness;
        obj["frameGenMode"] = static_cast<int>(p.config.frameGenMode);
        obj["vsync"] = p.config.vsync;
        arr.append(obj);
    }
    QFile file(QString::fromStdWString(filePath));
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(arr).toJson());
    return true;
}

void ProfileManager::AddOrUpdateProfile(const Profile& profile) {
    for (auto& existing : m_profiles) {
        if (existing.name == profile.name) {
            existing = profile;
            return;
        }
    }
    m_profiles.push_back(profile);
}

Profile ProfileManager::FindForProcess(const std::wstring& processName) const {
    for (const auto& p : m_profiles) {
        if (!p.targetProcessName.empty() && p.targetProcessName == processName) {
            return p;
        }
    }
    return m_profiles.front();
}

} // namespace rfps