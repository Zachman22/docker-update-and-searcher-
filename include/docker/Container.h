#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace docker {

struct PortMapping {
    int containerPort;
    int hostPort;
    std::string protocol; // "tcp" or "udp"
};

struct VolumeMount {
    std::string source;
    std::string destination;
    std::string mode; // "rw" or "ro"
};

struct NetworkConnection {
    std::string networkId;
    std::string networkName;
    std::string ipAddress;
    std::string gateway;
};

enum class ContainerState {
    Running,
    Stopped,
    Paused,
    Restarting,
    Dead,
    Created,
    Exited,
    Unknown
};

class Container {
public:
    Container() = default;
    Container(const std::string& id, const std::string& name);

    // Getters
    std::string getId() const { return id_; }
    std::string getName() const { return name_; }
    std::string getImage() const { return image_; }
    std::string getImageTag() const { return imageTag_; }
    ContainerState getState() const { return state_; }
    std::vector<PortMapping> getPorts() const { return ports_; }
    std::vector<VolumeMount> getVolumes() const { return volumes_; }
    std::vector<NetworkConnection> getNetworks() const { return networks_; }
    std::map<std::string, std::string> getEnvironment() const { return environment_; }
    std::chrono::system_clock::time_point getCreatedTime() const { return createdTime_; }
    std::chrono::system_clock::time_point getStartedTime() const { return startedTime_; }
    bool isHealthy() const { return isHealthy_; }
    std::string getStatus() const { return status_; }

    // Setters
    void setId(const std::string& id) { id_ = id; }
    void setName(const std::string& name) { name_ = name; }
    void setImage(const std::string& image) { image_ = image; }
    void setImageTag(const std::string& tag) { imageTag_ = tag; }
    void setState(ContainerState state) { state_ = state; }
    void setPorts(const std::vector<PortMapping>& ports) { ports_ = ports; }
    void setVolumes(const std::vector<VolumeMount>& volumes) { volumes_ = volumes; }
    void setNetworks(const std::vector<NetworkConnection>& networks) { networks_ = networks; }
    void setEnvironment(const std::map<std::string, std::string>& env) { environment_ = env; }
    void setCreatedTime(const std::chrono::system_clock::time_point& time) { createdTime_ = time; }
    void setStartedTime(const std::chrono::system_clock::time_point& time) { startedTime_ = time; }
    void setHealthy(bool healthy) { isHealthy_ = healthy; }
    void setStatus(const std::string& status) { status_ = status; }

    // Utility methods
    bool isRunning() const { return state_ == ContainerState::Running; }
    std::string getStateString() const;
    std::vector<std::string> getDependencies() const { return dependencies_; }
    void addDependency(const std::string& dependency) { dependencies_.push_back(dependency); }

private:
    std::string id_;
    std::string name_;
    std::string image_;
    std::string imageTag_;
    ContainerState state_ = ContainerState::Unknown;
    std::vector<PortMapping> ports_;
    std::vector<VolumeMount> volumes_;
    std::vector<NetworkConnection> networks_;
    std::map<std::string, std::string> environment_;
    std::chrono::system_clock::time_point createdTime_;
    std::chrono::system_clock::time_point startedTime_;
    bool isHealthy_ = true;
    std::string status_;
    std::vector<std::string> dependencies_;
};

} // namespace docker
