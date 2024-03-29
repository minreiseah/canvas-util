#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <sys/stat.h>

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
  std::printf("\033[1;36mDOWNLOADING FILES\033[0m\n");

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

  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // fail on 400 or 500

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // debugging

  // get courses
  std::vector<Course> courses = listCourses(baseURL, curl);

  for(auto &course : courses) {
    if (course.code == "CS1101S" || course.code == "SOCT101") continue;

    if (course.code == "MA2116/ST2131") course.code = "ST2131";
    if (course.code == "UTC2714/UTS2706") course.code = "UTS2706";

    std::printf("Processing Course:\033[1;35m %s\033[0m\n", course.name.c_str());

    // get folders
    std::vector<Folder> folders = listFoldersByCourse(baseURL, curl, course);
    bool isFolderEmpty = true;

    for(auto &folder : folders) {
      std::vector<File> files = listFilesByFolder(baseURL, curl, folder.id);

      for(auto &file : files) {
        isFolderEmpty = false;
        writeFile(curl, folder.path, file.url, file.name);
      }
      if (!isFolderEmpty) std::cout << std::endl;
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
  std::string path = "/courses?enrollment_state=active";
  std::string query = baseURL + path;
  curl_easy_setopt(curl, CURLOPT_URL, query.c_str());

  // set response
  std::string response;
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  // perform request
  CURLcode res;
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cout << "LIBCURL ERROR: " << (int)res << std::endl;
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
  std::string query = baseURL + "/courses/" + std::to_string(course.id) + "/folders?per_page=1000";
  curl_easy_setopt(curl, CURLOPT_URL, query.c_str());

  // set response
  std::string response;
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  // perform request
  CURLcode res;
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cout << "LIBCURL ERROR: " << (int)res << std::endl;
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
  std::string additional_query = "?include[]=user&include[]=usage_rights&include[]=enhanced_preview_url&include[]=context_asset_string&per_page=100&sort=&order=";
  std::string query = baseURL + "/folders/" + std::to_string(file) + "/files" + additional_query;
  curl_easy_setopt(curl, CURLOPT_URL, query.c_str());

  // set response
  std::string response;
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  // perform request
  CURLcode res;
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cout << "LIBCURL ERROR: " << (int)res << std::endl;
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
  std::printf("%s", filepath.c_str());

  // check if file already exists
  struct stat buffer;
  bool fileExists = stat (filepath.c_str(), &buffer) == 0;
  if(fileExists) {
      std::cout << "\033[1;34m EXISTS\033[0m\n";
      return 1;
  }

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
      std::cout << "\033[1;31m FAILED\033[0m\n";
      std:: cout << "LIBCURL ERROR: " << (int)res << std::endl;
      return 0;
    }

    std::cout << "\033[1;32m SUCCESS\033[0m\n";

  }
  return 1;
}
