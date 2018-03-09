/*
Copyright (C) 2015-2018  Xavier MARCHAL
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/filesystem.hpp>

#include <fstream>
#include <unordered_set>

#include "http_engine.h"

HttpEngine::HttpEngine(size_t concurrency) : Module(concurrency) {

}

void HttpEngine::run() {
    std::ifstream file("mime.types");
    if (file) {
        std::string line;
        while(!file.eof()) {
            std::getline(file, line);
            if (line.length() > 0 && line[0] != '#') {
                std::stringstream ss(line);
                std::string type;
                std::string extension;
                ss >> type;
                do {
                    ss >> extension;
                    //std::cout << (extension[0] == '.' ? extension : "." + extension) << std::endl;
                    _mime_types.emplace(extension[0] == '.' ? extension : "." + extension, type);
                } while (!ss.eof());
            }
        }
    }
}

void HttpEngine::fromHttpSource(const std::shared_ptr<HttpRequest> &http_request) {
    _ios.post(boost::bind(&HttpEngine::resolve, this, http_request));
}

void HttpEngine::resolve(const std::shared_ptr<HttpRequest> &http_request) {
    enum method_type {
        CONNECT,
        DELETE,
        GET,
        HEAD,
        OPTIONS,
        PATCH,
        POST,
        PUT,
        TRACE,
    };
    static const std::map<std::string, method_type> AVAILABLE_METHODS{
            {"DELETE",  DELETE},
            {"HEAD",    HEAD},
            {"GET",     GET},
            {"OPTIONS", OPTIONS},
            {"PATCH",   PATCH},
            {"POST",    POST},
            {"PUT",     PUT},
            {"TRACE",   TRACE}
    };

    if (!http_request->is_parsed()) {
        return;
    }

    auto http_response = std::make_shared<HttpResponse>();
    auto it = AVAILABLE_METHODS.find(http_request->get_method());
    if (it != AVAILABLE_METHODS.end()) {
        switch (it->second) {
            case HEAD:
                get(http_request, http_response, false);
                break;
            case GET:
                get(http_request, http_response);
                break;
            default:
                break;
        }
    }
    http_request->is_parsed();
    http_request->getRawStream()->is_completed();
    _http_source->fromHttpSink(http_request, http_response);
}

std::shared_ptr<HttpResponse> HttpEngine::get(const std::shared_ptr<HttpRequest> &http_request,
                                              const std::shared_ptr<HttpResponse> &http_response, bool skip_body) {
    static const std::unordered_set<std::string> STATIC_EXTENSIONS {
            ".css", ".js", ".jpg", ".jpeg", ".gif", ".ico", ".png", ".bmp", ".pict", ".csv", ".doc", ".pdf", ".pls", ".ppt",
            ".tif", ".tiff", ".eps", ".ejs", ".swf", ".midi", ".mid", ".ttf", ".eot", ".woff", ".woff2", ".otf", ".svg",
            ".svgz", ".webp", ".docx", ".xlsx", ".xls", ".pptx", ".ps", ".class", ".jar"
    };

    std::string filename = "./html" + http_request->get_path();
    boost::filesystem::path path(filename);
    if (boost::filesystem::is_regular_file(path)) {
        std::cout << filename << " -> send file" << std::endl;
        //try open the desired file
        std::ifstream file(filename, std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
        if (file.is_open()) {
            http_response->set_version("HTTP/1.1");
            http_response->set_status_code("200");
            http_response->set_reason("OK");
            http_response->set_field("connection", "close");

            auto it = _mime_types.find(http_request->get_extension());
            if (it != _mime_types.end()) {
                http_response->set_field("content-type", it->second);
            }
            http_response->set_field("cache-control", STATIC_EXTENSIONS.find(http_request->get_extension()) != STATIC_EXTENSIONS.end() ? "max-age=2592000" : "private");

            http_response->set_field("content-length", std::to_string(file.tellg()));
            http_response->is_parsed(true);

            if (!skip_body) {
                file.seekg(std::ifstream::beg);
                http_response->getRawStream()->append_raw_data(file.rdbuf());
            }
            http_response->getRawStream()->is_completed(true);
        }
    } else if (boost::filesystem::is_directory(path)) {
        if (*http_request->get_path().rbegin() != '/') {
            std::cout << filename << " -> send redirection" << std::endl;
            http_response->set_version("HTTP/1.1");
            http_response->set_status_code("302");
            http_response->set_reason("Found");
            http_response->set_field("location", http_request->get_path() + "/");
            http_response->set_field("connection", "close");
            http_response->is_parsed(true);
            http_response->getRawStream()->is_completed(true);
        } else {
            std::ifstream file(filename + "/index.html", std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
            if (file.is_open()) {
                std::cout << filename << " -> send default" << std::endl;
                http_response->set_version("HTTP/1.1");
                http_response->set_status_code("200");
                http_response->set_reason("OK");
                http_response->set_field("connection", "close");
                http_response->set_field("content-type", "text/html; charset=utf-8");
                http_response->set_field("content-length", std::to_string(file.tellg()));
                http_response->is_parsed(true);

                if (!skip_body) {
                    file.seekg(std::ifstream::beg);
                    http_response->getRawStream()->append_raw_data(file.rdbuf());
                }
                http_response->getRawStream()->is_completed(true);
            } else {
                std::cout << filename << " -> send listing" << std::endl;
                std::string body = "<html><head></head><body><h1>In directory " + http_request->get_path().substr(1) + ":</h1><br/><ul>";
                boost::filesystem::directory_iterator end;
                for (auto it = boost::filesystem::directory_iterator(path); it != end; ++it) {
                    std::string url = it->path().filename().string();
                    if (boost::filesystem::is_directory(*it)) {
                        url += '/';
                    }
                    body.append("<li><a href=\"").append(http_request->get_path()).append(url).append("\">").append(url).append("</a></li><br/>");
                }
                body += "</ul></body></html>";
                http_response->set_version("HTTP/1.1");
                http_response->set_status_code("200");
                http_response->set_reason("OK");
                http_response->set_field("connection", "close");
                http_response->set_field("content-type", "text/html; charset=utf-8");
                if (!skip_body) {
                    http_response->getRawStream()->append_raw_data(body);
                }
                http_response->set_field("content-length", std::to_string(body.size()));
                http_response->is_parsed(true);
                http_response->getRawStream()->is_completed(true);
            }
        }
    } else {
        std::ifstream file("./html/404.html", std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
        if (file.is_open()) {
            std::cout << filename << " -> send personalized 404 message" << std::endl;
            http_response->set_version("HTTP/1.1");
            http_response->set_status_code("404");
            http_response->set_reason("Not Found");
            http_response->set_field("connection", "close");
            http_response->set_field("content-type", "text/html; charset=utf-8");
            http_response->set_field("content-length", std::to_string(file.tellg()));
            http_response->is_parsed(true);

            if(!skip_body) {
                file.seekg(std::ifstream::beg);
                http_response->getRawStream()->append_raw_data(file.rdbuf());
            }
            http_response->getRawStream()->is_completed(true);
        } else {
            std::cout << filename << " -> send default 404 message" << std::endl;
            std::string body = "404 Not Found";
            http_response->set_version("HTTP/1.1");
            http_response->set_status_code("404");
            http_response->set_reason("Not Found");
            http_response->set_field("connection", "close");
            http_response->set_field("content-type", "text/plain; charset=utf-8");
            if(!skip_body) {
                http_response->getRawStream()->append_raw_data(body);
            }
            http_response->set_field("content-length", std::to_string(body.size()));
            http_response->is_parsed(true);
            http_response->getRawStream()->is_completed(true);
        }
    }
    return http_response;
}