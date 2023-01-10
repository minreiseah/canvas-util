#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <filesystem>

#include <curl/curl.h>
#include <jsoncpp/json/json.h>

#include "defines.h"

namespace fs = std::filesystem;

size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

size_t writeData(void *ptr, size_t size, size_t nmemb, void *stream);

typedef struct {
    int id;
    std::string name;
    std::string code;
} Course;

std::vector<Course> listCourses(std::string baseURL, CURL *curl);

typedef struct {
    int id;
    std::string name;
    std::string path;
    std::string createdAt;
    std::string updatedAt;
} Folder;

std::vector<Folder> listFoldersByCourse(std::string baseURL, CURL *curl, Course &course);

typedef struct {
    int id;
    std::string name;
    std::string url;
    std::string createdAt;
    std::string updatedAt;
} File;

std::vector<File> listFilesByFolder(std::string baseURL, CURL *curl, int folderID);

std::string formatPath(std::string path, std::string code);

bool writeFile(CURL *curl, std::string path, std::string url, std::string name);

int main() {
    // setup
    std::string baseURL;
    baseURL = BASEURL;
    CURL *curl;
    curl = curl_easy_init();

    std::string accessToken = ACCESS_TOKEN;
    std::string authbearer = "Authorization: Bearer " + accessToken;

    struct curl_slist *headers;
    headers = NULL;
    headers = curl_slist_append(headers, authbearer.c_str());
    curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // fail on 400 or 500

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // debugging

    // get courses
    std::vector<Course> courses = listCourses(baseURL, curl);

    for(auto &course : courses) {
        std::cout << "Processing Course: " << course.name << std::endl;

        // get folders
        std::vector<Folder> folders = listFoldersByCourse(baseURL, curl, course);

        for(auto &folder : folders) {
            std::cout << "Folder: " << folder.name << std::endl;

            // get files
            std::vector<File> files = listFilesByFolder(baseURL, curl, folder.id);

            for(auto &file : files) {
                std::cout << "File: " << file.name;
                bool writeSuccessful = writeFile(curl, folder.path, file.url, file.name);
                if(!writeSuccessful) {
                    std::cout << " FAILED" << std::endl;
                    return 1;
                } else {
                    std:: cout<< " SUCCESS" << std::endl;
                }
            }

            std::cout << std::endl;
        }

        std::cout << std::endl;
    }


    // cleanup
    curl_easy_cleanup(curl);
    curl = NULL;
    curl_slist_free_all(headers);
    headers = NULL;

    return 0;
}

size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    // cast userdata pointer to a string pointer
    std::string *response = static_cast<std::string*>(userdata);

    // append received data to response string
    response->append(ptr, size *nmemb);
    return size *nmemb;
}

size_t writeData(void *ptr, size_t size, size_t nmemb, void *stream) {
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

std::vector<Course> listCourses(std::string baseURL, CURL *curl) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    std::string path = "courses";
    std::string query = baseURL + "/" + path;
    curl_easy_setopt(curl, CURLOPT_URL, query.c_str());

    // set response
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // perform request
    CURLcode res;
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std:: cout << &"ERROR: " [ (int)res];
    }

    // parse response
    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(response, root);
    if (!parsingSuccessful) {
        std::cout << "ERROR: Failed to parse " + query << std::endl;
        return {};
    }

    // extract info
    std::vector<Course> courses;
    for(auto &course: root) {
        if(course["access_restricted_by_date"].asBool()) continue;
        if(course["enrollment_term_id"].asInt() != TERMID && TERMID != -1) continue;
        Course data;
        data.id = course["id"].asInt();
        data.name = course["name"].asString();
        data.code = course["course_code"].asString();
        courses.push_back(data);
    }

    return courses;
}

std::vector<Folder> listFoldersByCourse(std::string baseURL, CURL *curl, Course& course) {
    // setup
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    std::string query = baseURL + "/courses/" + std::to_string(course.id) + "/folders";
    curl_easy_setopt(curl, CURLOPT_URL, query.c_str());

    // set response
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // perform request
    CURLcode res;
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std:: cout << &"ERROR: " [ (int)res];
    }

    // parse response
    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(response, root);
    if (!parsingSuccessful) {
        std::cout << "ERROR: Failed to parse " + query << std::endl;
        return {};
    }

    // extract info
    std::vector<Folder> folders;
    for(auto &folder : root) {
        Folder data;
        data.id = folder["id"].asInt();
        data.name = folder["name"].asString();
        data.path = formatPath(folder["full_name"].asString(), course.code);
        data.createdAt = folder["created_at"].asString();
        data.updatedAt = folder["updated_at"].asString();
        folders.push_back(data);
    }

    return folders;
}

std::vector<File> listFilesByFolder(std::string baseURL, CURL *curl, int file) {
    // setup
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    std::string query = baseURL + "/folders/" + std::to_string(file) + "/files";
    curl_easy_setopt(curl, CURLOPT_URL, query.c_str());

    // set response
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // perform request
    CURLcode res;
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std:: cout << &"ERROR: " [ (int)res];
        return {};
    }

    // parse response
    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(response, root);
    if (!parsingSuccessful) {
        std::cout << "ERROR: Failed to parse " + query << std::endl;
        return {};
    }

    // extract info
    std::vector<File> files;
    for(auto &file : root) {
        File data;
        data.id = file["id"].asInt();
        data.name = file["filename"].asString();
        data.url = file["url"].asString();
        data.createdAt = file["created_at"].asString();
        data.updatedAt = file["updated_at"].asString();
        files.push_back(data);
    }

    return files;
}

// returns writeSuccessful
std::string formatPath(std::string path, std::string code) {
    std::string defaultRootPath = DEFAULTROOTPATH;
    size_t pos = path.find(defaultRootPath);
    if (pos != std::string::npos) {
        path.replace(pos, defaultRootPath.length(), code);
    }
    std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    path = TARGETDIR + path;
    return path;
}

// return writeSuccessful
bool writeFile(CURL *curl, std::string path, std::string url, std::string name) {
    // create directory if not exists
    fs::path dirPath = path;
    fs::create_directories(dirPath);

    // setup
    std::string filepath = path + '/' + name;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // set file response
    FILE* fp = std::fopen(filepath.c_str(), "wb");
    if(fp) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        // perform request
        CURLcode res;
        res = curl_easy_perform(curl);

        std::fclose(fp);

        if (res != CURLE_OK) {
            std:: cout << &"ERROR: " [ (int)res];
            return 0;
        }

    }


    return 1;
}
