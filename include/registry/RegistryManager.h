#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace registry {

// Registry type enum
enum class RegistryType {
    DockerHub,
    GHCR,          // GitHub Container Registry
    Quay,          // Quay.io
    GitLab,        // GitLab Container Registry
    Harbor,        // Harbor registry
    ECR,           // AWS Elastic Container Registry
    GCR,           // Google Container Registry
    ACR,           // Azure Container Registry
    Custom         // Custom/private registry
};

// Authentication method
enum class AuthMethod {
    None,
    Basic,         // Username/password
    Token,         // Bearer token
    OAuth,         // OAuth token
    AWS            // AWS credentials (for ECR)
};

// Registry credentials
struct RegistryCredentials {
    std::string username;
    std::string password;
    std::string token;
    AuthMethod method = AuthMethod::Basic;
};

// Registry configuration
struct RegistryConfig {
    std::string name;
    RegistryType type;
    std::string url;
    std::string apiUrl;
    bool secure = true;
    std::optional<RegistryCredentials> credentials;
    std::map<std::string, std::string> customHeaders;
};

// Image tag information
struct ImageTag {
    std::string name;
    std::string digest;
    std::string created;
    int64_t size = 0;
    std::map<std::string, std::string> labels;
};

// Repository information
struct Repository {
    std::string name;
    std::string description;
    bool isPublic = true;
    std::vector<ImageTag> tags;
    std::string lastUpdated;
};

// Search result
struct ImageSearchResult {
    std::string name;
    std::string description;
    int stars = 0;
    bool isOfficial = false;
    bool isAutomated = false;
    std::string registry;
};

/**
 * RegistryManager - Manages multiple container registries
 */
class RegistryManager {
public:
    RegistryManager();
    ~RegistryManager();

    /**
     * Add a registry configuration
     * @param config Registry configuration
     * @return Success status
     */
    bool addRegistry(const RegistryConfig& config);

    /**
     * Remove a registry
     * @param name Registry name
     * @return Success status
     */
    bool removeRegistry(const std::string& name);

    /**
     * Get registry configuration
     * @param name Registry name
     * @return Registry configuration if found
     */
    std::optional<RegistryConfig> getRegistry(const std::string& name);

    /**
     * List all configured registries
     * @return Vector of registry names
     */
    std::vector<std::string> listRegistries() const;

    /**
     * Get all registry configurations
     * @return Vector of registry configs
     */
    std::vector<RegistryConfig> getAllRegistries() const;

    /**
     * Set credentials for a registry
     * @param registryName Registry name
     * @param credentials Credentials
     * @return Success status
     */
    bool setCredentials(const std::string& registryName, const RegistryCredentials& credentials);

    /**
     * Test registry connection and authentication
     * @param registryName Registry name
     * @return Success status and error message
     */
    std::pair<bool, std::string> testConnection(const std::string& registryName);

    /**
     * List repositories in a registry
     * @param registryName Registry name
     * @param searchTerm Optional search term
     * @return Vector of repository names
     */
    std::vector<std::string> listRepositories(const std::string& registryName,
                                              const std::string& searchTerm = "");

    /**
     * Get repository information
     * @param registryName Registry name
     * @param repository Repository name
     * @return Repository information
     */
    std::optional<Repository> getRepository(const std::string& registryName,
                                           const std::string& repository);

    /**
     * List tags for an image
     * @param registryName Registry name
     * @param repository Repository name
     * @return Vector of image tags
     */
    std::vector<ImageTag> listTags(const std::string& registryName,
                                   const std::string& repository);

    /**
     * Get tag information
     * @param registryName Registry name
     * @param repository Repository name
     * @param tag Tag name
     * @return Tag information
     */
    std::optional<ImageTag> getTag(const std::string& registryName,
                                   const std::string& repository,
                                   const std::string& tag);

    /**
     * Search for images across registries
     * @param searchTerm Search term
     * @param registryFilter Optional registry to search (empty = all)
     * @return Vector of search results
     */
    std::vector<ImageSearchResult> searchImages(const std::string& searchTerm,
                                                const std::string& registryFilter = "");

    /**
     * Pull image from registry
     * @param registryName Registry name
     * @param image Image name (repository:tag)
     * @return Success status
     */
    bool pullImage(const std::string& registryName, const std::string& image);

    /**
     * Push image to registry
     * @param registryName Registry name
     * @param image Image name (repository:tag)
     * @return Success status
     */
    bool pushImage(const std::string& registryName, const std::string& image);

    /**
     * Check if image exists in registry
     * @param registryName Registry name
     * @param repository Repository name
     * @param tag Tag name
     * @return True if image exists
     */
    bool imageExists(const std::string& registryName,
                    const std::string& repository,
                    const std::string& tag);

    /**
     * Get image manifest
     * @param registryName Registry name
     * @param repository Repository name
     * @param tag Tag name
     * @return Manifest JSON
     */
    std::string getManifest(const std::string& registryName,
                           const std::string& repository,
                           const std::string& tag);

    /**
     * Parse image reference to determine registry
     * @param imageRef Full image reference (e.g., "ghcr.io/user/repo:tag")
     * @return Tuple of (registry, repository, tag)
     */
    std::tuple<std::string, std::string, std::string> parseImageReference(const std::string& imageRef);

    /**
     * Get default registry for domain
     * @param domain Domain (e.g., "ghcr.io")
     * @return Registry type
     */
    static RegistryType getRegistryTypeFromDomain(const std::string& domain);

    /**
     * Create a default registry configuration
     * @param type Registry type
     * @return Default configuration
     */
    static RegistryConfig createDefaultConfig(RegistryType type);

    /**
     * Save registry configurations to file
     * @param filePath Path to save configurations
     * @return Success status
     */
    bool saveToFile(const std::string& filePath);

    /**
     * Load registry configurations from file
     * @param filePath Path to load configurations from
     * @return Success status
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * Get last error message
     * @return Error message
     */
    std::string getLastError() const;

private:
    std::map<std::string, RegistryConfig> registries_;
    std::string lastError_;

    // Helper methods for specific registries
    std::vector<ImageTag> listDockerHubTags(const RegistryConfig& config, const std::string& repository);
    std::vector<ImageTag> listGHCRTags(const RegistryConfig& config, const std::string& repository);
    std::vector<ImageTag> listQuayTags(const RegistryConfig& config, const std::string& repository);
    std::vector<ImageTag> listGitLabTags(const RegistryConfig& config, const std::string& repository);
    std::vector<ImageTag> listGenericRegistryTags(const RegistryConfig& config, const std::string& repository);

    std::string makeRequest(const std::string& url, const std::optional<RegistryCredentials>& credentials,
                           const std::map<std::string, std::string>& headers = {});

    std::string getAuthHeader(const RegistryCredentials& credentials);
    bool authenticate(const RegistryConfig& config);
};

} // namespace registry
