#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <sys/stat.h>

#include <curl/curl.h>
#include <jsoncpp/json/json.h>

#include "utils.h"
#include "defines.h"

namespace fs = std::filesystem;

size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);

size_t writeData(void *ptr, size_t size, size_t nmemb, FILE *stream);

bool writeFile(CURL *curl, std::string path, std::string url, std::string name);

std::string buildFilePath (std::string folderpath, std::string filename, std::string type);

typedef struct {
  std::string id;
  std::string name;
  std::string path;
} Folder;

std::vector<Folder> listFoldersByCourse(CURL *curl, std::string folderID, std::string courseName, std::string folderName);

typedef struct {
  std::string name;
  std::string url;
} Video;

std::vector<Video> listVideosByFolder(CURL *curl, Folder folder);

std::map<std::string, std::string> courses = {
  {"cs3230", "b05adfd0-ee69-4935-8ca3-b05301118c13"},
  {"cs2100", "f01cee76-bb0f-4dda-8eba-b05a001120cb"},
  {"st1131", "bcba8f40-d698-4381-9444-b0430059e2bb"},
  {"st2131", "7c59929d-d3cc-4cb8-8f6f-b05300846879"},
  {"is2218", "235b2c6b-ca63-4548-ad96-b060001a3fa4"},
};

int main(int argc, char *argv[]){ std::printf("\033[1;36mDOWNLOADING VIDEOS\033[0m\n");

  // setup
  CURL *curl;
  curl = curl_easy_init();

  struct curl_slist *headers = NULL;
  std::string aspxauthToken = ASPXAUTH;
  std::string auth = "cookie: .ASPXAUTH=" + aspxauthToken;
  headers = curl_slist_append(headers, auth.c_str());

  headers = curl_slist_append(headers, "content-type: application/json; charset=UTF-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1L);

  curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // fail on 400 or 500
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.71.1");

  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // debugging

  // get course folders
  for(auto &course : courses) {
    std::printf("Processing Course:\033[1;35m %s\033[0m\n", course.first.c_str());

    std::vector<Folder> folders = listFoldersByCourse(curl, course.second, course.first, course.first);

    for(auto &folder : folders) {
      bool isFolderEmpty = true;
      std::vector<Video> videos = listVideosByFolder(curl, folder);

      for(auto &video : videos) {
        isFolderEmpty = false;
        writeFile(curl, folder.path, video.url, video.name);
      }
      if (isFolderEmpty) {
        std::printf("\033[1;31mEMPTY\033[0m\n");
      } else {
        std::cout << std::endl;
      }
    }
    std::cout << std::endl;
  }

  curl_easy_cleanup(curl);
  curl = NULL;
  curl_slist_free_all(headers);
  headers = NULL;

  return 0;
}

std::vector<Folder> listFoldersByCourse(CURL *curl, std::string folderID, std::string courseName, std::string folderName) {
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(curl, CURLOPT_URL, "https://mediaweb.ap.panopto.com/Panopto/Services/Data.svc/GetSessions");

  // set query
  Json::Value query;
  query["queryParameters"]["folderID"] = folderID;
  query["queryParameters"]["getFolderData"] = true;
  std::regex pattern("([^{])\\s*:");
  std::string replacement("$1 : ");
  std::string formattedQuery = std::regex_replace(query.toStyledString(), pattern, replacement);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formattedQuery.c_str());

  // set response
  std::string response;
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  // make request
  CURLcode res;
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cout << "LIBCURL ERROR in Getting Folders: " << (int)res << std::endl;
    return {};
  }

  // parse response
  Json::Value root;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(response, root);
  if (!parsingSuccessful) {
    std::cout << "ERROR: Failed to parse." << std::endl;
    return {};
  }

  // extract info
  std::vector<Folder> folders;

  Folder this_folder;
  this_folder.id = folderID;
  this_folder.name = folderName;
  this_folder.path = TARGETDIR + courseName + "/lectures-downloaded"; // download directory
  folders.push_back(this_folder);

  // recursively add folders
  for (auto &folder : root["d"]["Subfolders"]) {
    std::vector<Folder> other_folders = listFoldersByCourse(curl, folder["ID"].asString(), courseName, folder["Name"].asString());
    for (Folder inner_folder : other_folders) {
      folders.push_back(inner_folder);
    }
  }

  return folders;
}

std::vector<Video> listVideosByFolder(CURL *curl, Folder folder) {
  std::printf("Folder:\033[1;33m %s\033[0m\n", folder.name.c_str());

  // set URI
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(curl, CURLOPT_URL, "https://mediaweb.ap.panopto.com/Panopto/Services/Data.svc/GetSessions");

  // set query
  Json::Value query;
  query["queryParameters"]["folderID"] = folder.id;
  std::regex pattern("([^{])\\s*:");
  std::string replacement("$1 : ");
  std::string formattedQuery = std::regex_replace(query.toStyledString(), pattern, replacement);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formattedQuery.c_str());

  // set response
  std::string response;
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  // make request
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
    std::cout << "ERROR: Failed to parse." << std::endl;
    return {};
  }

  // extract info
  std::vector<Video> videos;
  for(auto &video : root["d"]["Results"]) {
    Video data;
    data.name = video["SessionName"].asString();
    data.url = video["IosVideoUrl"].asString();
    videos.push_back(data);
  }

  return videos;
}

bool writeFile(CURL *curl, std::string folderpath, std::string url, std::string name) {
  // setup
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
  curl_easy_setopt(curl, CURLOPT_POST, 0);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  // create directory if not exists
  fs::path dirPath = folderpath;
  fs::create_directories(dirPath);

  std::string filepath = buildFilePath(folderpath, name, "mp4");
  std::printf("%s", filepath.c_str());

  // check if file already exists
  struct stat buffer;
  bool fileExists = stat (filepath.c_str(), &buffer) == 0;
  if(fileExists) {
      std::cout << "\033[1;34m EXISTS\033[0m\n";
      return 1;
  }

  // open file to write
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

std::string buildFilePath (std::string folderpath, std::string filename, std::string type) {
  std::regex invalid_chars("[^a-zA-Z0-9._-]");
  filename += '.' + type;
  filename = std::regex_replace(filename, invalid_chars, "_");
  return folderpath + '/' + filename;
}

size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  // cast userdata pointer to a string pointer
  std::string *response = static_cast<std::string*>(userdata);

  // append received data to response string
  response->append(ptr, size *nmemb);
  return size *nmemb;
}

size_t writeData(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t written = fwrite(ptr, size, nmemb, stream);
  return written;
}
