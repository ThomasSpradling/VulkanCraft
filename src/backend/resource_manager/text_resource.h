#include "resource.h"
#include <fstream>
#include <sstream>
#include <string>

class TextResource : public Resource {
public:
    TextResource(const std::string &id) : Resource(id) {}
    ~TextResource() override {
        UnloadResource();
    }

    virtual bool Load() override {
        std::string file_path = RESOURCE_PATH "/texts/" + GetId() + ".txt";
        std::ifstream ifstream(file_path);
        std::ostringstream oss;
        
        std::string line;
        while(std::getline(ifstream, line)) {
            oss << line;
        }
        m_contents = oss.str();
        ifstream.close();
        return true;
    };

    virtual void Unload() override {
        m_contents.clear();
    }

    std::string Contents() const { return m_contents; }
private:
    std::string m_contents;
};
