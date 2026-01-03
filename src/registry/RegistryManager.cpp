#include "registry/RegistryManager.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace registry {

// Callback for CURL writes
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

RegistryManager::RegistryManager() {
    // Add Docker Hub by default
    auto dockerHub = createDefaultConfig(RegistryType::DockerHub);
    registries_[dockerHub.name] = dockerHub;
}

RegistryManager::~RegistryManager() = default;

bool RegistryManager::addRegistry(const RegistryConfig& config) {
    if (config.name.empty()) {
        lastError_ = "Registry name cannot be empty";
        return false;
    }

    registries_[config.name] = config;
    return true;
}

bool RegistryManager::removeRegistry(const std::string& name) {
    auto it = registries_.find(name);
    if (it == registries_.end()) {
        lastError_ = "Registry not found: " + name;
        return false;
    }

    registries_.erase(it);
    return true;
}

std::optional<RegistryConfig> RegistryManager::getRegistry(const std::string& name) {
    auto it = registries_.find(name);
    if (it == registries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::string> RegistryManager::listRegistries() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : registries_) {
        names.push_back(name);
    }
    return names;
}

std::vector<RegistryConfig> RegistryManager::getAllRegistries() const {
    std::vector<RegistryConfig> configs;
    for (const auto& [_, config] : registries_) {
        configs.push_back(config);
    }
    return configs;
}

bool RegistryManager::setCredentials(const std::string& registryName, const RegistryCredentials& credentials) {
    auto it = registries_.find(registryName);
    if (it == registries_.end()) {
        lastError_ = "Registry not found: " + registryName;
        return false;
    }

    it->second.credentials = credentials;
    return true;
}

std::string RegistryManager::getAuthHeader(const RegistryCredentials& credentials) {
    switch (credentials.method) {
        case AuthMethod::Basic: {
            std::string auth = credentials.username + ":" + credentials.password;
            // Base64 encode would go here
            return "Basic " + auth;
        }
        case AuthMethod::Token:
        case AuthMethod::OAuth:
            return "Bearer " + credentials.token;
        default:
            return "";
    }
}

std::string RegistryManager::makeRequest(const std::string& url,
                                        const std::optional<RegistryCredentials>& credentials,
                                        const std::map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        lastError_ = "Failed to initialize CURL";
        return "";
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* headerList = nullptr;

    // Add authentication
    if (credentials.has_value()) {
        std::string authHeader = "Authorization: " + getAuthHeader(*credentials);
        headerList = curl_slist_append(headerList, authHeader.c_str());
    }

    // Add custom headers
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        headerList = curl_slist_append(headerList, header.c_str());
    }

    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    CURLcode res = curl_easy_perform(curl);

    if (headerList) {
        curl_slist_free_all(headerList);
    }

    if (res != CURLE_OK) {
        lastError_ = "CURL error: " + std::string(curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return "";
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode >= 400) {
        lastError_ = "HTTP error: " + std::to_string(httpCode);
        return "";
    }

    return response;
}

std::pair<bool, std::string> RegistryManager::testConnection(const std::string& registryName) {
    auto it = registries_.find(registryName);
    if (it == registries_.end()) {
        return {false, "Registry not found"};
    }

    const auto& config = it->second;

    // Try to make a simple API call
    std::string testUrl;
    switch (config.type) {
        case RegistryType::DockerHub:
            testUrl = "https://hub.docker.com/v2/repositories/library/alpine/tags";
            break;
        case RegistryType::GHCR:
            testUrl = "https://ghcr.io/v2/";
            break;
        case RegistryType::Quay:
            testUrl = "https://quay.io/api/v1/repository";
            break;
        case RegistryType::GitLab:
            testUrl = config.apiUrl + "/v2/";
            break;
        default:
            testUrl = config.apiUrl + "/v2/";
    }

    std::string response = makeRequest(testUrl, config.credentials);

    if (response.empty() && !lastError_.empty()) {
        return {false, lastError_};
    }

    return {true, "Connection successful"};
}

std::vector<ImageTag> RegistryManager::listDockerHubTags(const RegistryConfig& config, const std::string& repository) {
    std::vector<ImageTag> tags;

    std::string url = "https://hub.docker.com/v2/repositories/" + repository + "/tags?page_size=100";
    std::string response = makeRequest(url, config.credentials);

    if (response.empty()) {
        return tags;
    }

    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("results")) {
            for (const auto& result : json["results"]) {
                ImageTag tag;
                tag.name = result.value("name", "");

                if (result.contains("images") && !result["images"].empty()) {
                    tag.digest = result["images"][0].value("digest", "");
                    tag.size = result["images"][0].value("size", 0);
                }

                tag.created = result.value("last_updated", "");
                tags.push_back(tag);
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "Failed to parse Docker Hub response: " + std::string(e.what());
    }

    return tags;
}

std::vector<ImageTag> RegistryManager::listGHCRTags(const RegistryConfig& config, const std::string& repository) {
    std::vector<ImageTag> tags;

    // GHCR uses OCI distribution spec
    std::string url = "https://ghcr.io/v2/" + repository + "/tags/list";
    std::string response = makeRequest(url, config.credentials);

    if (response.empty()) {
        return tags;
    }

    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("tags")) {
            for (const auto& tagName : json["tags"]) {
                ImageTag tag;
                tag.name = tagName.get<std::string>();

                // Get manifest for digest
                std::string manifestUrl = "https://ghcr.io/v2/" + repository + "/manifests/" + tag.name;
                std::map<std::string, std::string> headers = {
                    {"Accept", "application/vnd.docker.distribution.manifest.v2+json"}
                };
                std::string manifest = makeRequest(manifestUrl, config.credentials, headers);

                if (!manifest.empty()) {
                    try {
                        auto manifestJson = nlohmann::json::parse(manifest);
                        if (manifestJson.contains("config")) {
                            tag.digest = manifestJson["config"].value("digest", "");
                            tag.size = manifestJson["config"].value("size", 0);
                        }
                    } catch (...) {}
                }

                tags.push_back(tag);
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "Failed to parse GHCR response: " + std::string(e.what());
    }

    return tags;
}

std::vector<ImageTag> RegistryManager::listQuayTags(const RegistryConfig& config, const std::string& repository) {
    std::vector<ImageTag> tags;

    std::string url = "https://quay.io/api/v1/repository/" + repository + "/tag/?limit=100";
    std::string response = makeRequest(url, config.credentials);

    if (response.empty()) {
        return tags;
    }

    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("tags")) {
            for (const auto& tagData : json["tags"]) {
                ImageTag tag;
                tag.name = tagData.value("name", "");
                tag.digest = tagData.value("manifest_digest", "");
                tag.size = tagData.value("size", 0);

                if (tagData.contains("last_modified")) {
                    tag.created = tagData["last_modified"].get<std::string>();
                }

                tags.push_back(tag);
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "Failed to parse Quay response: " + std::string(e.what());
    }

    return tags;
}

std::vector<ImageTag> RegistryManager::listGitLabTags(const RegistryConfig& config, const std::string& repository) {
    // GitLab Container Registry uses OCI distribution spec
    return listGenericRegistryTags(config, repository);
}

std::vector<ImageTag> RegistryManager::listGenericRegistryTags(const RegistryConfig& config, const std::string& repository) {
    std::vector<ImageTag> tags;

    std::string url = config.apiUrl + "/v2/" + repository + "/tags/list";
    std::string response = makeRequest(url, config.credentials);

    if (response.empty()) {
        return tags;
    }

    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("tags")) {
            for (const auto& tagName : json["tags"]) {
                ImageTag tag;
                tag.name = tagName.get<std::string>();
                tags.push_back(tag);
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "Failed to parse registry response: " + std::string(e.what());
    }

    return tags;
}

std::vector<ImageTag> RegistryManager::listTags(const std::string& registryName, const std::string& repository) {
    auto it = registries_.find(registryName);
    if (it == registries_.end()) {
        lastError_ = "Registry not found: " + registryName;
        return {};
    }

    const auto& config = it->second;

    switch (config.type) {
        case RegistryType::DockerHub:
            return listDockerHubTags(config, repository);
        case RegistryType::GHCR:
            return listGHCRTags(config, repository);
        case RegistryType::Quay:
            return listQuayTags(config, repository);
        case RegistryType::GitLab:
            return listGitLabTags(config, repository);
        default:
            return listGenericRegistryTags(config, repository);
    }
}

std::optional<ImageTag> RegistryManager::getTag(const std::string& registryName,
                                               const std::string& repository,
                                               const std::string& tag) {
    auto tags = listTags(registryName, repository);

    for (const auto& t : tags) {
        if (t.name == tag) {
            return t;
        }
    }

    return std::nullopt;
}

bool RegistryManager::imageExists(const std::string& registryName,
                                 const std::string& repository,
                                 const std::string& tag) {
    return getTag(registryName, repository, tag).has_value();
}

std::vector<ImageSearchResult> RegistryManager::searchImages(const std::string& searchTerm,
                                                             const std::string& registryFilter) {
    std::vector<ImageSearchResult> results;

    for (const auto& [name, config] : registries_) {
        if (!registryFilter.empty() && name != registryFilter) {
            continue;
        }

        // Only Docker Hub supports search API well
        if (config.type == RegistryType::DockerHub) {
            std::string url = "https://hub.docker.com/v2/search/repositories/?query=" + searchTerm;
            std::string response = makeRequest(url, config.credentials);

            if (!response.empty()) {
                try {
                    auto json = nlohmann::json::parse(response);
                    if (json.contains("results")) {
                        for (const auto& result : json["results"]) {
                            ImageSearchResult searchResult;
                            searchResult.name = result.value("repo_name", "");
                            searchResult.description = result.value("short_description", "");
                            searchResult.stars = result.value("star_count", 0);
                            searchResult.isOfficial = result.value("is_official", false);
                            searchResult.registry = name;
                            results.push_back(searchResult);
                        }
                    }
                } catch (...) {}
            }
        }
    }

    return results;
}

std::tuple<std::string, std::string, std::string> RegistryManager::parseImageReference(const std::string& imageRef) {
    std::string registry, repository, tag;

    // Format: [registry/]repository[:tag]
    size_t tagPos = imageRef.find_last_of(':');
    size_t slashPos = imageRef.find_first_of('/');

    // Extract tag
    if (tagPos != std::string::npos && tagPos > slashPos) {
        tag = imageRef.substr(tagPos + 1);
    } else {
        tag = "latest";
        tagPos = imageRef.length();
    }

    std::string repoPath = imageRef.substr(0, tagPos);

    // Check if registry is specified
    if (slashPos != std::string::npos) {
        std::string possibleRegistry = repoPath.substr(0, slashPos);

        // Check if it looks like a domain
        if (possibleRegistry.find('.') != std::string::npos ||
            possibleRegistry.find(':') != std::string::npos) {
            registry = possibleRegistry;
            repository = repoPath.substr(slashPos + 1);
        } else {
            registry = "docker.io";
            repository = repoPath;
        }
    } else {
        registry = "docker.io";
        repository = repoPath;
    }

    return {registry, repository, tag};
}

RegistryType RegistryManager::getRegistryTypeFromDomain(const std::string& domain) {
    if (domain.find("docker.io") != std::string::npos || domain.find("docker.com") != std::string::npos) {
        return RegistryType::DockerHub;
    } else if (domain.find("ghcr.io") != std::string::npos) {
        return RegistryType::GHCR;
    } else if (domain.find("quay.io") != std::string::npos) {
        return RegistryType::Quay;
    } else if (domain.find("gitlab") != std::string::npos) {
        return RegistryType::GitLab;
    } else if (domain.find("ecr") != std::string::npos && domain.find("amazonaws.com") != std::string::npos) {
        return RegistryType::ECR;
    } else if (domain.find("gcr.io") != std::string::npos) {
        return RegistryType::GCR;
    } else if (domain.find("azurecr.io") != std::string::npos) {
        return RegistryType::ACR;
    }

    return RegistryType::Custom;
}

RegistryConfig RegistryManager::createDefaultConfig(RegistryType type) {
    RegistryConfig config;
    config.type = type;
    config.secure = true;

    switch (type) {
        case RegistryType::DockerHub:
            config.name = "Docker Hub";
            config.url = "https://hub.docker.com";
            config.apiUrl = "https://hub.docker.com/v2";
            break;

        case RegistryType::GHCR:
            config.name = "GitHub Container Registry";
            config.url = "https://ghcr.io";
            config.apiUrl = "https://ghcr.io/v2";
            break;

        case RegistryType::Quay:
            config.name = "Quay.io";
            config.url = "https://quay.io";
            config.apiUrl = "https://quay.io/api/v1";
            break;

        case RegistryType::GitLab:
            config.name = "GitLab Container Registry";
            config.url = "https://registry.gitlab.com";
            config.apiUrl = "https://registry.gitlab.com/v2";
            break;

        case RegistryType::Harbor:
            config.name = "Harbor Registry";
            config.url = "https://harbor.local";
            config.apiUrl = "https://harbor.local/api";
            break;

        default:
            config.name = "Custom Registry";
            config.url = "https://registry.local";
            config.apiUrl = "https://registry.local/v2";
    }

    return config;
}

bool RegistryManager::saveToFile(const std::string& filePath) {
    try {
        nlohmann::json json = nlohmann::json::array();

        for (const auto& [_, config] : registries_) {
            nlohmann::json regJson;
            regJson["name"] = config.name;
            regJson["type"] = static_cast<int>(config.type);
            regJson["url"] = config.url;
            regJson["apiUrl"] = config.apiUrl;
            regJson["secure"] = config.secure;

            if (config.credentials.has_value()) {
                regJson["credentials"] = {
                    {"username", config.credentials->username},
                    {"method", static_cast<int>(config.credentials->method)}
                    // Note: Don't save passwords in plain text!
                };
            }

            json.push_back(regJson);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            lastError_ = "Failed to open file for writing: " + filePath;
            return false;
        }

        file << json.dump(2);
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Failed to save registries: " + std::string(e.what());
        return false;
    }
}

bool RegistryManager::loadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            lastError_ = "Failed to open file for reading: " + filePath;
            return false;
        }

        nlohmann::json json;
        file >> json;

        registries_.clear();

        for (const auto& regJson : json) {
            RegistryConfig config;
            config.name = regJson.value("name", "");
            config.type = static_cast<RegistryType>(regJson.value("type", 0));
            config.url = regJson.value("url", "");
            config.apiUrl = regJson.value("apiUrl", "");
            config.secure = regJson.value("secure", true);

            if (regJson.contains("credentials")) {
                RegistryCredentials creds;
                creds.username = regJson["credentials"].value("username", "");
                creds.method = static_cast<AuthMethod>(regJson["credentials"].value("method", 0));
                config.credentials = creds;
            }

            registries_[config.name] = config;
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = "Failed to load registries: " + std::string(e.what());
        return false;
    }
}

std::string RegistryManager::getLastError() const {
    return lastError_;
}

std::vector<std::string> RegistryManager::listRepositories(const std::string& registryName,
                                                          const std::string& searchTerm) {
    // This would require different implementations per registry
    // For now, return empty
    return {};
}

std::optional<Repository> RegistryManager::getRepository(const std::string& registryName,
                                                        const std::string& repository) {
    // Get tags for the repository
    auto tags = listTags(registryName, repository);

    if (tags.empty()) {
        return std::nullopt;
    }

    Repository repo;
    repo.name = repository;
    repo.tags = tags;

    return repo;
}

bool RegistryManager::pullImage(const std::string& registryName, const std::string& image) {
    // This would integrate with Docker client
    lastError_ = "Pull not implemented yet";
    return false;
}

bool RegistryManager::pushImage(const std::string& registryName, const std::string& image) {
    // This would integrate with Docker client
    lastError_ = "Push not implemented yet";
    return false;
}

std::string RegistryManager::getManifest(const std::string& registryName,
                                        const std::string& repository,
                                        const std::string& tag) {
    auto it = registries_.find(registryName);
    if (it == registries_.end()) {
        lastError_ = "Registry not found";
        return "";
    }

    const auto& config = it->second;
    std::string url = config.apiUrl + "/v2/" + repository + "/manifests/" + tag;

    std::map<std::string, std::string> headers = {
        {"Accept", "application/vnd.docker.distribution.manifest.v2+json"}
    };

    return makeRequest(url, config.credentials, headers);
}

bool RegistryManager::authenticate(const RegistryConfig& config) {
    // Implement authentication logic for different registries
    return true;
}

} // namespace registry
